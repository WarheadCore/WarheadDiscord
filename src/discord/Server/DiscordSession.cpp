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

#include "DiscordSession.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "Opcodes.h"
#include "PacketUtilities.h"
#include "QueryHolder.h"
#include "DiscordPacket.h"
#include "DiscordSocket.h"
#include "Timer.h"

/// DiscordSession constructor
DiscordSession::DiscordSession(uint32 id, std::string&& name, std::shared_ptr<DiscordSocket> sock) :
    _accountId(id),
    _accountName(std::move(name)),
    _socket(sock),
    _latency(0)
{
    if (_socket)
        _address = _socket->GetRemoteIpAddress().to_string();
}

/// DiscordSession destructor
DiscordSession::~DiscordSession()
{
    /// - If have unclosed socket, close it
    if (_socket)
    {
        _socket->CloseSocket();
        _socket = nullptr;
    }
}

/// Send a packet to the client
void DiscordSession::SendPacket(DiscordPacket const* packet)
{
    if (packet->GetOpcode() == NULL_OPCODE)
    {
        LOG_ERROR("network.opcode", "Send NULL_OPCODE");
        return;
    }

    if (!_socket)
        return;

    LOG_TRACE("network.opcode", "S->C: {}", GetOpcodeNameForLogging(static_cast<OpcodeServer>(packet->GetOpcode())));
    _socket->SendPacket(*packet);
}

/// Add an incoming packet to the queue
void DiscordSession::QueuePacket(DiscordPacket const& packet)
{
    _recvQueue.AddPacket(new DiscordPacket(packet));
}

/// Logging helper for unexpected opcodes
void DiscordSession::LogUnexpectedOpcode(DiscordPacket* packet, char const* status, const char* reason)
{
    LOG_ERROR("network.opcode", "Received unexpected opcode {} Status: {} Reason: {}",
        GetOpcodeNameForLogging(static_cast<OpcodeClient>(packet->GetOpcode())), status, reason);
}

/// Logging helper for unexpected opcodes
void DiscordSession::LogUnprocessedTail(DiscordPacket* packet)
{
    if (!sLog->ShouldLog("network.opcode", LogLevel::LOG_LEVEL_TRACE) || packet->rpos() >= packet->wpos())
        return;

    LOG_TRACE("network.opcode", "Unprocessed tail data (read stop at {} from {}) Opcode {}",
        uint32(packet->rpos()), uint32(packet->wpos()), GetOpcodeNameForLogging(static_cast<OpcodeClient>(packet->GetOpcode())));

    packet->print_storage();
}

/// Update the DiscordSession (triggered by Discord update)
bool DiscordSession::Update()
{
    DiscordPacket* packet{ nullptr };
    uint32 processedPackets = 0;
    time_t currentTime = GetEpochTime().count();

    while (_socket && _recvQueue.GetNextPacket(packet))
    {
        OpcodeClient opcode = static_cast<OpcodeClient>(packet->GetOpcode());
        ClientOpcodeHandler const* opHandle = opcodeTable[opcode];

        try
        {
            opHandle->Call(this, *packet);
            LogUnprocessedTail(packet);
        }
        catch (DiscordPackets::PacketArrayMaxCapacityException const& pamce)
        {
            LOG_ERROR("network", "PacketArrayMaxCapacityException: {} while parsing {}", pamce.what(), GetOpcodeNameForLogging(static_cast<OpcodeClient>(packet->GetOpcode())));
        }
        catch (ByteBufferException const&)
        {
            LOG_ERROR("network", "DiscordSession::Update ByteBufferException occured while parsing a packet (opcode: {}) from client {}, accountid={}. Skipped packet.", packet->GetOpcode(), GetRemoteAddress(), GetAccountId());
            if (sLog->ShouldLog("network", LogLevel::LOG_LEVEL_DEBUG))
            {
                LOG_DEBUG("network", "Dumping error causing packet:");
                packet->hexlike();
            }
        }

        delete packet;

        processedPackets++;

//#define MAX_PROCESSED_PACKETS_IN_SAME_WORLDSESSION_UPDATE 150
//        processedPackets++;
//
//        // process only a max amout of packets in 1 Update() call.
//        // Any leftover will be processed in next update
//        if (processedPackets > MAX_PROCESSED_PACKETS_IN_SAME_WORLDSESSION_UPDATE)
//            break;
    }

    if (processedPackets)
        LOG_DEBUG("server", "{}: {} packages processed", __FUNCTION__, processedPackets);

    ProcessQueryCallbacks();

    return true;
}

bool DiscordSession::HandleSocketClosed()
{
    if (_socket && !_socket->IsOpen() /*world stop*/)
    {
        _socket = nullptr;
        return true;
    }

    return false;
}

bool DiscordSession::IsSocketClosed() const
{
    return !_socket || !_socket->IsOpen();
}

/// Kick a player out of the Discord
void DiscordSession::KickSession(std::string_view reason, bool setKicked)
{
    if (_socket)
    {
        LOG_INFO("network.kick", "Account: {} kicked with reason: {}", GetAccountId(), reason);
        _socket->CloseSocket();
    }

    if (setKicked)
        SetKicked(true);
}

void DiscordSession::Handle_NULL(DiscordPacket& null)
{
    LOG_ERROR("network.opcode", "Received unhandled opcode {}",
        GetOpcodeNameForLogging(static_cast<OpcodeClient>(null.GetOpcode())));
}

void DiscordSession::Handle_EarlyProccess(DiscordPacket& recvPacket)
{
    LOG_ERROR("network.opcode", "Received opcode {} that must be processed in DiscordSocket::ReadDataHandler",
        GetOpcodeNameForLogging(static_cast<OpcodeClient>(recvPacket.GetOpcode())));
}

void DiscordSession::Handle_ServerSide(DiscordPacket& recvPacket)
{
    LOG_ERROR("network.opcode", "Received server-side opcode {}",
        GetOpcodeNameForLogging(static_cast<OpcodeServer>(recvPacket.GetOpcode())));
}

void DiscordSession::ProcessQueryCallbacks()
{
    _queryProcessor.ProcessReadyCallbacks();
    _transactionCallbacks.ProcessReadyCallbacks();
    _queryHolderProcessor.ProcessReadyCallbacks();
}

TransactionCallback& DiscordSession::AddTransactionCallback(TransactionCallback&& callback)
{
    return _transactionCallbacks.AddCallback(std::move(callback));
}

SQLQueryHolderCallback& DiscordSession::AddQueryHolderCallback(SQLQueryHolderCallback&& callback)
{
    return _queryHolderProcessor.AddCallback(std::move(callback));
}
