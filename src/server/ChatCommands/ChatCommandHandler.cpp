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

#include "ChatCommandHandler.h"
#include "Errors.h"
#include <dpp/cluster.h>

CommandHandler* CommandHandler::instance()
{
    static CommandHandler instance;
    return &instance;
}

void CommandHandler::AddCommand(std::string_view commandName, std::string_view description, CommandParameters&& params, CommandInvoke&& invoke)
{
    auto [itr, isEmplace] = _commandInvoke.emplace(std::string{ commandName }, std::move(invoke));
    ASSERT(isEmplace, "> Command '{}' is exist", commandName);

    dpp::slashcommand command;
    command.set_name(std::string{ commandName });
    command.set_description(std::string{ description });

    CommandParameters copyParams{ std::move(params) };

    for (auto& [option, optionChoiceList] : copyParams)
    {
        for (auto const& choice : optionChoiceList)
            option.add_choice(choice);

        command.add_option(option);
    }

    AddSlashCommand(std::move(command));
}

void CommandHandler::SetApplicaionID(dpp::cluster* bot)
{
    for (auto& command : _commands)
    {
        if (!command.application_id)
            command.set_application_id(bot->me.id);
    }
}

void CommandHandler::RefisterCommandsForGuild(dpp::cluster* bot, uint64 guildID)
{
    bot->guild_bulk_command_create(_commands, guildID, [](const dpp::confirmation_callback_t& callback)
    {
        if (callback.is_error())
            std::cout << callback.http_info.body << "\n";
    });
}

void CommandHandler::TryExecuteCommand(dpp::slashcommand_t const& slashCommandEvent)
{
    auto const& itr = _commandInvoke.find(slashCommandEvent.command.get_command_name());
    if (itr == _commandInvoke.end())
        return;

    itr->second(slashCommandEvent);
}

void CommandHandler::AddSlashCommand(dpp::slashcommand&& command)
{
    _commands.emplace_back(std::move(command));
}
