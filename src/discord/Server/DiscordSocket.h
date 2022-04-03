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

#ifndef __WORLDSOCKET_H__
#define __WORLDSOCKET_H__

#include "AuthCrypt.h"
#include "Define.h"
#include "MPSCQueue.h"
#include "Socket.h"
#include "Util.h"
#include "DiscordPacket.h"
#include "DiscordSession.h"
#include <boost/asio/ip/tcp.hpp>

using boost::asio::ip::tcp;

namespace DiscordPackets
{
    class ServerPacket;
}

struct AuthSession;
enum class DiscordAuthResponseCodes : uint8;

class WH_DISCORD_API DiscordSocket : public Socket<DiscordSocket>
{
    typedef Socket<DiscordSocket> BaseSocket;

public:
    DiscordSocket(tcp::socket&& socket);
    ~DiscordSocket();

    DiscordSocket(DiscordSocket const& right) = delete;
    DiscordSocket& operator=(DiscordSocket const& right) = delete;

    void Start() override;
    bool Update() override;

    void SendPacket(DiscordPacket const& packet);

    void SetSendBufferSize(std::size_t sendBufferSize) { _sendBufferSize = sendBufferSize; }

protected:
    void OnClose() override;
    void ReadHandler() override;
    bool ReadHeaderHandler();

    enum class ReadDataHandlerResult
    {
        Ok = 0,
        Error = 1,
        WaitingForQuery = 2
    };

    ReadDataHandlerResult ReadDataHandler();

private:
    void CheckIpCallback(PreparedQueryResult result);

    /// writes network.opcode log
    /// accessing DiscordSession is not threadsafe, only do it when holding _worldSessionLock
    void LogOpcodeText(OpcodeClient opcode) const;

    /// sends and logs network.opcode without accessing DiscordSession
    void SendPacketAndLogOpcode(DiscordPacket const& packet);
    //void HandleSendAuthSession();
    void HandleAuthSession(DiscordPacket& recvPacket);
    void HandleAuthSessionCallback(std::shared_ptr<AuthSession> authSession, PreparedQueryResult result);
    //void LoadSessionPermissionsCallback(PreparedQueryResult result);
    void SendAuthResponseError(DiscordAuthResponseCodes code);

    //bool HandlePing(DiscordPacket& recvPacket);

    TimePoint _LastPingTime;
    uint32 _OverSpeedPings;

    std::mutex _worldSessionLock;
    DiscordSession* _worldSession;
    bool _authed;

    MessageBuffer _headerBuffer;
    MessageBuffer _packetBuffer;
    MPSCQueue<DiscordPacket> _bufferQueue;
    std::size_t _sendBufferSize;

    QueryCallbackProcessor _queryProcessor;
    std::string _ipCountry;
};

#endif
