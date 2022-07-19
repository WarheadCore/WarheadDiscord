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

#ifndef _ACCOUNT_MGR_H
#define _ACCOUNT_MGR_H

#include "AsyncCallbackProcessor.h"
#include "DatabaseEnvFwd.h"
#include "Define.h"
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>

constexpr auto MAX_ACCOUNT_STR = 50;
constexpr auto MAX_PASS_STR = 50;

enum class AccountResponceResult : uint8;

struct DiscordAccountInfo
{
    DiscordAccountInfo(uint32 id, std::string_view name, int64 guildID, std::string_view realmName) :
        ID(id), Name(name), GuildID(guildID), RealmName(realmName) { }

    uint32 ID{ 0 };
    int64 GuildID{ 0 };
    std::string Name;
    std::string RealmName;
};

/// The Discord
class WH_SERVER_API AccountMgr
{
public:
    static AccountMgr* instance();

    void Initialize();

    void Update();

    AccountResponceResult CreateAccount(std::string username, std::string key, int64 guildID, std::string_view realmName = {});
    AccountResponceResult ChangeKey(std::string_view name, std::string newPassword);
    void CheckAccount(std::string_view accountName, std::function<void(uint32)>&& execute);
    uint32 GetID(std::string_view accountName);
    std::string_view GetName(uint32 id);
    std::string_view GetRealmName(uint32 id);
    std::string GetRandomKey();

    void AddAccountInfo(DiscordAccountInfo&& info);
    void AddAccountInfo(uint32 id, std::string_view name, int64 guildID, std::string_view realmName);
    inline auto GetAllAccountsInfo() { return &_accounts; }

    DiscordAccountInfo const* GetAccountInfo(uint32 id);

private:
    QueryCallbackProcessor _queryProcessor;
    std::unordered_map<uint32, DiscordAccountInfo> _accounts;
    std::mutex _mutex;
};

#define sAccountMgr AccountMgr::instance()

#endif
