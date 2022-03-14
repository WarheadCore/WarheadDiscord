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

#include "Packet.h"
#include "Errors.h"

DiscordPackets::Packet::Packet(DiscordPacket&& discordPacket)
    : _worldPacket(std::move(discordPacket))
{
}

DiscordPackets::ServerPacket::ServerPacket(OpcodeServer opcode, size_t initialSize /*= 200*/)
    : Packet(DiscordPacket(opcode, initialSize))
{
}

void DiscordPackets::ServerPacket::Read()
{
    ASSERT(!"Read not implemented for server packets.");
}

DiscordPackets::ClientPacket::ClientPacket(OpcodeClient expectedOpcode, DiscordPacket&& packet)
    : Packet(std::move(packet))
{
    ASSERT(GetOpcode() == expectedOpcode);
}

DiscordPackets::ClientPacket::ClientPacket(DiscordPacket&& packet)
    : Packet(std::move(packet))
{
}

DiscordPacket const* DiscordPackets::ClientPacket::Write()
{
    ASSERT(!"Write not allowed for client packets.");
    // Shut up some compilers
    return nullptr;
}
