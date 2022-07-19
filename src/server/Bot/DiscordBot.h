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
#include "DiscordSharedDefines.h"
#include "Duration.h"
#include <memory>
#include <functional>
#include <unordered_map>

using CompleteFunction = std::function<void(bool)>;
using CompleteChannelFunction = std::function<void(DiscordChannelsList&&)>;

namespace dpp
{
    class cluster;
    class slashcommand;
    struct embed;
    struct command_option;
}

class TaskScheduler;
class ChatHandler;

struct DiscordClients
{
    DiscordClients(int64 guildID, std::string_view guildName, uint32 membersCount, Seconds inviteDate) :
        GuildID(guildID), GuildName(guildName), MembersCount(membersCount), InviteDate(inviteDate) { }

    int64 GuildID{ 0 };
    std::string GuildName;
    uint32 MembersCount{ 0 };
    Seconds InviteDate{ 0 };
};

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
    void SendEmbedMessage(int64 channelID, std::shared_ptr<dpp::embed> embed);

    void Start();
    void Test();
    void Update(Milliseconds diff);

    // Guid check
    void CheckBotInGuild(int64 guildID, CompleteFunction&& execute);
    void CheckChannels(int64 guildID, CompleteChannelFunction&& channelList);

private:
    void ConfigureLogs();
    void ConfigureCommands();
    void ConfigureGuildInviteHooks();
    void LoadClients();
    void CheckClients();

    // For guild
    void CreateCommands(int64 guildID);

    // Clients cache
    bool HasClient(int64 guildID);
    DiscordClients* GetClient(int64 guildID);
    void AddClient(int64 guildID, std::string_view guildName, uint32 membersCount, Seconds inviteDate, bool atStartup = false);
    void DeleteClient(int64 guildID);
    void DeleteAllClients();

    // Logs
    void LogAddClient(int64 guildID, DiscordMessageColor color, std::string_view icon, std::string_view guildName, uint32 membersCount, double creationDate);
    void LogDeleteClient(int64 guildID, DiscordMessageColor color, std::string_view icon, std::string_view guildName);

    bool _isEnable{ false };

    // Commands
    static bool HandleAccountTestCommand(ChatHandler* handler);

    std::unique_ptr<dpp::cluster> _bot;
    std::unique_ptr<TaskScheduler> _scheduler;

    std::unordered_map<int64, DiscordClients> _guilds;
    std::vector<std::string> _commands;
};

#define sDiscordBot DiscordBot::instance()

#endif // _DISCORD_H_
