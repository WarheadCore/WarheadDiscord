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

    _bot = std::make_unique<dpp::cluster>(botToken, dpp::i_all_intents);

    // Prepare logs
    ConfigureLogs();

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
