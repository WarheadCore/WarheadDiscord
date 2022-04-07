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

#ifndef _BAN_MANAGER_H
#define _BAN_MANAGER_H

#include "Define.h"
#include "Duration.h"
#include <memory>
#include <string_view>
#include <string>
#include <unordered_map>

/// Ban function return codes
enum class BanResponceCode : uint8
{
    Ok,
    Error,
    NotFound,
    Exist
};

struct BanInfo
{
    BanInfo(Seconds duration, Seconds banDate = 0s);

    Seconds Duration;
    Seconds BanDate;
    Seconds UnabanDate;

    inline bool IsPermanently() const { return Duration == 0s || BanDate == UnabanDate; }
};

class WH_SERVER_API BanMgr
{
public:
    static BanMgr* instance();

    void Initialize();

    BanResponceCode BanAccount(std::string_view accountName, Seconds duration, std::string_view reason);
    BanResponceCode BanIp(std::string_view ip, Seconds duration, std::string_view reason);

    void DeleteAccount(std::string const& accountName);
    void DeleteIp(std::string const& ip);

    BanInfo const* GetBanInfoAccount(std::string const& accountName);
    BanInfo const* GetBanInfoIp(std::string const& ip);

    void AddAccount(std::string const& accountName, BanInfo const& banType);
    void AddIp(std::string const& ip, BanInfo const& banType);

private:
    std::unordered_map<std::string, std::unique_ptr<BanInfo>> _storeAccount;
    std::unordered_map<std::string, std::unique_ptr<BanInfo>> _storeIp;
};

#define sBanMgr BanMgr::instance()

#endif // _BAN_MANAGER_H
