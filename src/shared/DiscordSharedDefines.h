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

#ifndef __DISCORD_SHARED_DEFINES_H__
#define __DISCORD_SHARED_DEFINES_H__

#include "Define.h"

 /// List of Opcodes
enum DiscordCode : uint16
{
    // Client
    CLIENT_SEND_HELLO = 1,
    CLIENT_AUTH_SESSION,
    CLIENT_SEND_MESSAGE,
    CLIENT_SEND_MESSAGE_EMBED,

    // Server
    SERVER_SEND_AUTH_RESPONSE,

    NUM_MSG_TYPES
};

enum OpcodeMisc : uint16
{
    NUM_OPCODE_HANDLERS = NUM_MSG_TYPES,
    NULL_OPCODE = 0x0000
};

enum class DiscordAuthResponseCodes : uint8
{
    Ok,
    Failed,
    UnknownAccount,
    Banned,
    IncorrectPassword,
    ServerOffline
};

#endif // __DISCORD_SHARED_DEFINES_H__
