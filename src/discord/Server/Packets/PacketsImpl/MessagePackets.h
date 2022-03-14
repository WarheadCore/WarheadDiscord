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

#ifndef MessagePackets_h__
#define MessagePackets_h__

#include "Packet.h"

namespace DiscordPackets::Message
{
    class SendDiscordMessage final : public ClientPacket
    {
    public:
        SendDiscordMessage(DiscordPacket&& packet) : ClientPacket(CLIENT_SEND_MESSAGE, std::move(packet)) { }

        void Read() override;

        int64 ChannelID{ 0 };
        std::string Context;
    };

    class SendDiscordEmbedMessage final : public ClientPacket
    {
    public:
        SendDiscordEmbedMessage(DiscordPacket&& packet) : ClientPacket(CLIENT_SEND_MESSAGE_EMBED, std::move(packet)) { }

        void Read() override;

        int64 ChannelID{ 0 };
        uint32 Color{ 0 };
        std::string Title;
        std::string Description;
        time_t Timestamp{ 0 };
    };
}

#endif // ChatPackets_h__
