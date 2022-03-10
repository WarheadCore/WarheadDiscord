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

#ifndef _LOG_H_
#define _LOG_H_

#include "Define.h"
#include <unordered_map>
#include <fmt/format.h>
#include <fmt/color.h>

namespace spdlog::sinks
{
    class sink;
}

enum class LogLevel : uint8
{
    Trace,
    Debug,
    Info,
    Warning,
    Error,
    Critical,
    Disabled,

    Max
};

enum class SinkType : uint8
{
    Console,
    File,

    Max
};

enum class SinkOptions : uint8
{
    Type,
    Level,
    Pattern,
    Option1,
    Option2,
    Option3,
    Option4,

    Max
};

enum class ConsoleColor : uint16
{
    Black        = 0x0000,
    Red          = 0x0004,
    Green        = 0x0002,
    Brown        = 0x0006,
    Blue         = 0x0001,
    Magenta      = 0x0005,
    Cyan         = 0x0003,
    Gray         = 0x0007,
    DarkGray     = 0x0008,
    LightRed     = 0x000C,
    LightGreen   = 0x000A,
    Yellow       = 0x000E,
    LightBlue    = 0x0009,
    LightMagenta = 0x000D,
    LightCyan    = 0x000B,
    White        = 0x000F
};

enum class LoggerOptions : uint8
{
    LogLevel,
    SinkName,

    Max
};

class WH_COMMON_API Log
{
private:
    Log();
    ~Log();
    Log(Log const&) = delete;
    Log(Log&&) = delete;
    Log& operator=(Log const&) = delete;
    Log& operator=(Log&&) = delete;

public:
    static Log* instance();

    void Initialize();
    void LoadFromConfig();

    bool ShouldLog(std::string_view type, LogLevel level) const;

    template<typename... Args>
    inline void outMessage(std::string const& filter, LogLevel const level, const char* file, int line, const char* function, std::string_view fmt, Args&&... args)
    {
        Write(filter, level, file, line, function, fmt::format(fmt, std::forward<Args>(args)...));
    }

private:
    void Write(std::string_view filter, LogLevel const level, const char* file, int line, const char* function, std::string_view message);

    void CreateLoggerFromConfig(std::string const& configLoggerName);
    void CreateSinksFromConfig(std::string const& loggerSinkName);
    void ReadLoggersFromConfig();
    void ReadSinksFromConfig();

    void InitLogsDir();
    void Clear();
    uint16 GetColorCode(std::string_view colorName);

    std::string_view GetPositionOptions(std::string_view options, uint8 position, std::string_view _default = {});

    inline std::string_view GetPositionOptions(std::string_view options, SinkType position, std::string_view _default = {})
    {
        return GetPositionOptions(options, static_cast<uint8>(position), _default);
    }

    inline std::string_view GetPositionOptions(std::string_view options, SinkOptions position, std::string_view _default = {})
    {
        return GetPositionOptions(options, static_cast<uint8>(position), _default);
    }

    inline std::string_view GetPositionOptions(std::string_view options, LoggerOptions position, std::string_view _default = {})
    {
        return GetPositionOptions(options, static_cast<uint8>(position), _default);
    }

    std::string const GetChannelsFromLogger(std::string const& loggerName);

    std::string m_logsDir;
    LogLevel lowestLogLevel{ LogLevel::Critical };

    void AddSink(std::string const& sinkName, std::shared_ptr<spdlog::sinks::sink> sink);
    std::shared_ptr<spdlog::sinks::sink> GetSink(std::string const& sinkName);

    std::unordered_map<std::string, std::shared_ptr<spdlog::sinks::sink>> _sinkList;
};

#define sLog Log::instance()

#define LOG_EXCEPTION_FREE(filterType__, level__, ...) \
    { \
        try \
        { \
            sLog->outMessage(filterType__, level__, __FILE__, __LINE__, static_cast<const char *>(__FUNCTION__), fmt::format(__VA_ARGS__)); \
        } \
        catch (const std::exception& e) \
        { \
            sLog->outMessage("server", LogLevel::Error, __FILE__, __LINE__, static_cast<const char *>(__FUNCTION__), "Wrong format occurred ({}) at '{}:{}'", \
                e.what(), __FILE__, __LINE__); \
        } \
    }

#define LOG_MSG_BODY(filterType__, level__, ...)                        \
        do {                                                            \
            if (sLog->ShouldLog(filterType__, level__))                 \
                LOG_EXCEPTION_FREE(filterType__, level__, __VA_ARGS__); \
        } while (0)

#define LOG_CRIT(filterType__, ...) \
    LOG_MSG_BODY(filterType__, LogLevel::Critical, __VA_ARGS__)

#define LOG_ERROR(filterType__, ...) \
    LOG_MSG_BODY(filterType__, LogLevel::Error, __VA_ARGS__)

#define LOG_WARN(filterType__, ...)  \
    LOG_MSG_BODY(filterType__, LogLevel::Warning, __VA_ARGS__)

#define LOG_INFO(filterType__, ...)  \
    LOG_MSG_BODY(filterType__, LogLevel::Info, __VA_ARGS__)

#define LOG_DEBUG(filterType__, ...) \
    LOG_MSG_BODY(filterType__, LogLevel::Debug, __VA_ARGS__)

#define LOG_TRACE(filterType__, ...) \
    LOG_MSG_BODY(filterType__, LogLevel::Trace, __VA_ARGS__)

#define FMT_LOG_INFO(...) \
    fmt::print(fmt::emphasis::bold | fg(fmt::color::cyan), fmt::format(__VA_ARGS__) + "\n");

#define FMT_LOG_ERROR(...) \
    fmt::print(fmt::emphasis::bold | fg(fmt::color::red), fmt::format(__VA_ARGS__) + "\n");

#endif // _LOG_H__
