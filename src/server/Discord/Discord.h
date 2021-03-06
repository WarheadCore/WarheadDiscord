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

#ifndef _DISCORD_H_
#define _DISCORD_H_

#include "DatabaseEnvFwd.h"
#include "Define.h"
#include "DiscordSharedDefines.h"
#include "TaskScheduler.h"
#include "Timer.h"
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
class WH_SERVER_API Discord
{
public:
    Discord();
    ~Discord();

    static Discord* instance();

    static uint32 _loopCounter;

    DiscordSession* FindSession(uint32 id) const;

    void AddSession(DiscordSession* session);
    void KickSession(uint32 id);

    /// Get the number of current active sessions
    void UpdateMaxSessionCounters();
    [[nodiscard]] const auto& GetAllSessions() const { return _sessions; }
    [[nodiscard]] uint32 GetActiveSessionCount() const { return _sessions.size(); }

    /// Get number of sessions
    [[nodiscard]] inline uint32 GetSessionCount() const { return _sessionCount; }
    [[nodiscard]] inline uint32 GetMaxSessionCount() const { return _maxSessionCount; }

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

    void SetInitialDiscordSettings();
    void LoadConfigSettings();

    /// Are we in the middle of a shutdown?
    [[nodiscard]] bool IsShuttingDown() const { return _shutdownTimer > 0s; }
    [[nodiscard]] Seconds GetShutDownTimeLeft() const { return _shutdownTimer; }
    void ShutdownServ(Seconds time, uint8 exitcode, const std::string_view reason = {});
    void ShutdownCancel();
    void ShutdownMsg(bool show = false, const std::string_view reason = {});
    static uint8 GetExitCode() { return _exitCode; }
    static void StopNow(uint8 exitcode) { _stopEvent = true; _exitCode = exitcode; }
    static bool IsStopped() { return _stopEvent; }

    void Update(Milliseconds diff);
    void UpdateSessions();

    void KickAll();

private:
    void _UpdateGameTime();

    static std::atomic<bool> _stopEvent;
    static uint8 _exitCode;
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
