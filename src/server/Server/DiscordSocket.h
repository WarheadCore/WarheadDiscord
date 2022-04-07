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

#ifndef _DISCORD_SOCKET_H_
#define _DISCORD_SOCKET_H_

#include "Define.h"
#include "DiscordPacket.h"
#include "DiscordSession.h"
#include "MPSCQueue.h"
#include "Opcodes.h"
#include "Socket.h"
#include <boost/asio/ip/tcp.hpp>

using boost::asio::ip::tcp;

struct AuthSession;
enum class DiscordAuthResponseCodes : uint8;

class WH_SERVER_API DiscordSocket : public Socket<DiscordSocket>
{
    typedef Socket<DiscordSocket> BaseSocket;

public:
    DiscordSocket(tcp::socket&& socket);
    ~DiscordSocket() = default;

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
    void LogOpcodeText(OpcodeClient opcode) const;
    void SendPacketAndLogOpcode(DiscordPacket const& packet);
    void HandleAuthSession(DiscordPacket& recvPacket);
    void HandleAuthSessionCallback(std::shared_ptr<AuthSession> authSession, PreparedQueryResult result);
    void SendAuthResponseError(DiscordAuthResponseCodes code);

    bool HandlePing(DiscordPacket& recvPacket);

    TimePoint _LastPingTime;
    uint32 _OverSpeedPings;

    std::mutex _discordSessionLock;
    DiscordSession* _discordSession;
    bool _authed;

    MessageBuffer _headerBuffer;
    MessageBuffer _packetBuffer;
    MPSCQueue<DiscordPacket> _bufferQueue;
    std::size_t _sendBufferSize;

    QueryCallbackProcessor _queryProcessor;
    std::string _ipCountry;
};

#endif
