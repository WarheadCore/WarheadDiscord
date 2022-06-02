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

#ifndef __DISCORD_SESSION_H_
#define __DISCORD_SESSION_H_

#include "AsyncCallbackProcessor.h"
#include "DatabaseEnv.h"
#include "Define.h"
#include "DiscordSharedDefines.h"
#include "Duration.h"
#include "PacketQueue.h"

class DiscordPacket;
class DiscordSocket;

namespace DiscordPackets
{
    namespace Misc
    {
        class HelloClient;
    }

    namespace Auth
    {
        class AuthResponse;
    }

    namespace Message
    {
        class SendDiscordMessage;
        class SendDiscordEmbedMessage;
    }
}

/// Player session in the Discord
class WH_SERVER_API DiscordSession
{
public:
    DiscordSession(uint32 id, int64 guidID, std::string&& name, DiscordChannelsList&& channels, std::shared_ptr<DiscordSocket> sock);
    ~DiscordSession();

    void SendPacket(DiscordPacket const* packet);

    inline uint32 GetAccountId() const { return _accountId; }
    inline int64 GetGuildId() const { return _guildID; }
    inline std::string const& GetAccountName() const { return _accountName; }
    inline std::string const& GetRemoteAddress() { return _address; }

    void QueuePacket(DiscordPacket const& packet);
    bool Update();

    void KickSession(bool setKicked = true) { return KickSession("Unknown reason", setKicked); }
    void KickSession(std::string_view reason, bool setKicked = true);
    void SetKicked(bool val) { _kicked = val; }

    Microseconds GetLatency() const { return _latency; }
    void SetLatency(int64 latency) { _latency = Microseconds{ latency }; }

    // Packets

    // Misc
    void HandleHelloOpcode(DiscordPackets::Misc::HelloClient& packet);

    // Message
    void HandleSendDiscordMessageOpcode(DiscordPackets::Message::SendDiscordMessage& packet);
    void HandleSendDiscordEmbedMessageOpcode(DiscordPackets::Message::SendDiscordEmbedMessage& packet);

    // Auth
    void SendAuthResponse(DiscordAuthResponseCodes code);

public: // opcodes handlers
    void Handle_NULL(DiscordPacket& null); // not used
    void Handle_EarlyProccess(DiscordPacket& recvPacket); // just mark packets processed in DiscordSocket::OnRead
    void Handle_ServerSide(DiscordPacket& recvPacket); // sever side only, can't be accepted from client

    bool HandleSocketClosed();
    bool IsSocketClosed() const;

    /*
     * CALLBACKS
     */

    QueryCallbackProcessor& GetQueryProcessor() { return _queryProcessor; }
    TransactionCallback& AddTransactionCallback(TransactionCallback&& callback);
    SQLQueryHolderCallback& AddQueryHolderCallback(SQLQueryHolderCallback&& callback);

private:
    void ProcessQueryCallbacks();

    QueryCallbackProcessor _queryProcessor;
    AsyncCallbackProcessor<TransactionCallback> _transactionCallbacks;
    AsyncCallbackProcessor<SQLQueryHolderCallback> _queryHolderProcessor;

    int64 GetChannelID(uint8 channelType);

private:
    // logging helper
    void LogUnexpectedOpcode(DiscordPacket* packet, char const* status, const char* reason);
    void LogUnprocessedTail(DiscordPacket* packet);

    std::shared_ptr<DiscordSocket> _socket;
    std::string _address;
    uint32 _accountId;
    int64 _guildID;
    std::string _accountName;
    std::atomic<Microseconds> _latency;
    bool _kicked{ false };
    DiscordChannelsList _channels;
    PacketQueue<DiscordPacket> _recvQueue;

    DiscordSession(DiscordSession const& right) = delete;
    DiscordSession& operator=(DiscordSession const& right) = delete;
};

#endif
