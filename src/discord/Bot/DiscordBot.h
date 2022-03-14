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

#ifndef _DISCORD_H_
#define _DISCORD_H_

#include "Define.h"
#include <memory>
#include <string_view>

namespace dpp
{
    class cluster;
    struct embed;
}

enum class DiscordMessageColor : uint32
{
    Blue = 0x28a745,
    Red = 0xdc3545,
    Orange = 0xfd7e14,
    Purple = 0x6f42c1,
    Indigo = 0x6610f2,
    Yellow = 0xffc107,
    Teal = 0x20c997,
    Cyan = 0x17a2b8,
    Gray = 0xadb5bd,
    White = 0xffffff
};

class WH_DISCORD_API DiscordBot
{
    DiscordBot() = default;
    ~DiscordBot() = default;

    DiscordBot(DiscordBot const&) = delete;
    DiscordBot(DiscordBot&&) = delete;
    DiscordBot& operator=(DiscordBot const&) = delete;
    DiscordBot& operator=(DiscordBot&&) = delete;

public:
    static DiscordBot* instance();

    void SendDefaultMessage(int64 channelID, std::string_view message);
    void SendEmbedMessage(int64 channelID, dpp::embed const* embed);

    void Start();    

private:
    void ConfigureLogs();

    bool _isEnable{ false };

    std::unique_ptr<dpp::cluster> _bot = {};
};

#define sDiscordBot DiscordBot::instance()

#endif // _DISCORD_H_
