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

bool PacketFilter::Process(DiscordPacket* packet)
{
    ClientOpcodeHandler const* opHandle = opcodeTable[static_cast<OpcodeClient>(packet->GetOpcode())];

    if (!_session)
        return false;

    if (packet->GetOpcode() == NULL_OPCODE || packet->GetOpcode() >= NUM_OPCODE_HANDLERS)
        return false;

    return true;
}

/// DiscordSession constructor
DiscordSession::DiscordSession(uint32 id, std::string&& name, std::shared_ptr<DiscordSocket> sock) :
    _accountId(id),
    _accountName(std::move(name)),
    _socket(sock),
    m_latency(0)
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

    ///- empty incoming packet queue
    DiscordPacket* packet = nullptr;
    while (_recvQueue.next(packet))
        delete packet;
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

#if defined(WARHEAD_DEBUG)
    // Code for network use statistic
    static uint64 sendPacketCount = 0;
    static uint64 sendPacketBytes = 0;

    static time_t firstTime = GetEpochTime().count();
    static time_t lastTime = firstTime;                     // next 60 secs start time

    static uint64 sendLastPacketCount = 0;
    static uint64 sendLastPacketBytes = 0;

    time_t cur_time = GetEpochTime().count();

    if ((cur_time - lastTime) < 60)
    {
        sendPacketCount += 1;
        sendPacketBytes += packet->size();

        sendLastPacketCount += 1;
        sendLastPacketBytes += packet->size();
    }
    else
    {
        uint64 minTime = uint64(cur_time - lastTime);
        uint64 fullTime = uint64(lastTime - firstTime);

        LOG_DEBUG("network", "Send all time packets count: {} bytes: {} avr.count/sec: {} avr.bytes/sec: {} time: {}", sendPacketCount, sendPacketBytes, float(sendPacketCount) / fullTime, float(sendPacketBytes) / fullTime, uint32(fullTime));
        LOG_DEBUG("network", "Send last min packets count: {} bytes: {} avr.count/sec: {} avr.bytes/sec: {}", sendLastPacketCount, sendLastPacketBytes, float(sendLastPacketCount) / minTime, float(sendLastPacketBytes) / minTime);

        lastTime = cur_time;
        sendLastPacketCount = 1;
        sendLastPacketBytes = packet->wpos();               // wpos is real written size
    }
#endif // !WARHEAD_DEBUG

    LOG_TRACE("network.opcode", "S->C: {}", GetOpcodeNameForLogging(static_cast<OpcodeServer>(packet->GetOpcode())));
    _socket->SendPacket(*packet);
}

/// Add an incoming packet to the queue
void DiscordSession::QueuePacket(DiscordPacket* new_packet)
{
    _recvQueue.add(new_packet);
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
bool DiscordSession::Update(uint32 diff, PacketFilter& updater)
{
    ///- Retrieve packets from the receive queue and call the appropriate handlers
    /// not process packets if socket already closed
    DiscordPacket* packet = nullptr;

    //! Delete packet after processing by default
    bool deletePacket = true;
    std::vector<DiscordPacket*> requeuePackets;
    uint32 processedPackets = 0;
    time_t currentTime = GetEpochTime().count();

    while (_socket && _recvQueue.next(packet, updater))
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

        if (deletePacket)
            delete packet;

        deletePacket = true;

#define MAX_PROCESSED_PACKETS_IN_SAME_WORLDSESSION_UPDATE 150
        processedPackets++;

        // process only a max amout of packets in 1 Update() call.
        // Any leftover will be processed in next update
        if (processedPackets > MAX_PROCESSED_PACKETS_IN_SAME_WORLDSESSION_UPDATE)
            break;
    }

    _recvQueue.readd(requeuePackets.begin(), requeuePackets.end());

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
