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

#include "Define.h"
#include "Log.h"
#include "DiscordBot.h"
#include "DiscordSession.h"
#include "MessagePackets.h"
#include <dpp/dpp.h>

void DiscordSession::HandleSendDiscordMessageOpcode(DiscordPackets::Message::SendDiscordMessage& packet)
{
    sDiscordBot->SendDefaultMessage(packet.ChannelID, packet.Context);
}

void DiscordSession::HandleSendDiscordEmbedMessageOpcode(DiscordPackets::Message::SendDiscordEmbedMessage& packet)
{
    dpp::embed embed;
    embed.set_color(packet.Color);
    embed.set_title(packet.Title);
    embed.set_description(packet.Description);
    embed.set_timestamp(packet.Timestamp);

    sDiscordBot->SendEmbedMessage(packet.ChannelID, &embed);
}
