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

#include "AuthSession.h"
#include "Timer.h"
#include "ByteBuffer.h"
#include "Errors.h"
#include "StopWatch.h"
#include <map>

void AuthSession::Start()
{
    LOG_TRACE("auth", "Accepted connection from {}:{}", GetRemoteIpAddress().to_string(), GetRemotePort());

    AsyncRead();
}

void AuthSession::OnClose()
{
    LOG_TRACE("auth", "End connection from {}:{}", GetRemoteIpAddress().to_string(), GetRemotePort());
}

bool AuthSession::Update()
{
    if (!AuthSocket::Update())
        return false;

    return true;
}

void AuthSession::ReadHandler()
{
    MessageBuffer& packet = GetReadBuffer();
    ByteBuffer buffer(std::move(MessageBuffer(packet)));

    packet.Reset();

    AsyncRead();
}

void AuthSession::SendPacket(ByteBuffer& packet)
{
    if (!IsOpen())
    {
        LOG_ERROR("auth", "> Can't send packet. Socket is close");
        return;
    }

    if (packet.empty())
    {
        LOG_ERROR("auth", "> Can't send packet. Packet is empty");
        return;
    }

    MessageBuffer buffer(packet.size());
    buffer.Write(packet.contents(), packet.size());
    QueuePacket(std::move(buffer));
}
