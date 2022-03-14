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

#ifndef PacketBaseDiscord_h__
#define PacketBaseDiscord_h__

#include "DiscordPacket.h"

namespace DiscordPackets
{
    class WH_DISCORD_API Packet
    {
    public:
        Packet(DiscordPacket&& discordPacket);

        virtual ~Packet() = default;

        Packet(Packet const& right) = delete;
        Packet& operator=(Packet const& right) = delete;

        virtual DiscordPacket const* Write() = 0;
        virtual void Read() = 0;

        [[nodiscard]] DiscordPacket const* GetRawPacket() const { return &_worldPacket; }
        [[nodiscard]] size_t GetSize() const { return _worldPacket.size(); }

    protected:
        DiscordPacket _worldPacket;
    };

    class WH_DISCORD_API ServerPacket : public Packet
    {
    public:
        ServerPacket(OpcodeServer opcode, size_t initialSize = 200);

        void Read() override final;

        void Clear() { _worldPacket.clear(); }
        DiscordPacket&& Move() { return std::move(_worldPacket); }
        void ShrinkToFit() { _worldPacket.shrink_to_fit(); }

        [[nodiscard]] OpcodeServer GetOpcode() const { return OpcodeServer(_worldPacket.GetOpcode()); }
    };

    class WH_DISCORD_API ClientPacket : public Packet
    {
    public:
        ClientPacket(DiscordPacket&& packet);
        ClientPacket(OpcodeClient expectedOpcode, DiscordPacket&& packet);

        DiscordPacket const* Write() override final;

        [[nodiscard]] OpcodeClient GetOpcode() const { return OpcodeClient(_worldPacket.GetOpcode()); }
    };
}

#endif // PacketBaseDiscord_h__
