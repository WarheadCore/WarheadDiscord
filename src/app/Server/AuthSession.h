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

#ifndef __AUTHSESSION_H__
#define __AUTHSESSION_H__

#include "Socket.h"
#include "ByteBuffer.h"
#include <boost/asio/ip/tcp.hpp>
#include <memory>

class AuthSession : public Socket<AuthSession>
{
    using AuthSocket = Socket<AuthSession>;

public:
    AuthSession(boost::asio::ip::tcp::socket&& socket) :
        Socket(std::move(socket)) { }

    void Start() override;    
    bool Update() override;

    void SendPacket(ByteBuffer& packet);

protected:
    void ReadHandler() override;
    void OnClose() override;
};

#endif
