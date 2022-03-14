/*
 * This file is part of the WarheadApp Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "Config.h"
#include "StopWatch.h"
#include "GitRevision.h"
#include "Log.h"
#include "Logo.h"
#include "MySQLThreading.h"
#include "IPLocation.h"
#include "DatabaseEnv.h"
#include "DatabaseLoader.h"
#include "OpenSSLCrypto.h"
#include "Discord.h"
#include "DiscordConfig.h"
#include "DiscordSocket.h"
#include "DiscordSocketMgr.h"
#include <boost/asio/signal_set.hpp>
#include <boost/version.hpp>
#include <openssl/crypto.h>
#include <openssl/opensslv.h>

#ifndef _WARHEAD_DISCORD_CONFIG
#define _WARHEAD_DISCORD_CONFIG "WarheadDiscord.conf"
#endif

bool StartDB();
void StopDB();
void SignalHandler(std::weak_ptr<Warhead::Asio::IoContext> ioContextRef, boost::system::error_code const& error, int signalNumber);
void DiscordUpdateLoop();

int main(int argc, char** argv)
{
    signal(SIGABRT, &Warhead::AbortHandler);

    // Command line parsing to get the configuration file name
    std::string configFile = sConfigMgr->GetConfigPath() + std::string(_WARHEAD_DISCORD_CONFIG);
    int count = 1;

    while (count < argc)
    {
        if (strcmp(argv[count], "-c") == 0)
        {
            if (++count >= argc)
            {
                printf("Runtime-Error: -c option requires an input argument\n");
                return 1;
            }
            else
                configFile = argv[count];
        }
        ++count;
    }

    if (!sConfigMgr->LoadAppConfigs(configFile))
        return 1;

    // Init logging
    sLog->Initialize();

    Warhead::Logo::Show("discordserver",
        [](std::string_view text)
        {
            LOG_INFO("server", text);
        },
        []()
        {
            LOG_INFO("server", "> Using configuration file:       {}", sConfigMgr->GetFilename());
            LOG_INFO("server", "> Using SSL version:              {} (library: {})", OPENSSL_VERSION_TEXT, SSLeay_version(SSLEAY_VERSION));
            LOG_INFO("server", "> Using Boost version:            {}.{}.{}", BOOST_VERSION / 100000, BOOST_VERSION / 100 % 1000, BOOST_VERSION % 100);
        }
    );

    OpenSSLCrypto::threadsSetup();

    std::shared_ptr<void> opensslHandle(nullptr, [](void*) { OpenSSLCrypto::threadsCleanup(); });

    std::shared_ptr<Warhead::Asio::IoContext> ioContext = std::make_shared<Warhead::Asio::IoContext>();

    // Set signal handlers
    boost::asio::signal_set signals(*ioContext, SIGINT, SIGTERM);
#if WARHEAD_PLATFORM == WARHEAD_PLATFORM_WINDOWS
    signals.add(SIGBREAK);
#endif
    signals.async_wait(std::bind(&SignalHandler, std::weak_ptr<Warhead::Asio::IoContext>(ioContext), std::placeholders::_1, std::placeholders::_2));

    // Start the Boost based thread pool
    int numThreads = sConfigMgr->GetOption<int32>("ThreadPool", 1);
    std::shared_ptr<std::vector<std::thread>> threadPool(new std::vector<std::thread>(), [ioContext](std::vector<std::thread>* del)
    {
        ioContext->stop();
        for (auto& thr : *del)
            thr.join();

        delete del;
    });

    if (numThreads < 1)
        numThreads = 1;

    for (int i = 0; i < numThreads; ++i)
    {
        threadPool->push_back(std::thread([ioContext]()
        {
            ioContext->run();
        }));
    }

    // Initialize the database connection
    if (!StartDB())
        return 1;

    std::shared_ptr<void> dbHandle(nullptr, [](void*) { StopDB(); });

    // Load IP Location Database
    sIPLocation->Load();

    // Init server
    sDiscord->SetInitialDiscordSettings();

    // Launch the worldserver listener socket
    uint16 worldPort = sDiscordConfig->GetOption<uint16>("ServerPort");
    std::string worldListener = sConfigMgr->GetOption<std::string>("BindIP", "0.0.0.0");

    int networkThreads = sConfigMgr->GetOption<int32>("Network.Threads", 1);
    if (networkThreads <= 0)
    {
        LOG_ERROR("server", "Network.Threads must be greater than 0");
        Discord::StopNow(ERROR_EXIT_CODE);
        return 1;
    }

    if (!sDiscordSocketMgr.StartDiscordNetwork(*ioContext, worldListener, worldPort, networkThreads))
    {
        LOG_ERROR("server", "Failed to initialize network");
        Discord::StopNow(ERROR_EXIT_CODE);
        return 1;
    }

    std::shared_ptr<void> sDiscordSocketMgrHandle(nullptr, [](void*)
    {
        sDiscord->KickAll();              // save and kick all players
        sDiscord->UpdateSessions(1);      // real players unload required UpdateSessions call

        sDiscordSocketMgr.StopNetwork();

        //sDiscord->SendServerShutdown();
    });

    LOG_INFO("server", "{} (discordserver-daemon) ready...", GitRevision::GetFullVersion());

    DiscordUpdateLoop();

    // Shutdown starts here
    threadPool.reset();

    LOG_INFO("server", "Halting process...");

    // 0 - normal shutdown
    // 1 - shutdown at error
    // 2 - restart command used, this code can be used by restarter for restart Warheadd

    return Discord::GetExitCode();
}

void DiscordUpdateLoop()
{
    uint32 realCurrTime = 0;
    uint32 realPrevTime = getMSTime();

    DiscordDatabase.WarnAboutSyncQueries(true);

    ///- While we have not Discord::m_stopEvent, update the world
    while (!Discord::IsStopped())
    {
        ++Discord::m_worldLoopCounter;
        realCurrTime = getMSTime();

        uint32 diff = getMSTimeDiff(realPrevTime, realCurrTime);
        if (!diff)
        {
            // sleep until enough time passes that we can update all timers
            std::this_thread::sleep_for(1ms);
            continue;
        }

        sDiscord->Update(diff);
        realPrevTime = realCurrTime;
    }

    DiscordDatabase.WarnAboutSyncQueries(false);
}

/// Initialize connection to the database
bool StartDB()
{
    MySQL::Library_Init();

    // Load databases
    // NOTE: While authserver is singlethreaded you should keep synch_threads == 1.
    // Increasing it is just silly since only 1 will be used ever.
    DatabaseLoader loader("server");
    loader
        .AddDatabase(DiscordDatabase, "Discord");

    if (!loader.Load())
        return false;

    LOG_INFO("server", "Started discord database connection pool.");
    return true;
}

/// Close the connection to the database
void StopDB()
{
    DiscordDatabase.Close();
    MySQL::Library_End();
}

void SignalHandler(std::weak_ptr<Warhead::Asio::IoContext> ioContextRef, boost::system::error_code const& error, int /*signalNumber*/)
{
    if (!error)
        Discord::StopNow(SHUTDOWN_EXIT_CODE);
}

