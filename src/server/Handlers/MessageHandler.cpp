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
#include "DiscordBot.h"
#include "DiscordSession.h"
#include "Log.h"
#include "MessagePackets.h"
#include <dpp/dpp.h>

void DiscordSession::HandleSendDiscordMessageOpcode(DiscordPackets::Message::SendDiscordMessage& packet)
{
    auto channelID = GetChannelID(packet.ChannelType);
    if (!channelID)
        return;

    sDiscordBot->SendDefaultMessage(channelID, packet.Context);
}

void DiscordSession::HandleSendDiscordEmbedMessageOpcode(DiscordPackets::Message::SendDiscordEmbedMessage& packet)
{
    auto channelID = GetChannelID(packet.ChannelType);
    if (!channelID)
        return;

    auto embed = std::make_shared<dpp::embed>();
    embed->set_color(packet.Color);
    embed->set_title(packet.Title);
    embed->set_description(packet.Description);

    for (auto const& embedField : packet.EmbedFields)
    {
        if (!embedField.IsCorrectName())
            LOG_ERROR("discord", "> Incorrect size for embed name. Size {}. Context '{}'", embedField.Name.size(), embedField.Name);

        if (!embedField.IsCorrectValue())
            LOG_ERROR("discord", "> Incorrect size for embed value. Size {}. Context '{}'", embedField.Value.size(), embedField.Value);

        embed->add_field(embedField.Name, embedField.Value, embedField.IsInline);
    }

    embed->set_timestamp(packet.Timestamp);

    sDiscordBot->SendEmbedMessage(channelID, embed);
}
