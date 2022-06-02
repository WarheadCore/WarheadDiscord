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
#include "DatabaseEnv.h"
#include "DatabaseLoader.h"
#include "Discord.h"
#include "DiscordConfig.h"
#include "DiscordSocket.h"
#include "DiscordSocketMgr.h"
#include "GitRevision.h"
#include "IPLocation.h"
#include "Log.h"
#include "Logo.h"
#include "MySQLThreading.h"
#include "OpenSSLCrypto.h"
#include "StopWatch.h"
#include <boost/asio/signal_set.hpp>
#include <boost/version.hpp>
#include <openssl/crypto.h>
#include <openssl/opensslv.h>

#include "DiscordBot.h"

#ifndef _WARHEAD_DISCORD_CONFIG
#define _WARHEAD_DISCORD_CONFIG "WarheadDiscord.conf"
#endif

void TestFunction();

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

    TestFunction();

    LOG_INFO("server", "Halting process...");

    // 0 - normal shutdown
    // 1 - shutdown at error
    // 2 - restart command used, this code can be used by restarter for restart Warheadd

    return 0;
}

void TestFunction()
{
    sDiscordBot->Start();
    sDiscordBot->Test();
}
