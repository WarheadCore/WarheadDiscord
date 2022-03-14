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

#include "PacketUtilities.h"
#include "Errors.h"
#include <sstream>
#include <utf8.h>

DiscordPackets::InvalidStringValueException::InvalidStringValueException(std::string const& value) :
    ByteBufferInvalidValueException("string", value.c_str()) { }

DiscordPackets::InvalidUtf8ValueException::InvalidUtf8ValueException(std::string const& value) :
    InvalidStringValueException(value) { }

bool DiscordPackets::Strings::Utf8::Validate(std::string const& value)
{
    if (!utf8::is_valid(value.begin(), value.end()))
        throw InvalidUtf8ValueException(value);

    return true;
}

DiscordPackets::PacketArrayMaxCapacityException::PacketArrayMaxCapacityException(std::size_t requestedSize, std::size_t sizeLimit)
{
    std::ostringstream builder;
    builder << "Attempted to read more array elements from packet " << requestedSize << " than allowed " << sizeLimit;
    message().assign(builder.str());
}

void DiscordPackets::CheckCompactArrayMaskOverflow(std::size_t index, std::size_t limit)
{
    ASSERT(index < limit, "Attempted to insert {} values into CompactArray but it can only hold {}", index, limit);
}
