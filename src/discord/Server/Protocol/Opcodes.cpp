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

#include "Opcodes.h"
#include "Log.h"
#include "AllPackets.h"
#include "DiscordSession.h"
#include <iomanip>
#include <sstream>

template<class PacketClass, void(DiscordSession::* HandlerFunction)(PacketClass&)>
class PacketHandler : public ClientOpcodeHandler
{
public:
    PacketHandler(std::string_view name) : ClientOpcodeHandler(name) { }

    void Call(DiscordSession* session, DiscordPacket& packet) const override
    {
        PacketClass nicePacket(std::move(packet));
        nicePacket.Read();
        (session->*HandlerFunction)(nicePacket);
    }
};

template<void(DiscordSession::* HandlerFunction)(DiscordPacket&)>
class PacketHandler<DiscordPacket, HandlerFunction> : public ClientOpcodeHandler
{
public:
    PacketHandler(std::string_view name) : ClientOpcodeHandler(name) { }

    void Call(DiscordSession* session, DiscordPacket& packet) const override
    {
        (session->*HandlerFunction)(packet);
    }
};

OpcodeTable opcodeTable;

template<typename T>
struct get_packet_class
{
};

template<typename PacketClass>
struct get_packet_class<void(DiscordSession::*)(PacketClass&)>
{
    using type = PacketClass;
};

OpcodeTable::OpcodeTable()
{
    memset(_internalTableClient, 0, sizeof(_internalTableClient));
}

OpcodeTable::~OpcodeTable()
{
    for (uint16 i = 0; i < NUM_OPCODE_HANDLERS; ++i)
        delete _internalTableClient[i];
}

template<typename Handler, Handler HandlerFunction>
void OpcodeTable::ValidateAndSetClientOpcode(OpcodeClient opcode, std::string_view name)
{
    if (uint32(opcode) == NULL_OPCODE)
    {
        LOG_ERROR("network", "Opcode {} does not have a value", name);
        return;
    }

    if (uint32(opcode) >= NUM_OPCODE_HANDLERS)
    {
        LOG_ERROR("network", "Tried to set handler for an invalid opcode {}", uint32(opcode));
        return;
    }

    if (_internalTableClient[opcode] != nullptr)
    {
        LOG_ERROR("network", "Tried to override client handler of {} with {} (opcode {})", opcodeTable[opcode]->Name, name, uint32(opcode));
        return;
    }

    _internalTableClient[opcode] = new PacketHandler<typename get_packet_class<Handler>::type, HandlerFunction>(name);
}

void OpcodeTable::ValidateAndSetServerOpcode(OpcodeServer opcode, std::string_view name)
{
    if (uint32(opcode) == NULL_OPCODE)
    {
        LOG_ERROR("network", "Opcode {} does not have a value", name);
        return;
    }

    if (uint32(opcode) >= NUM_OPCODE_HANDLERS)
    {
        LOG_ERROR("network", "Tried to set handler for an invalid opcode {}", uint32(opcode));
        return;
    }

    if (_internalTableClient[opcode] != nullptr)
    {
        LOG_ERROR("network", "Tried to override server handler of {} with {} (opcode {})", opcodeTable[opcode]->Name, name, uint32(opcode));
        return;
    }

    _internalTableClient[opcode] = new PacketHandler<DiscordPacket, &DiscordSession::Handle_ServerSide>(name);
}

/// Correspondence between opcodes and their names
void OpcodeTable::Initialize()
{
#define DEFINE_HANDLER(opcode, handler) \
    ValidateAndSetClientOpcode<decltype(handler), handler>(opcode, #opcode)

#define DEFINE_SERVER_OPCODE_HANDLER(opcode) \
    ValidateAndSetServerOpcode(opcode, #opcode)

    // Client
    DEFINE_HANDLER(CLIENT_SEND_HELLO,               &DiscordSession::HandleHelloOpcode);
    DEFINE_HANDLER(CLIENT_AUTH_SESSION,             &DiscordSession::Handle_EarlyProccess);
    DEFINE_HANDLER(CLIENT_SEND_MESSAGE,             &DiscordSession::HandleSendDiscordMessageOpcode);
    DEFINE_HANDLER(CLIENT_SEND_MESSAGE_EMBED,       &DiscordSession::HandleSendDiscordEmbedMessageOpcode);

    // Server
    DEFINE_SERVER_OPCODE_HANDLER(SERVER_SEND_AUTH_RESPONSE);

#undef DEFINE_HANDLER
#undef DEFINE_SERVER_OPCODE_HANDLER
}

template<typename T>
inline std::string GetOpcodeNameForLoggingImpl(T id)
{
    uint16 opcode = uint16(id);
    std::ostringstream ss;
    ss << '[';

    if (static_cast<uint16>(id) < NUM_OPCODE_HANDLERS)
    {
        if (OpcodeHandler const* handler = opcodeTable[id])
            ss << handler->Name;
        else
            ss << "UNKNOWN OPCODE";
    }
    else
        ss << "INVALID OPCODE";

    ss << " 0x" << std::hex << std::setw(4) << std::setfill('0') << std::uppercase << opcode << std::nouppercase << std::dec << " (" << opcode << ")]";
    return ss.str();
}

std::string GetOpcodeNameForLogging(DiscordCode opcode)
{
    return GetOpcodeNameForLoggingImpl(opcode);
}
