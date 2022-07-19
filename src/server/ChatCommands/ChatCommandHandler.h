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

#ifndef _DISCORD_CHAT_COMMAND_HANDLER_H_
#define _DISCORD_CHAT_COMMAND_HANDLER_H_

#include "Define.h"
#include <vector>
#include <functional>
#include <string>
#include <string_view>
#include <dpp/dispatcher.h>

namespace dpp
{
    class cluster;
}

using CommandParameters = std::vector<std::pair<dpp::command_option, std::vector<dpp::command_option_choice>>>;
using CommandInvoke = std::function<void(dpp::slashcommand_t const&)>;

class CommandHandler
{
public:
    static CommandHandler* instance();

    void AddCommand(std::string_view commandName, std::string_view description, CommandParameters&& params, CommandInvoke&& invoke);
    void SetApplicaionID(dpp::cluster* bot);
    void RefisterCommandsForGuild(dpp::cluster* bot, uint64 guildID);
    void TryExecuteCommand(dpp::slashcommand_t const& slashCommandEvent);

private:
    void AddSlashCommand(dpp::slashcommand&& command);

    std::vector<dpp::slashcommand> _commands;
    std::unordered_map<std::string, CommandInvoke> _commandInvoke;

    CommandHandler() = default;
    ~CommandHandler() = default;

    CommandHandler(CommandHandler const&) = delete;
    CommandHandler(CommandHandler&&) = delete;
    CommandHandler& operator=(CommandHandler const&) = delete;
    CommandHandler& operator=(CommandHandler&&) = delete;
};

#define sCommandHandler CommandHandler::instance()

#endif // _DISCORD_CHAT_COMMAND_HANDLER_H_
