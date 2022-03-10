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

#ifndef _STOPWATCH_H_
#define _STOPWATCH_H_

#include "Timer.h"
#include <fmt/core.h>

class StopWatch
{
    using clock = std::chrono::steady_clock;

public:
    StopWatch()
        : _startTime{ clock::now() } {}

    Microseconds elapsed() const
    {
        return std::chrono::duration_cast<Microseconds>(clock::now() - _startTime);
    }

    void reset()
    {
        _startTime = clock::now();
    }

private:
    std::chrono::time_point<clock> _startTime;
};

namespace fmt
{
    template<>
    struct formatter<StopWatch> : formatter<string_view>
    {
        template<typename FormatContext>
        auto format(const StopWatch& sw, FormatContext& ctx) -> decltype(ctx.out())
        {
            return formatter<string_view>::format(Warhead::Time::ToTimeString(sw.elapsed()), ctx);
        }
    };
}

#endif
