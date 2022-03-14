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

#ifndef __WORLD_H
#define __WORLD_H

#include "Define.h"
#include "DatabaseEnvFwd.h"
#include "LockedQueue.h"
#include "Timer.h"
#include "TaskScheduler.h"
#include <atomic>
#include <unordered_map>

class DiscordPacket;
class DiscordSocket;
class DiscordSession;

enum ShutdownExitCode
{
    SHUTDOWN_EXIT_CODE = 0,
    ERROR_EXIT_CODE    = 1,
    RESTART_EXIT_CODE  = 2,
};

/// The Discord
class WH_DISCORD_API Discord
{
public:
    Discord();
    ~Discord();

    static Discord* instance();

    static uint32 m_worldLoopCounter;

    DiscordSession* FindSession(uint32 id) const;

    void AddSession(DiscordSession* s);
    bool KickSession(uint32 id);

    /// Get the number of current active sessions
    void UpdateMaxSessionCounters();
    [[nodiscard]] const auto& GetAllSessions() const { return _sessions; }
    [[nodiscard]] uint32 GetActiveSessionCount() const { return _sessions.size(); }

    /// Get number of players
    [[nodiscard]] inline uint32 GetPlayerCount() const { return _sessionCount; }
    [[nodiscard]] inline uint32 GetMaxPlayerCount() const { return _maxSessionCount; }

    /// Increase/Decrease number of players
    inline void IncreaseSessionCount()
    {
        _sessionCount++;
        _maxSessionCount = std::max(_maxSessionCount, _sessionCount);
    }

    inline void DecreaseSessionCount() { _sessionCount--; }

    /// Deny clients?
    [[nodiscard]] bool IsClosed() const;

    /// Close
    void SetClosed(bool val);

    //bool HasRecentlyDisconnected(DiscordSession*);

    void SetInitialDiscordSettings();
    void LoadConfigSettings();

    /// Are we in the middle of a shutdown?
    [[nodiscard]] bool IsShuttingDown() const { return _shutdownTimer > 0s; }
    [[nodiscard]] Seconds GetShutDownTimeLeft() const { return _shutdownTimer; }
    void ShutdownServ(Seconds time, uint8 exitcode, const std::string_view reason = {});
    void ShutdownCancel();
    void ShutdownMsg(bool show = false, const std::string_view reason = {});
    static uint8 GetExitCode() { return m_ExitCode; }
    static void StopNow(uint8 exitcode) { m_stopEvent = true; m_ExitCode = exitcode; }
    static bool IsStopped() { return m_stopEvent; }

    void Update(uint32 diff);
    void UpdateSessions(uint32 diff);

    void KickAll();

private:
    void _UpdateGameTime();

    static std::atomic<bool> m_stopEvent;
    static uint8 m_ExitCode;
    Seconds _shutdownTimer;

    bool m_isClosed;

    std::unordered_map<uint32, DiscordSession*> _sessions;
    std::size_t m_maxActiveSessionCount;
    uint32 _sessionCount;
    uint32 _maxSessionCount;

    TaskScheduler _scheduler;
};

#define sDiscord Discord::instance()

#endif
