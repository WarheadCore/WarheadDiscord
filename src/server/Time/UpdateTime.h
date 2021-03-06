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

#ifndef __UPDATETIME_H
#define __UPDATETIME_H

#include "Define.h"
#include "Duration.h"
#include <array>
#include <string>

constexpr auto AVG_DIFF_COUNT = 1000;

class WH_SERVER_API UpdateTime
{
    using DiffTableArray = std::array<Milliseconds, AVG_DIFF_COUNT>;

public:    
    void Update(Milliseconds diff);

    inline Milliseconds GetLastUpdateTime() const { return _lastUpdateTime; }
    inline Milliseconds GetAverageUpdateTime() const { return _averageUpdateTime; }

protected:
    UpdateTime() = default;

private:
    DiffTableArray _updateTimeDataTable{};
    std::size_t _updateTimeTableIndex{ 0 };
    Milliseconds _lastUpdateTime{ 0ms };
    Milliseconds _averageUpdateTime{ 0ms };
};

class WH_SERVER_API DiscordUpdateTime : public UpdateTime
{
public:
    DiscordUpdateTime() : UpdateTime() { }
};

WH_SERVER_API extern DiscordUpdateTime sDiscordUpdateTime;

#endif
