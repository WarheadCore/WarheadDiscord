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

#ifndef _OPCODES_H
#define _OPCODES_H

#include "Define.h"
#include <string>

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

using OpcodeClient = DiscordCode;
using OpcodeServer = DiscordCode;

class DiscordSession;
class DiscordPacket;

class OpcodeHandler
{
public:
    OpcodeHandler(std::string_view name) : Name(std::string(name)) { }
    virtual ~OpcodeHandler() = default;

    std::string Name;
};

class ClientOpcodeHandler : public OpcodeHandler
{
public:
    ClientOpcodeHandler(std::string_view name)
        : OpcodeHandler(name) { }

    virtual void Call(DiscordSession* session, DiscordPacket& packet) const = 0;
};

class ServerOpcodeHandler : public OpcodeHandler
{
public:
    ServerOpcodeHandler(std::string_view name)
        : OpcodeHandler(name) { }
};

class OpcodeTable
{
public:
    OpcodeTable();

    OpcodeTable(OpcodeTable const&) = delete;
    OpcodeTable& operator=(OpcodeTable const&) = delete;

    ~OpcodeTable();

    void Initialize();

    ClientOpcodeHandler const* operator[](DiscordCode index) const
    {
        return _internalTableClient[index];
    }

private:
    template<typename Handler, Handler HandlerFunction>
    void ValidateAndSetClientOpcode(OpcodeClient opcode, std::string_view name);

    void ValidateAndSetServerOpcode(OpcodeServer opcode, std::string_view name);

    ClientOpcodeHandler* _internalTableClient[NUM_OPCODE_HANDLERS];
};

extern OpcodeTable opcodeTable;

/// Lookup opcode name for human understandable logging
std::string GetOpcodeNameForLogging(DiscordCode opcode);

#endif
