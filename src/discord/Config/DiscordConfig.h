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

#ifndef _DISCORD_CONFIG_H_
#define _DISCORD_CONFIG_H_

#include "Define.h"
#include "Optional.h"
#include <string_view>
#include <unordered_map>

class WH_DISCORD_API DiscordConfig
{
    DiscordConfig(DiscordConfig const&) = delete;
    DiscordConfig(DiscordConfig&&) = delete;
    DiscordConfig& operator= (DiscordConfig const&) = delete;
    DiscordConfig& operator= (DiscordConfig&&) = delete;

    DiscordConfig() = default;
    ~DiscordConfig() = default;

public:
    static DiscordConfig* instance();

    // Add config option
    template<typename T>
    void AddOption(std::string_view optionName, Optional<T> def = {});

    // Add option without template
    void AddOption(std::string_view optionName, Optional<std::string> def = {});

    // Add option list without template
    void AddOption(std::initializer_list<std::string> const& optionList);

    // Get config options
    template<typename T>
    T GetOption(std::string_view optionName, Optional<T> = std::nullopt);

    // Set config option
    template<typename T>
    void SetOption(std::string_view optionName, T value);

private:
    std::unordered_map<std::string /*name*/, std::string /*value*/> _configOptions;
};

#define sDiscordConfig DiscordConfig::instance()

#define CONF_GET_BOOL(__optionName) sDiscordConfig->GetOption<bool>(__optionName)
#define CONF_GET_STR(__optionName) sDiscordConfig->GetOption<std::string>(__optionName)
#define CONF_GET_INT(__optionName) sDiscordConfig->GetOption<int32>(__optionName)
#define CONF_GET_UINT(__optionName) sDiscordConfig->GetOption<uint32>(__optionName)
#define CONF_GET_FLOAT(__optionName) sDiscordConfig->GetOption<float>(__optionName)

#endif // __GAME_CONFIG
