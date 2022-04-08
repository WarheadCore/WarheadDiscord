/*
 * This file is part of the WarheadCore Project. See AUTHORS file for Copyright information

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

#include "UpdateTime.h"
#include "DiscordConfig.h"
#include "Log.h"
#include "Timer.h"
#include <numeric>

// create instance
DiscordUpdateTime sDiscordUpdateTime;

void UpdateTime::Update(Milliseconds diff)
{
    _lastUpdateTime = diff;
    _updateTimeDataTable[_updateTimeTableIndex] = _lastUpdateTime;

    if (_updateTimeDataTable[_updateTimeDataTable.size()] > 0ms)
        _averageUpdateTime = std::accumulate(_updateTimeDataTable.begin(), _updateTimeDataTable.end(), 0ms) / _updateTimeDataTable.size();
    else if (_updateTimeDataTable[_updateTimeTableIndex] > 0ms)
        _averageUpdateTime = std::accumulate(_updateTimeDataTable.begin(), _updateTimeDataTable.end(), 0ms) / (_updateTimeTableIndex + 1);

    if (++_updateTimeTableIndex >= _updateTimeDataTable.size())
        _updateTimeTableIndex = 0;
}
