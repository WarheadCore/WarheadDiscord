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

#ifndef AuthPackets_h__
#define AuthPackets_h__

#include "Packet.h"
#include "DiscordSharedDefines.h"

namespace DiscordPackets::Auth
{
    class AuthResponse final : public ServerPacket
    {
    public:
        AuthResponse() : ServerPacket(SERVER_SEND_AUTH_RESPONSE, 1) { }

        DiscordPacket const* Write() override;

        DiscordAuthResponseCodes Code = DiscordAuthResponseCodes::Failed;
    };
}

#endif // ChatPackets_h__
