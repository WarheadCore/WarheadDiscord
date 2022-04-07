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

#ifndef _DISCORD_BOT_H_
#define _DISCORD_BOT_H_

#include "Define.h"
#include <memory>
#include <string_view>

namespace dpp
{
    class cluster;
    struct embed;
}

class WH_SERVER_API DiscordBot
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
    void ConfigureCommands();

    bool _isEnable{ false };
    int64 _warheadServerID{ 0 };

    std::unique_ptr<dpp::cluster> _bot;
};

#define sDiscordBot DiscordBot::instance()

#endif // _DISCORD_H_
