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

#include "AccountMgr.h"
#include "Containers.h"
#include "DatabaseEnv.h"
#include "DiscordSharedDefines.h"
#include "Log.h"
#include "SRP6.h"
#include "StopWatch.h"
#include "Util.h"
#include <algorithm>

/*static*/ AccountMgr* AccountMgr::instance()
{
    static AccountMgr instance;
    return &instance;
}

void AccountMgr::Initialize()
{
    LOG_INFO("server.loading", "> Loading accounts...");

    StopWatch sw;

    _accounts.clear();

    auto result = DiscordDatabase.Query(DiscordDatabase.GetPreparedStatement(DISCORD_SEL_ACCOUNTS));
    if (!result)
    {
        LOG_ERROR("server.loading", "> Not found accounts");
        return;
    }

    do
    {
        auto const& [id, name, guildID, realmName] = result->FetchTuple<uint32, std::string_view, int64, std::string_view>();
        AddAccountInfo(id, name, guildID, realmName);
    } while (result->NextRow());

    LOG_INFO("server.loading", "> Loaded {} accounts in {}", _accounts.size(), sw);
    LOG_INFO("server.loading", "");
}

void AccountMgr::Update()
{
    _queryProcessor.ProcessReadyCallbacks();
}

void AccountMgr::AddAccountInfo(DiscordAccountInfo&& info)
{
    if (GetAccountInfo(info.ID))
    {
        LOG_ERROR("account", "> Account with id {} exist", info.ID);
        return;
    }

    _accounts.emplace(info.ID, std::move(info));
}

void AccountMgr::AddAccountInfo(uint32 id, std::string_view name, int64 guildID, std::string_view realmName)
{
    AddAccountInfo(std::move(DiscordAccountInfo(id, name, guildID, realmName)));
}

DiscordAccountInfo const* AccountMgr::GetAccountInfo(uint32 id)
{
    return Warhead::Containers::MapGetValuePtr(_accounts, id);
}

AccountResponceResult AccountMgr::CreateAccount(std::string username, std::string key, int64 guildID, std::string_view realmName /*= {}*/)
{
    if (utf8length(username) > MAX_ACCOUNT_STR)
        return AccountResponceResult::LongName;

    if (utf8length(key) > MAX_PASS_STR)
        return AccountResponceResult::LongKey;

    Utf8ToUpperOnlyLatin(username);
    Utf8ToUpperOnlyLatin(key);

    if (GetID(username))
        return AccountResponceResult::NameAlreadyExist;

    if (realmName.empty())
        realmName = "WarheadCore";

    auto const& [salt, verifier] = Warhead::Crypto::SRP6::MakeRegistrationData(username, key);

    // INSERT INTO account (`Name`, `Salt`, `Verifier`, `GuildID`, `RealmName`, `JoinDate`) VALUES (?, ?, ?, ?, ?, NOW())
    auto stmt = DiscordDatabase.GetPreparedStatement(DISCORD_INS_ACCOUNT);
    stmt->SetArguments(username, salt, verifier, guildID, realmName);
    DiscordDatabase.Execute(stmt);

    CheckAccount(username, [this, username, guildID, realmName](uint32 accountID)
    {
        if (!accountID)
        {
            LOG_INFO("account", "> Incorrect account name '{}' at add info to store", username);
            return;
        }

        AddAccountInfo(accountID, username, guildID, realmName);
    });

    return AccountResponceResult::Ok;
}

uint32 AccountMgr::GetID(std::string_view accountName)
{
    for (auto const& [ID, info] : _accounts)
        if (StringEqualI(info.Name, accountName))
            return ID;

    return 0;
}

std::string_view AccountMgr::GetName(uint32 id)
{
    auto const& itr = _accounts.find(id);
    if (itr != _accounts.end())
        return std::string_view{ itr->second.Name };

    return {};
}

std::string_view AccountMgr::GetRealmName(uint32 id)
{
    auto const& itr = _accounts.find(id);
    if (itr != _accounts.end())
        return std::string_view{ itr->second.RealmName };

    return {};
}

void AccountMgr::CheckAccount(std::string_view accountName, std::function<void(uint32)>&& execute)
{
    auto stmt = DiscordDatabase.GetPreparedStatement(DISCORD_SEL_ACCOUNT_ID_BY_USERNAME);
    stmt->SetArguments(accountName);

    _queryProcessor.AddCallback(DiscordDatabase.AsyncQuery(stmt).WithPreparedCallback([execute = std::move(execute)](PreparedQueryResult result)
    {
        if (!result)
        {
            execute(0);
            return;
        }

        auto const& [accountID] = result->FetchTuple<uint32>();
        execute(accountID);
    }));
}

AccountResponceResult AccountMgr::ChangeKey(std::string_view name, std::string newPassword)
{
    std::string safeUser{ name };
    Utf8ToUpperOnlyLatin(safeUser);
    Utf8ToUpperOnlyLatin(newPassword);

    uint32 accountID = GetID(safeUser);

    if (!accountID)
        return AccountResponceResult::NameNotExist;

    if (utf8length(newPassword) > MAX_PASS_STR)
        return AccountResponceResult::LongKey;

    auto [salt, verifier] = Warhead::Crypto::SRP6::MakeRegistrationData(safeUser, newPassword);

    auto stmt = DiscordDatabase.GetPreparedStatement(DISCORD_UPD_LOGON);
    stmt->SetArguments(salt, verifier, accountID);
    DiscordDatabase.Execute(stmt);

    return AccountResponceResult::Ok;
}

std::string AccountMgr::GetRandomKey()
{
    std::string key;
    key.resize(MAX_PASS_STR, 0);

    uint32 smallChars = urand(1, MAX_PASS_STR);
    uint32 bigChars = urand(1, MAX_PASS_STR - smallChars);
    uint32 numbers = MAX_PASS_STR - smallChars - bigChars;

    // Rand numbers
    for (uint32 i = 0; i < numbers; ++i)
        key[i] = char(rand() % 10 + 48);

    for (uint32 i = numbers; i < numbers + bigChars; ++i)
        key[i] = char(rand() % 26 + 65);

    for (uint32 i = numbers + bigChars; i < MAX_PASS_STR; ++i)
        key[i] = char(rand() % 26 + 97);

    Warhead::Containers::RandomShuffle(key);

    return key;
}
