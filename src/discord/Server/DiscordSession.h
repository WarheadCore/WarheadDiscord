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

#include "Define.h"
#include "AsyncCallbackProcessor.h"
#include "DatabaseEnvFwd.h"
#include "DiscordSharedDefines.h"
#include "LockedQueue.h"
#include "Packet.h"
#include <map>
#include <utility>

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

class WH_DISCORD_API PacketFilter
{
public:
    explicit PacketFilter(DiscordSession* session) : _session(session) {}
    ~PacketFilter() = default;

    bool Process(DiscordPacket* packet);

protected:
    DiscordSession* const _session;
};

/// Player session in the Discord
class WH_DISCORD_API DiscordSession
{
public:
    DiscordSession(uint32 id, std::string&& name, std::shared_ptr<DiscordSocket> sock);
    ~DiscordSession();

    void SendPacket(DiscordPacket const* packet);

    uint32 GetAccountId() const { return _accountId; }
    std::string const& GetRemoteAddress() { return _address; }

    void QueuePacket(DiscordPacket* new_packet);
    bool Update(uint32 diff, PacketFilter& updater);

    void KickSession(bool setKicked = true) { return KickSession("Unknown reason", setKicked); }
    void KickSession(std::string_view reason, bool setKicked = true);
    void SetKicked(bool val) { _kicked = val; }

    uint32 GetLatency() const { return m_latency; }
    void SetLatency(uint32 latency) { m_latency = latency; }

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

private:
    // logging helper
    void LogUnexpectedOpcode(DiscordPacket* packet, char const* status, const char* reason);
    void LogUnprocessedTail(DiscordPacket* packet);

    std::shared_ptr<DiscordSocket> _socket;
    std::string _address;
    uint32 _accountId;
    std::string _accountName;
    std::atomic<uint32> m_latency;
    bool _kicked{ false };
    LockedQueue<DiscordPacket*> _recvQueue;

    DiscordSession(DiscordSession const& right) = delete;
    DiscordSession& operator=(DiscordSession const& right) = delete;
};

#endif
