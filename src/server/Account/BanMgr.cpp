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

#include "BanMgr.h"
#include "DatabaseEnv.h"
#include "AccountMgr.h"
#include "Containers.h"
#include "GameTime.h"
#include "Discord.h"
#include "DiscordSession.h"
#include "Log.h"
#include "Timer.h"
#include "StopWatch.h"

BanInfo::BanInfo(Seconds duration, Seconds banDate /*= 0s*/) :
    Duration(duration), BanDate(banDate == 0s ? GameTime::GetGameTime() : banDate), UnabanDate(banDate + duration) { }

BanMgr* BanMgr::instance()
{
    static BanMgr instance;
    return &instance;
}

void BanMgr::Initialize()
{
    StopWatch sw;

    auto stmt = DiscordDatabase.GetPreparedStatement(DISCORD_UPD_ACCOUNT_BAN_EXPIRED);
    DiscordDatabase.Execute(stmt);

    stmt = DiscordDatabase.GetPreparedStatement(DISCORD_UPD_IP_BAN_EXPIRED);
    DiscordDatabase.Execute(stmt);
}

BanResponceCode BanMgr::BanAccount(std::string_view accountName, Seconds duration, std::string_view reason)
{
    if (accountName.empty())
        return BanResponceCode::Error;

    uint32 accountID = sAccountMgr->GetID(accountName);
    if (!accountID)
        return BanResponceCode::NotFound;

    auto const& banInfo = GetBanInfoAccount(std::string(accountName));
    if (banInfo && (banInfo->IsPermanently() || banInfo->UnabanDate > GameTime::GetGameTime()))
        return BanResponceCode::Exist;

    DeleteAccount(std::string(accountName));

    // No SQL injection with prepared statements
    auto stmt = DiscordDatabase.GetPreparedStatement(DISCORD_INS_ACCOUNT_BAN);
    stmt->SetData(0, accountID);
    stmt->SetData(1, duration);
    stmt->SetData(2, "Console");
    stmt->SetData(3, reason);
    DiscordDatabase.Execute(stmt);

    if (auto session = sDiscord->FindSession(accountID))
        session->KickSession("Ban Account");

    std::string durationString = Warhead::Time::ToTimeString(duration);
    if (duration == 0s)
        durationString = "Permanently";

    LOG_WARN("ban.account", "> Ban account '{}'. Reason '{}'. Duration {}", accountName, reason, durationString);

    AddAccount(std::string(accountName), BanInfo(duration));

    return BanResponceCode::Ok;
}

BanResponceCode BanMgr::BanIp(std::string_view ip, Seconds duration, std::string_view reason)
{
    if (ip.empty())
        return BanResponceCode::Error;

    auto const& banInfo = GetBanInfoIp(std::string(ip));
    if (banInfo && (banInfo->IsPermanently() || banInfo->UnabanDate > GameTime::GetGameTime()))
        return BanResponceCode::Exist;

    DeleteIp(std::string(ip));

    auto stmt = DiscordDatabase.GetPreparedStatement(DISCORD_INS_IP_BAN);
    stmt->SetData(0, ip);
    stmt->SetData(1, duration);
    stmt->SetData(2, "Console");
    stmt->SetData(3, reason);
    DiscordDatabase.Execute(stmt);

    for (auto const& [accountID, session] : sDiscord->GetAllSessions())
    {
        if (session->GetRemoteAddress() == ip)
            session->KickSession("Ban IP");
    }

    std::string durationString = Warhead::Time::ToTimeString(duration);
    if (duration == 0s)
        durationString = "Permanently";

    LOG_WARN("ban.account", "> Ban ip '{}'. Reason '{}'. Duration {}", ip, reason, durationString);

    AddIp(std::string(ip), BanInfo(duration));

    return BanResponceCode::Ok;
}

BanInfo const* BanMgr::GetBanInfoAccount(std::string const& accountName)
{
    auto banInfo = Warhead::Containers::MapGetValuePtr(_storeAccount, accountName);
    return banInfo ? banInfo->get() : nullptr;
}

BanInfo const* BanMgr::GetBanInfoIp(std::string const& ip)
{
    auto banInfo = Warhead::Containers::MapGetValuePtr(_storeIp, ip);
    return banInfo ? banInfo->get() : nullptr;
}

void BanMgr::AddAccount(std::string const& accountName, BanInfo const& banType)
{
    auto const& itr = _storeAccount.find(accountName);
    if (itr != _storeAccount.end())
        _storeAccount.erase(accountName);

    _storeAccount.emplace(accountName, banType);
}

void BanMgr::AddIp(std::string const& ip, BanInfo const& banType)
{
    auto const& itr = _storeIp.find(ip);
    if (itr != _storeIp.end())
        _storeIp.erase(ip);

    _storeIp.emplace(ip, banType);
}

void BanMgr::DeleteAccount(std::string const& accountName)
{
    if (!GetBanInfoAccount(accountName))
        return;

    uint32 accountID = sAccountMgr->GetID(accountName);
    if (!accountID)
        return;

    auto stmt = DiscordDatabase.GetPreparedStatement(DISCORD_DEL_BAN_ACCOUNT);
    stmt->SetArguments(accountID);
    DiscordDatabase.Execute(stmt);

    _storeAccount.erase(accountName);
}

void BanMgr::DeleteIp(std::string const& ip)
{
    if (!GetBanInfoIp(ip))
        return;

    auto stmt = DiscordDatabase.GetPreparedStatement(DISCORD_DEL_BAN_IP);
    stmt->SetArguments(ip);
    DiscordDatabase.Execute(stmt);

    _storeIp.erase(ip);
}
