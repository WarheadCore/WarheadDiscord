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
#include "BanMgr.h"
#include "Config.h"
#include "CryptoHash.h"
#include "CryptoRandom.h"
#include "DatabaseEnv.h"
#include "Discord.h"
#include "DiscordBot.h"
#include "DiscordPacketHeader.h"
#include "DiscordSession.h"
#include "DiscordSharedDefines.h"
#include "GameTime.h"
#include "IPLocation.h"
#include "Opcodes.h"
#include "SRP6.h"
#include "SmartEnum.h"

using boost::asio::ip::tcp;

DiscordSocket::DiscordSocket(tcp::socket&& socket)
    : Socket(std::move(socket)), _OverSpeedPings(0), _discordSession(nullptr), _authed(false), _sendBufferSize(READ_BLOCK_SIZE)
{
    _headerBuffer.Resize(sizeof(DiscordClientPktHeader));
}

void DiscordSocket::Start()
{
    DiscordDatabasePreparedStatement* stmt = DiscordDatabase.GetPreparedStatement(DISCORD_SEL_IP_INFO);
    stmt->SetArguments(GetRemoteIpAddress().to_string());

    _queryProcessor.AddCallback(DiscordDatabase.AsyncQuery(stmt).WithPreparedCallback(std::bind(&DiscordSocket::CheckIpCallback, this, std::placeholders::_1)));

    LOG_DEBUG("network", "> Connect from {}", GetRemoteIpAddress().to_string());
}

void DiscordSocket::CheckIpCallback(PreparedQueryResult result)
{
    if (result)
    {
        do
        {
            auto const& [isBanned, isPermanentlyBanned] = result->FetchTuple<uint64, uint64>();

            if (isPermanentlyBanned)
            {
                SendAuthResponseError(DiscordAuthResponseCodes::BannedPermanentlyIP);
                LOG_ERROR("network", "DiscordSocket::CheckIpCallback: Sent Auth Response 'BannedPermanentlyIP' (IP {} permanently banned).", GetRemoteIpAddress().to_string());
                DelayedCloseSocket();
                return;
            }

            if (isBanned)
            {
                SendAuthResponseError(DiscordAuthResponseCodes::BannedIP);
                LOG_ERROR("network", "DiscordSocket::CheckIpCallback: Sent Auth Response 'BannedIP' (IP {} banned).", GetRemoteIpAddress().to_string());
                DelayedCloseSocket();
                return;
            }

        } while (result->NextRow());
    }

    AsyncRead();
}

bool DiscordSocket::Update()
{
    if (!BaseSocket::Update())
        return false;

    DiscordPacket* queued{ nullptr };
    MessageBuffer buffer(_sendBufferSize);

    while (_bufferQueue.Dequeue(queued))
    {
        DiscordServerPktHeader header(queued->size() + sizeof(queued->GetOpcode()), queued->GetOpcode());
        if (buffer.GetRemainingSpace() < queued->size() + header.GetHeaderLength())
        {
            MessageBuffer copyBuffer(buffer);
            QueuePacket(std::move(copyBuffer));
            buffer.Resize(_sendBufferSize);
        }

        if (buffer.GetRemainingSpace() >= queued->size() + header.GetHeaderLength())
        {
            buffer.Write(header.header, header.GetHeaderLength());
            if (!queued->empty())
                buffer.Write(queued->contents(), queued->size());
        }
        else // single packet larger than READ_BLOCK_SIZE bytes
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

    _queryProcessor.ProcessReadyCallbacks();

    return true;
}

void DiscordSocket::OnClose()
{
    {
        std::lock_guard<std::mutex> sessionGuard(_discordSessionLock);
        _discordSession = nullptr;
        LOG_DEBUG("network", "> Disconnect from {}", GetRemoteIpAddress().to_string());
    }
}

void DiscordSocket::ReadHandler()
{
    if (!IsOpen())
        return;

    MessageBuffer& packet = GetReadBuffer();

    while (packet.GetActiveSize() > 0)
    {
        if (_headerBuffer.HasRemainingSpace())
        {
            // need to receive the header
            std::size_t readHeaderSize = std::min(packet.GetActiveSize(), _headerBuffer.GetRemainingSpace());
            _headerBuffer.Write(packet.GetReadPointer(), readHeaderSize);
            packet.ReadCompleted(readHeaderSize);

            if (_headerBuffer.HasRemainingSpace())
            {
                // Couldn't receive the whole header this time.
                ASSERT(!packet.HasActiveSize());
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
        if (_packetBuffer.HasRemainingSpace())
        {
            // need more data in the payload
            std::size_t readDataSize = std::min(packet.GetActiveSize(), _packetBuffer.GetRemainingSpace());
            _packetBuffer.Write(packet.GetReadPointer(), readDataSize);
            packet.ReadCompleted(readDataSize);

            if (_packetBuffer.HasRemainingSpace())
            {
                // Couldn't receive the whole data this time.
                ASSERT(!packet.HasActiveSize());
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
    std::string Account;
    std::string Key;
    std::string CoreName;
    std::string CoreVersion;
    uint32 ModuleVersion{ 0 };
};

struct AccountInfo
{
    uint32 ID{ 0 };
    Warhead::Crypto::SRP6::Salt Salt{};
    Warhead::Crypto::SRP6::Verifier Verifier{};
    uint64 GuildID{ 0 };
    std::string RealmName;
    std::string CoreName;
    uint32 ModuleVersion{ 0 };
    std::string LastIP;
    Seconds BanDate{ 0s };
    Seconds UnBanDate{ 0s };
    bool IsBanned{ false };
    bool IsPermanentlyBanned{ false };
    std::unique_ptr<boost::asio::ip::address> RemoteIpAddress;

    //             0         1           2               3                4             5               6
    // SELECT `a`.`ID`, `a`.`Salt`, `a`.`Verifier`, `a`.`RealmName`, `a`.`LastIP`, `a`.`CoreName`, `a`.`ModuleVersion`
    //       7              8
    // `ab`.`bandate`, `ab`.`unbandate`
    // FROM `account` a
    // LEFT JOIN `account_banned` ab ON `a`.`id` = `ab`.`id` AND `ab`.`active` = 1 WHERE `a`.`Name` = ? LIMIT 1

    explicit AccountInfo(Field* fields)
    {
        ID                  = fields[0].Get<uint32>();
        Salt                = fields[1].Get<Binary, Warhead::Crypto::SRP6::SALT_LENGTH>();
        Verifier            = fields[2].Get<Binary, Warhead::Crypto::SRP6::VERIFIER_LENGTH>();
        GuildID             = fields[3].Get<uint64>();
        RealmName           = fields[4].Get<std::string>();
        LastIP              = fields[5].Get<std::string>();
        CoreName            = fields[6].Get<std::string>();
        ModuleVersion       = fields[7].Get<uint32>();

        if (!fields[8].IsNull())
            BanDate = fields[8].Get<Seconds>(false);

        if (!fields[9].IsNull())
            UnBanDate = fields[9].Get<Seconds>(false);

        if (UnBanDate > 0s && UnBanDate < GameTime::GetGameTime())
            IsBanned = true;

        if (BanDate > 0s && BanDate == UnBanDate)
            IsPermanentlyBanned = true;
    }

    inline bool CheckKey(std::string const& accountName, std::string const& key)
    {
        std::string safeAccount = accountName;
        std::transform(safeAccount.begin(), safeAccount.end(), safeAccount.begin(), ::toupper);
        Utf8ToUpperOnlyLatin(safeAccount);

        std::string safeKey = key;
        Utf8ToUpperOnlyLatin(safeKey);
        std::transform(safeKey.begin(), safeKey.end(), safeKey.begin(), ::toupper);

        return Warhead::Crypto::SRP6::CheckLogin(safeAccount, safeKey, Salt, Verifier);
    }
};

DiscordSocket::ReadDataHandlerResult DiscordSocket::ReadDataHandler()
{
    DiscordClientPktHeader* header = reinterpret_cast<DiscordClientPktHeader*>(_headerBuffer.GetReadPointer());
    OpcodeClient opcode = static_cast<OpcodeClient>(header->cmd);

    DiscordPacket packet(opcode, std::move(_packetBuffer));
    DiscordPacket* packetToQueue;

    std::unique_lock<std::mutex> sessionGuard(_discordSessionLock, std::defer_lock);

    switch (opcode)
    {
        case CLIENT_SEND_PING:
        {
            LogOpcodeText(opcode);
            try
            {
                return HandlePing(packet) ? ReadDataHandlerResult::Ok : ReadDataHandlerResult::Error;
            }
            catch (ByteBufferException const&)
            {

            }

            LOG_ERROR("network", "DiscordSocket::ReadDataHandler(): client {} sent malformed CLIENT_SEND_PING", GetRemoteIpAddress().to_string());
            return ReadDataHandlerResult::Error;
        }
        case CLIENT_AUTH_SESSION:
        {
            LogOpcodeText(opcode);
            if (_authed)
            {
                // locking just to safely log offending user is probably overkill but we are disconnecting him anyway
                if (sessionGuard.try_lock())
                    LOG_ERROR("network", "DiscordSocket::ProcessIncoming: received duplicate CMSG_AUTH_SESSION from {}", _discordSession->GetRemoteAddress());

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

    if (!_discordSession)
    {
        LOG_ERROR("network.opcode", "ProcessIncoming: Client not authed opcode = {}", uint32(opcode));
        delete packetToQueue;
        return ReadDataHandlerResult::Error;
    }

    OpcodeHandler const* handler = opcodeTable[opcode];
    if (!handler)
    {
        LOG_ERROR("network.opcode", "No defined handler for opcode {} sent by {}", GetOpcodeNameForLogging(static_cast<OpcodeClient>(packetToQueue->GetOpcode())), _discordSession->GetRemoteAddress());
        delete packetToQueue;
        return ReadDataHandlerResult::Error;
    }

    // Copy the packet to the heap before enqueuing
    _discordSession->QueuePacket(*packetToQueue);
    delete packetToQueue;

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
    recvPacket >> authSession->Key;
    recvPacket >> authSession->CoreName;
    recvPacket >> authSession->CoreVersion;
    recvPacket >> authSession->ModuleVersion;

    // Get the account information from the database
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

    auto account = std::make_shared<AccountInfo>(result->Fetch());

    // For hook purposes, we get Remoteaddress at this point.
    account->RemoteIpAddress = std::make_unique<boost::asio::ip::address>(GetRemoteIpAddress());

    if (account->IsPermanentlyBanned)
    {
        SendAuthResponseError(DiscordAuthResponseCodes::BannedPermanentlyAccount);
        LOG_ERROR("network", "DiscordSocket::CheckIpCallback: Sent Auth Response 'BannedPermanentlyAccount' (Account {} permanently banned).", authSession->Account);
        DelayedCloseSocket();
        sBanMgr->AddAccount(authSession->Account, BanInfo(0s, account->BanDate));
        return;
    }
    else if (account->IsBanned)
    {
        SendAuthResponseError(DiscordAuthResponseCodes::BannedAccount);
        LOG_ERROR("network", "DiscordSocket::CheckIpCallback: Sent Auth Response 'BannedAccount' (Account {} banned).", authSession->Account);
        DelayedCloseSocket();
        sBanMgr->AddAccount(authSession->Account, BanInfo(account->UnBanDate - GameTime::GetGameTime(), account->BanDate));
        return;
    }

    // First reject the connection if packet contains invalid data or realm state doesn't allow logging in
    if (sDiscord->IsClosed())
    {
        SendAuthResponseError(DiscordAuthResponseCodes::ServerOffline);
        LOG_ERROR("network", "DiscordSocket::HandleAuthSession: Discord closed, denying client ({}).", GetRemoteIpAddress().to_string());
        DelayedCloseSocket();
        return;
    }

    if (!account->CheckKey(authSession->Account, authSession->Key))
    {
        SendAuthResponseError(DiscordAuthResponseCodes::IncorrectKey);
        LOG_ERROR("network", "DiscordSocket::HandleAuthSession: Sent Auth Response (incorrect key).");
        DelayedCloseSocket();
        return;
    }

    if (IpLocationRecord const* location = sIPLocation->GetLocationRecord(account->RemoteIpAddress->to_string()))
        _ipCountry = location->CountryCode;

    sDiscordBot->CheckBotInGuild(account->GuildID, [this, authSession, account](bool isExist)
    {
        if (!isExist)
        {
            SendAuthResponseError(DiscordAuthResponseCodes::BotNotFound);
            LOG_ERROR("network", "DiscordSocket::HandleAuthSession: Sent Auth Response (bot not found).");
            DelayedCloseSocket();
            return;
        }

        sDiscordBot->CheckChannels(account->GuildID, [this, authSession, account](DiscordChannelsList&& channelList)
        {
            if (channelList.empty())
            {
                SendAuthResponseError(DiscordAuthResponseCodes::ChannelsNotFound);
                LOG_ERROR("network", "DiscordSocket::HandleAuthSession: Sent Auth Response (channels not found).");
                DelayedCloseSocket();
                return;
            }

            if (channelList.size() != DEFAULT_CHANNELS_COUNT)
            {
                SendAuthResponseError(DiscordAuthResponseCodes::ChannelsIncorrect);
                LOG_ERROR("network", "DiscordSocket::HandleAuthSession: Sent Auth Response (channels incorrect).");
                DelayedCloseSocket();
                return;
            }

            LOG_INFO("network", "DiscordSocket::HandleAuthSession: Client '{}' authenticated successfully from {}", authSession->Account, account->RemoteIpAddress->to_string());

            _authed = true;

            _discordSession = new DiscordSession(account->ID, account->GuildID, std::move(authSession->Account), std::move(channelList), shared_from_this());

            sDiscord->AddSession(_discordSession);

            AsyncRead();
        });
    });
}

void DiscordSocket::SendAuthResponseError(DiscordAuthResponseCodes code)
{
    DiscordPacket packet(SERVER_SEND_AUTH_RESPONSE, 1);
    packet << uint8(code);

    SendPacketAndLogOpcode(packet);
}

bool DiscordSocket::HandlePing(DiscordPacket& recvPacket)
{
    using namespace std::chrono;

    int64 timePacket;
    int64 latency;

    // Get the ping packet content
    recvPacket >> timePacket;
    recvPacket >> latency;

    if (_LastPingTime == TimePoint())
    {
        _LastPingTime = steady_clock::now();
    }
    else
    {
        TimePoint now = steady_clock::now();

        steady_clock::duration diff = now - _LastPingTime;

        _LastPingTime = now;

        if (diff < 10s)
        {
            ++_OverSpeedPings;

            //uint32 maxAllowed = CONF_GET_INT("MaxOverspeedPings");
            uint32 maxAllowed = 5;

            if (maxAllowed && _OverSpeedPings > maxAllowed)
            {
                std::unique_lock<std::mutex> sessionGuard(_discordSessionLock);

                if (_discordSession)
                {
                    LOG_ERROR("network", "DiscordSocket::HandlePing: {} kicked for over-speed pings (address: {})",
                        _discordSession->GetAccountId(), GetRemoteIpAddress().to_string());

                    return false;
                }
            }
        }
        else
            _OverSpeedPings = 0;
    }

    {
        std::lock_guard<std::mutex> sessionGuard(_discordSessionLock);

        if (_discordSession)
            _discordSession->SetLatency(latency);
        else
        {
            LOG_ERROR("network", "DiscordSocket::HandlePing: peer sent CMSG_PING, but is not authenticated or got recently kicked, address = {}", GetRemoteIpAddress().to_string());
            return false;
        }
    }

    DiscordPacket packet(SERVER_SEND_PONG, 1);
    packet << timePacket;
    SendPacketAndLogOpcode(packet);

    return true;
}
