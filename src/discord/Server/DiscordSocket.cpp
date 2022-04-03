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

#include "DiscordSocket.h"
#include "Config.h"
#include "CryptoHash.h"
#include "CryptoRandom.h"
#include "DatabaseEnv.h"
#include "IPLocation.h"
#include "Opcodes.h"
#include "DiscordSession.h"
#include "DiscordSharedDefines.h"
#include "Discord.h"
#include "DiscordPacketHeader.h"

using boost::asio::ip::tcp;

DiscordSocket::DiscordSocket(tcp::socket&& socket)
    : Socket(std::move(socket)), _OverSpeedPings(0), _worldSession(nullptr), _authed(false), _sendBufferSize(4096)
{
    //Warhead::Crypto::GetRandomBytes(_authSeed);
    _headerBuffer.Resize(sizeof(DiscordClientPktHeader));
}

DiscordSocket::~DiscordSocket() = default;

void DiscordSocket::Start()
{
    DiscordDatabasePreparedStatement* stmt = DiscordDatabase.GetPreparedStatement(DISCORD_SEL_IP_INFO);
    stmt->SetArguments(GetRemoteIpAddress().to_string());

    _queryProcessor.AddCallback(DiscordDatabase.AsyncQuery(stmt).WithPreparedCallback(std::bind(&DiscordSocket::CheckIpCallback, this, std::placeholders::_1)));

    LOG_INFO("server", "> Connect from {}", GetRemoteIpAddress().to_string());
}

void DiscordSocket::CheckIpCallback(PreparedQueryResult result)
{
    if (result)
    {
        bool banned = false;
        do
        {
            Field* fields = result->Fetch();
            if (fields[0].Get<uint64>() != 0)
                banned = true;

        } while (result->NextRow());

        if (banned)
        {
            SendAuthResponseError(DiscordAuthResponseCodes::Banned);
            LOG_ERROR("network", "DiscordSocket::CheckIpCallback: Sent Auth Response (IP {} banned).", GetRemoteIpAddress().to_string());
            DelayedCloseSocket();
            return;
        }
    }

    AsyncRead();
}

bool DiscordSocket::Update()
{
    DiscordPacket* queued{ nullptr };
    MessageBuffer buffer(_sendBufferSize);

    while (_bufferQueue.Dequeue(queued))
    {
        DiscordServerPktHeader header(queued->size() + sizeof(queued->GetOpcode()), queued->GetOpcode());
        if (buffer.GetRemainingSpace() < queued->size() + header.GetHeaderLength())
        {
            QueuePacket(std::move(buffer));
            //buffer.Resize(_sendBufferSize);
        }

        if (buffer.GetRemainingSpace() >= queued->size() + header.GetHeaderLength())
        {
            buffer.Write(header.header, header.GetHeaderLength());
            if (!queued->empty())
                buffer.Write(queued->contents(), queued->size());
        }
        else // single packet larger than 4096 bytes
        {
            MessageBuffer packetBuffer(queued->size() + header.GetHeaderLength());
            packetBuffer.Write(header.header, header.GetHeaderLength());
            if (!queued->empty())
                packetBuffer.Write(queued->contents(), queued->size());

            QueuePacket(std::move(packetBuffer));
        }

        delete queued;
    }

    if (buffer.GetActiveSize() > 0)
        QueuePacket(std::move(buffer));

    if (!BaseSocket::Update())
        return false;

    _queryProcessor.ProcessReadyCallbacks();

    return true;
}

void DiscordSocket::OnClose()
{
    {
        std::lock_guard<std::mutex> sessionGuard(_worldSessionLock);
        _worldSession = nullptr;
        LOG_INFO("server", "> Disconnect from {}", GetRemoteIpAddress().to_string());
    }
}

void DiscordSocket::ReadHandler()
{
    if (!IsOpen())
        return;

    MessageBuffer& packet = GetReadBuffer();

    while (packet.GetActiveSize() > 0)
    {
        if (_headerBuffer.GetRemainingSpace() > 0)
        {
            // need to receive the header
            std::size_t readHeaderSize = std::min(packet.GetActiveSize(), _headerBuffer.GetRemainingSpace());
            _headerBuffer.Write(packet.GetReadPointer(), readHeaderSize);
            packet.ReadCompleted(readHeaderSize);

            if (_headerBuffer.GetRemainingSpace() > 0)
            {
                // Couldn't receive the whole header this time.
                ASSERT(packet.GetActiveSize() == 0);
                break;
            }

            // We just received nice new header
            if (!ReadHeaderHandler())
            {
                CloseSocket();
                return;
            }
        }

        // We have full read header, now check the data payload
        if (_packetBuffer.GetRemainingSpace() > 0)
        {
            // need more data in the payload
            std::size_t readDataSize = std::min(packet.GetActiveSize(), _packetBuffer.GetRemainingSpace());
            _packetBuffer.Write(packet.GetReadPointer(), readDataSize);
            packet.ReadCompleted(readDataSize);

            if (_packetBuffer.GetRemainingSpace() > 0)
            {
                // Couldn't receive the whole data this time.
                ASSERT(packet.GetActiveSize() == 0);
                break;
            }
        }

        // just received fresh new payload
        ReadDataHandlerResult result = ReadDataHandler();
        _headerBuffer.Reset();
        if (result != ReadDataHandlerResult::Ok)
        {
            if (result != ReadDataHandlerResult::WaitingForQuery)
                CloseSocket();

            return;
        }
    }

    AsyncRead();
}

bool DiscordSocket::ReadHeaderHandler()
{
    ASSERT(_headerBuffer.GetActiveSize() == sizeof(DiscordClientPktHeader));

    DiscordClientPktHeader* header = reinterpret_cast<DiscordClientPktHeader*>(_headerBuffer.GetReadPointer());
    EndianConvertReverse(header->size);
    EndianConvert(header->cmd);

    if (!header->IsValidSize() || !header->IsValidOpcode())
    {
        OpcodeClient nodeCode = static_cast<OpcodeClient>(header->cmd);

        LOG_ERROR("network", "DiscordSocket::ReadHeaderHandler(): client {} sent malformed packet (size: {}, {})",
            GetRemoteIpAddress().to_string(), header->size, header->cmd, GetOpcodeNameForLogging(nodeCode));

        return false;
    }

    header->size -= sizeof(header->cmd);
    _packetBuffer.Resize(header->size);
    return true;
}

struct AuthSession
{
    uint32 ClientVersion = 0;
    Warhead::Crypto::SHA1::Digest Digest = {};
    std::string Account;
};

struct AccountInfo
{
    uint32 Id;

    explicit AccountInfo(Field* fields)
    {
        Id = fields[0].Get<uint32>();
    }
};

DiscordSocket::ReadDataHandlerResult DiscordSocket::ReadDataHandler()
{
    DiscordClientPktHeader* header = reinterpret_cast<DiscordClientPktHeader*>(_headerBuffer.GetReadPointer());
    OpcodeClient opcode = static_cast<OpcodeClient>(header->cmd);

    DiscordPacket packet(opcode, std::move(_packetBuffer));
    DiscordPacket* packetToQueue;

    std::unique_lock<std::mutex> sessionGuard(_worldSessionLock, std::defer_lock);

    switch (opcode)
    {
        case CLIENT_AUTH_SESSION:
        {
            LogOpcodeText(opcode);
            if (_authed)
            {
                // locking just to safely log offending user is probably overkill but we are disconnecting him anyway
                if (sessionGuard.try_lock())
                    LOG_ERROR("network", "DiscordSocket::ProcessIncoming: received duplicate CMSG_AUTH_SESSION from {}", _worldSession->GetRemoteAddress());

                return ReadDataHandlerResult::Error;
            }

            try
            {
                HandleAuthSession(packet);
                return ReadDataHandlerResult::WaitingForQuery;
            }
            catch (ByteBufferException const&)
            {

            }

            LOG_ERROR("network", "DiscordSocket::ReadDataHandler(): client {} sent malformed CMSG_AUTH_SESSION", GetRemoteIpAddress().to_string());
            return ReadDataHandlerResult::Error;
        }
        default:
            packetToQueue = new DiscordPacket(std::move(packet));
            break;
    }

    sessionGuard.lock();

    LogOpcodeText(opcode);

    if (!_worldSession)
    {
        LOG_ERROR("network.opcode", "ProcessIncoming: Client not authed opcode = {}", uint32(opcode));
        delete packetToQueue;
        return ReadDataHandlerResult::Error;
    }

    OpcodeHandler const* handler = opcodeTable[opcode];
    if (!handler)
    {
        LOG_ERROR("network.opcode", "No defined handler for opcode {} sent by {}", GetOpcodeNameForLogging(static_cast<OpcodeClient>(packetToQueue->GetOpcode())), _worldSession->GetRemoteAddress());
        delete packetToQueue;
        return ReadDataHandlerResult::Error;
    }

    // Copy the packet to the heap before enqueuing
    _worldSession->QueuePacket(*packetToQueue);

    return ReadDataHandlerResult::Ok;
}

void DiscordSocket::LogOpcodeText(OpcodeClient opcode) const
{
    LOG_TRACE("network.opcode", "C->S: {} {}", GetRemoteIpAddress().to_string(), GetOpcodeNameForLogging(opcode));
}

void DiscordSocket::SendPacketAndLogOpcode(DiscordPacket const& packet)
{
    LOG_TRACE("network.opcode", "S->C: {} {}", GetRemoteIpAddress().to_string(), GetOpcodeNameForLogging(static_cast<OpcodeServer>(packet.GetOpcode())));
    SendPacket(packet);
}

void DiscordSocket::SendPacket(DiscordPacket const& packet)
{
    if (!IsOpen())
        return;

    _bufferQueue.Enqueue(new DiscordPacket(packet));
}

void DiscordSocket::HandleAuthSession(DiscordPacket& recvPacket)
{
    std::shared_ptr<AuthSession> authSession = std::make_shared<AuthSession>();

    // Read the content of the packet
    recvPacket >> authSession->Account;
    //recvPacket >> authSession->ClientVersion;
    //recvPacket >> authSession->Digest;

    // Get the account information from the auth database
    auto stmt = DiscordDatabase.GetPreparedStatement(DISCORD_SEL_ACCOUNT_INFO_BY_NAME);
    stmt->SetArguments(authSession->Account);

    _queryProcessor.AddCallback(DiscordDatabase.AsyncQuery(stmt).WithPreparedCallback(std::bind(&DiscordSocket::HandleAuthSessionCallback, this, authSession, std::placeholders::_1)));
}

void DiscordSocket::HandleAuthSessionCallback(std::shared_ptr<AuthSession> authSession, PreparedQueryResult result)
{
    // Stop if the account is not found
    if (!result)
    {
        // We can not log here, as we do not know the account. Thus, no accountId.
        SendAuthResponseError(DiscordAuthResponseCodes::UnknownAccount);
        LOG_ERROR("network", "DiscordSocket::HandleAuthSession: Sent Auth Response (unknown account).");
        DelayedCloseSocket();
        return;
    }

    AccountInfo account(result->Fetch());

    // For hook purposes, we get Remoteaddress at this point.
    std::string address = GetRemoteIpAddress().to_string();

    // First reject the connection if packet contains invalid data or realm state doesn't allow logging in
    if (sDiscord->IsClosed())
    {
        SendAuthResponseError(DiscordAuthResponseCodes::ServerOffline);
        LOG_ERROR("network", "DiscordSocket::HandleAuthSession: Discord closed, denying client ({}).", GetRemoteIpAddress().to_string());
        DelayedCloseSocket();
        return;
    }

    if (IpLocationRecord const* location = sIPLocation->GetLocationRecord(address))
        _ipCountry = location->CountryCode;

    /*if (account.IsBanned)
    {
        SendAuthResponseError(AUTH_BANNED);
        LOG_ERROR("network", "DiscordSocket::HandleAuthSession: Sent Auth Response (Account banned).");
        sScriptMgr->OnFailedAccountLogin(account.Id);
        DelayedCloseSocket();
        return;
    }*/

    LOG_DEBUG("network", "DiscordSocket::HandleAuthSession: Client '{}' authenticated successfully from {}.", authSession->Account, address);

    _authed = true;

    _worldSession = new DiscordSession(account.Id, std::move(authSession->Account), shared_from_this());

    sDiscord->AddSession(_worldSession);

    AsyncRead();
}

void DiscordSocket::SendAuthResponseError(DiscordAuthResponseCodes code)
{
    DiscordPacket packet(SERVER_SEND_AUTH_RESPONSE, 1);
    packet << uint8(code);

    SendPacketAndLogOpcode(packet);
}

//bool DiscordSocket::HandlePing(DiscordPacket& recvPacket)
//{
//    using namespace std::chrono;
//
//    uint32 ping;
//    uint32 latency;
//
//    // Get the ping packet content
//    recvPacket >> ping;
//    recvPacket >> latency;
//
//    if (_LastPingTime == TimePoint())
//    {
//        _LastPingTime = steady_clock::now();
//    }
//    else
//    {
//        TimePoint now = steady_clock::now();
//
//        steady_clock::duration diff = now - _LastPingTime;
//
//        _LastPingTime = now;
//
//        if (diff < 27s)
//        {
//            ++_OverSpeedPings;
//
//            uint32 maxAllowed = CONF_GET_INT("MaxOverspeedPings");
//
//            if (maxAllowed && _OverSpeedPings > maxAllowed)
//            {
//                std::unique_lock<std::mutex> sessionGuard(_worldSessionLock);
//
//                if (_worldSession && AccountMgr::IsPlayerAccount(_worldSession->GetSecurity()))
//                {
//                    LOG_ERROR("network", "DiscordSocket::HandlePing: {} kicked for over-speed pings (address: {})",
//                        _worldSession->GetPlayerInfo(), GetRemoteIpAddress().to_string());
//
//                    return false;
//                }
//            }
//        }
//        else
//            _OverSpeedPings = 0;
//    }
//
//    {
//        std::lock_guard<std::mutex> sessionGuard(_worldSessionLock);
//
//        if (_worldSession)
//            _worldSession->SetLatency(latency);
//        else
//        {
//            LOG_ERROR("network", "DiscordSocket::HandlePing: peer sent CMSG_PING, but is not authenticated or got recently kicked, address = {}", GetRemoteIpAddress().to_string());
//            return false;
//        }
//    }
//
//    DiscordPacket packet(SMSG_PONG, 4);
//    packet << ping;
//    SendPacketAndLogOpcode(packet);
//
//    return true;
//}
