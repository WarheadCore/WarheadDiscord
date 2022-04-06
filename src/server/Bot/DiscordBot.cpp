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

#include "DiscordBot.h"
#include "AccountMgr.h"
#include "DiscordConfig.h"
#include "GameTime.h"
#include "GitRevision.h"
#include "Log.h"
#include "StringConvert.h"
#include "Tokenize.h"
#include "UpdateTime.h"
#include "Discord.h"
#include "StopWatch.h"
#include <dpp/dpp.h>

DiscordBot* DiscordBot::instance()
{
    static DiscordBot instance;
    return &instance;
}

void DiscordBot::Start()
{
    _isEnable = CONF_GET_BOOL("Discord.Bot.Enable");

    if (!_isEnable)
        return;

    std::string botToken = CONF_GET_STR("Discord.Bot.Token");
    if (botToken.empty())
    {
        LOG_FATAL("discord", "> Empty bot token for discord. Disable system");
        _isEnable = false;
        return;
    }

    _warheadServerID = sDiscordConfig->GetOption<int64>("Discord.Guild.ID");
    if (!_warheadServerID)
    {
        LOG_FATAL("discord", "> Empty guild id for discord. Disable system");
        _isEnable = false;
        return;
    }

    _bot = std::make_unique<dpp::cluster>(botToken, dpp::i_all_intents);

    // Prepare logs
    ConfigureLogs();

    // Prepare commands
    ConfigureCommands();

    _bot->start(true);
}

void DiscordBot::SendDefaultMessage(int64 channelID, std::string_view message)
{
    if (!_isEnable)
        return;

    dpp::message discordMessage;
    discordMessage.channel_id = channelID;
    discordMessage.content = std::string(message);

    _bot->message_create(discordMessage);
}

void DiscordBot::SendEmbedMessage(int64 channelID, dpp::embed const* embed)
{
    if (!_isEnable || !embed)
        return;

    _bot->message_create(dpp::message(channelID, *embed));
}

void DiscordBot::ConfigureLogs()
{
    if (!_isEnable)
        return;

    _bot->on_ready([this]([[maybe_unused]] const auto& event)
    {
        LOG_INFO("discord.bot", "> DiscordBot: Logged in as {}", _bot->me.username);
    });

    _bot->on_log([](const dpp::log_t& event)
    {
        switch (event.severity)
        {
        case dpp::ll_trace:
            LOG_TRACE("discord.bot", "> DiscordBot: {}", event.message);
            break;
        case dpp::ll_debug:
            LOG_DEBUG("discord.bot", "> DiscordBot: {}", event.message);
            break;
        case dpp::ll_info:
            LOG_INFO("discord.bot", "> DiscordBot: {}", event.message);
            break;
        case dpp::ll_warning:
            LOG_WARN("discord.bot", "> DiscordBot: {}", event.message);
            break;
        case dpp::ll_error:
            LOG_ERROR("discord.bot", "> DiscordBot: {}", event.message);
            break;
        case dpp::ll_critical:
            LOG_CRIT("discord.bot", "> DiscordBot: {}", event.message);
            break;
        default:
            break;
        }
    });
}

void DiscordBot::ConfigureCommands()
{
    if (!_isEnable)
        return;

    _bot->on_ready([this](const dpp::ready_t& /*event*/)
    {
        if (dpp::run_once<struct register_bot_commands>())
        {
            //dpp::slashcommand coreComand("core", "Категория команд для ядра сервера", _bot->me.id);
            //dpp::slashcommand onlineComand("online", "Текущий онлайн сервера", _bot->me.id);
            dpp::slashcommand accountComand("account", "Commands for accounts helpers", _bot->me.id);

            //coreComand.add_option(dpp::command_option(dpp::co_sub_command, "info", "Общая информация о ядре и игровом мире"));
            accountComand.add_option(dpp::command_option(dpp::co_sub_command, "create", "Create account for Warhead Discord"));

            //_bot->guild_command_create(coreComand, _serverID);
            //_bot->guild_command_create(onlineComand, _serverID);
            _bot->guild_command_create(accountComand, _warheadServerID);
        }
    });

    _bot->on_interaction_create([this](const dpp::interaction_create_t& event)
    {
        dpp::command_interaction cmdData = event.command.get_command_interaction();

        if (event.command.get_command_name() == "account" && cmdData.options[0].name == "create")
        {
            dpp::interaction_modal_response modal("accCreate", "Create account for Warhead Discord");
            modal.add_component(
                dpp::component().
                set_label("Enter account name").
                set_id("account").
                set_type(dpp::cot_text).
                set_placeholder("name").
                set_min_length(4).
                set_max_length(MAX_ACCOUNT_STR).
                set_text_style(dpp::text_short));
            event.dialog(modal);
        }
    });

    _bot->on_form_submit([this](const dpp::form_submit_t& event)
    {
        auto const& value = std::get<std::string>(event.components[0].components[0].value);
        std::string key = sAccountMgr->GetRandomKey();

        auto result = sAccountMgr->CreateAccount(value, key);
        if (result != AccountResponceResult::Ok)
        {
            std::string errorMsg;

            switch (result)
            {
            case AccountResponceResult::LongName:
                errorMsg = Warhead::StringFormat("Name of account is too big. Recommend max `'{}'` chars. You used `'{}'`", MAX_ACCOUNT_STR, value.size());
                break;
            case AccountResponceResult::NameAlreadyExist:
                errorMsg = Warhead::StringFormat("Name of account `'{}'` is exist. Try other", value);
                break;
            default:
                errorMsg = "Unknown error";
                break;
            }

            dpp::message m;
            m.set_content(Warhead::StringFormat("Error at create. {}", errorMsg));
            m.set_flags(dpp::m_ephemeral);

            event.reply(m);
            return;
        }

        dpp::embed embed = dpp::embed();
        embed.set_color(static_cast<uint32>(DiscordMessageColor::Teal));
        embed.set_title("Create account for Warhead Discord");
        embed.set_description(Warhead::StringFormat("You have successfully created an account: `{}`", value));
        embed.add_field("You key", Warhead::StringFormat("`{}`", key));
        embed.set_timestamp(GameTime::GetGameTime().count());

        dpp::message message;
        message.add_embed(embed);
        message.set_flags(dpp::m_ephemeral);
        event.reply(message);
    });
}
