/*
 * This file is part of the WarheadCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "DiscordSocketMgr.h"
#include "Config.h"
#include "NetworkThread.h"
#include "DiscordSocket.h"
#include <boost/system/error_code.hpp>

class DiscordSocketThread : public NetworkThread<DiscordSocket>
{
public:
    void SocketAdded(std::shared_ptr<DiscordSocket> sock) override
    {
        sock->SetSendBufferSize(sDiscordSocketMgr.GetApplicationSendBufferSize());
    }

    void SocketRemoved(std::shared_ptr<DiscordSocket> sock) override
    {

    }
};

DiscordSocketMgr::DiscordSocketMgr() :
    BaseSocketMgr(), _socketSystemSendBufferSize(-1), _socketApplicationSendBufferSize(65536), _tcpNoDelay(true)
{
}

DiscordSocketMgr& DiscordSocketMgr::Instance()
{
    static DiscordSocketMgr instance;
    return instance;
}

bool DiscordSocketMgr::StartDiscordNetwork(Warhead::Asio::IoContext& ioContext, std::string const& bindIp, uint16 port, int threadCount)
{
    _tcpNoDelay = sConfigMgr->GetOption<bool>("Network.TcpNodelay", true);

    int const max_connections = WARHEAD_MAX_LISTEN_CONNECTIONS;
    LOG_DEBUG("network", "Max allowed socket connections {}", max_connections);

    // -1 means use default
    _socketSystemSendBufferSize = sConfigMgr->GetOption<int32>("Network.OutKBuff", -1);
    _socketApplicationSendBufferSize = sConfigMgr->GetOption<int32>("Network.OutUBuff", 65536);

    if (_socketApplicationSendBufferSize <= 0)
    {
        LOG_ERROR("misc", "Network.OutUBuff is wrong in your config file");
        return false;
    }

    if (!BaseSocketMgr::StartNetwork(ioContext, bindIp, port, threadCount))
        return false;

    _acceptor->AsyncAcceptWithCallback<&DiscordSocketMgr::OnSocketAccept>();
    return true;
}

void DiscordSocketMgr::StopNetwork()
{
    BaseSocketMgr::StopNetwork();
}

void DiscordSocketMgr::OnSocketOpen(tcp::socket&& sock, uint32 threadIndex)
{
    // set some options here
    if (_socketSystemSendBufferSize >= 0)
    {
        boost::system::error_code err;
        sock.set_option(boost::asio::socket_base::send_buffer_size(_socketSystemSendBufferSize), err);

        if (err && err != boost::system::errc::not_supported)
        {
            LOG_ERROR("network", "DiscordSocketMgr::OnSocketOpen sock.set_option(boost::asio::socket_base::send_buffer_size) err = {}", err.message());
            return;
        }
    }

    // Set TCP_NODELAY.
    if (_tcpNoDelay)
    {
        boost::system::error_code err;
        sock.set_option(boost::asio::ip::tcp::no_delay(true), err);

        if (err)
        {
            LOG_ERROR("network", "DiscordSocketMgr::OnSocketOpen sock.set_option(boost::asio::ip::tcp::no_delay) err = {}", err.message());
            return;
        }
    }

    BaseSocketMgr::OnSocketOpen(std::forward<tcp::socket>(sock), threadIndex);
}

NetworkThread<DiscordSocket>* DiscordSocketMgr::CreateThreads() const
{
    return new DiscordSocketThread[GetNetworkThreadCount()];
}
