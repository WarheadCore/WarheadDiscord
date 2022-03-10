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

#include "Log.h"
#include "Config.h"
#include "StringConvert.h"
#include "Tokenize.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>

namespace
{
    // Const loggers name
    constexpr auto LOGGER_ROOT = "root";

    // Prefix's
    constexpr auto PREFIX_LOGGER = "Logger.";
    constexpr auto PREFIX_SINK = "Sink.";
    constexpr auto PREFIX_LOGGER_LENGTH = 7;
    constexpr auto PREFIX_SINK_LENGTH = 5;

    std::shared_ptr<spdlog::logger> GetLoggerByType(std::string_view type)
    {
        if (auto logger = spdlog::get(std::string(type)))
            return logger;

        if (type == LOGGER_ROOT)
            return nullptr;

        std::string parentLogger = LOGGER_ROOT;
        size_t found = type.find_last_of('.');

        if (found != std::string::npos)
            parentLogger = type.substr(0, found);

        return GetLoggerByType(parentLogger);
    }
}

Log::Log()
{
    Clear();
}

Log::~Log()
{
    Clear();
}

Log* Log::instance()
{
    static Log instance;
    return &instance;
}

void Log::Clear()
{
    // Clear all loggers
    spdlog::shutdown();

    // Clear sink list
    _sinkList.clear();
}

void Log::Initialize()
{
    LoadFromConfig();
}

void Log::LoadFromConfig()
{
    lowestLogLevel = LogLevel::Critical;

    Clear();
    InitLogsDir();
    ReadSinksFromConfig();
    ReadLoggersFromConfig();

    // Clear sink list
    _sinkList.clear();
}

void Log::InitLogsDir()
{
    m_logsDir = sConfigMgr->GetOption<std::string>("LogsDir", "", false);

    if (!m_logsDir.empty())
        if ((m_logsDir.at(m_logsDir.length() - 1) != '/') && (m_logsDir.at(m_logsDir.length() - 1) != '\\'))
            m_logsDir.push_back('/');
}

void Log::ReadLoggersFromConfig()
{
    auto const& keys = sConfigMgr->GetKeysByString(PREFIX_LOGGER);
    if (!keys.size())
    {
        FMT_LOG_ERROR("Log::ReadLoggersFromConfig - Not found loggers, change config file!");
        return;
    }

    for (auto const& loggerName : keys)
        CreateLoggerFromConfig(loggerName);

    if (!spdlog::get(LOGGER_ROOT))
        FMT_LOG_ERROR("Log::ReadLoggersFromConfig - Logger '{0}' not found!\nPlease change or add 'Logger.{0}' option in config file!", LOGGER_ROOT);
}

void Log::ReadSinksFromConfig()
{
    auto const& keys = sConfigMgr->GetKeysByString(PREFIX_SINK);
    if (keys.empty())
    {
        FMT_LOG_ERROR("Log::ReadChannelsFromConfig - Not found sinks, change config file!");
        return;
    }

    for (auto const& sinkName : keys)
        CreateSinksFromConfig(sinkName);
}

std::string_view Log::GetPositionOptions(std::string_view options, uint8 position, std::string_view _default /*= {}*/)
{
    auto const& tokens = Warhead::Tokenize(options, ',', true);
    if (static_cast<uint8>(tokens.size()) < position + 1u)
        return _default;

    return tokens.at(position);
}

bool Log::ShouldLog(std::string_view type, LogLevel level) const
{
    // TODO: Use cache to store "Type.sub1.sub2": "Type" equivalence, should
    // Speed up in cases where requesting "Type.sub1.sub2" but only configured
    // Logger "Type"

    // Don't even look for a logger if the LogLevel is lower than lowest log levels across all loggers
    if (level < lowestLogLevel)
        return false;

    auto logger = GetLoggerByType(type);
    if (!logger)
        return false;

    LogLevel logLevel = LogLevel(logger->level());
    return logLevel != LogLevel::Disabled && logLevel <= level;
}

std::string const Log::GetChannelsFromLogger(std::string const& loggerName)
{
    std::string const& loggerOptions = sConfigMgr->GetOption<std::string>(PREFIX_LOGGER + loggerName, "2, Console Server", false);

    auto const& tokensOptions = Warhead::Tokenize(loggerOptions, ',', true);
    if (tokensOptions.empty())
        return "";

    return std::string(tokensOptions.at(static_cast<uint8>(LoggerOptions::SinkName)));
}

void Log::CreateLoggerFromConfig(std::string const& configLoggerName)
{
    if (configLoggerName.empty())
        return;

    std::string const& options = sConfigMgr->GetOption<std::string>(configLoggerName, "", false);
    std::string loggerName = configLoggerName.substr(PREFIX_LOGGER_LENGTH);

    if (options.empty())
    {
        FMT_LOG_ERROR("Log::CreateLoggerFromConfig: Missing config option Logger.{}", loggerName);
        return;
    }

    auto const& tokens = Warhead::Tokenize(options, ',', true);
    if (!tokens.size() || tokens.size() < static_cast<size_t>(LoggerOptions::Max))
    {
        FMT_LOG_ERROR("Log::CreateLoggerFromConfig: Bad config options for Logger ({})", loggerName);
        return;
    }

    LogLevel level = static_cast<LogLevel>(Warhead::StringTo<uint8>(GetPositionOptions(options, LoggerOptions::LogLevel)).value_or(static_cast<uint8>(LogLevel::Max)));
    if (level >= LogLevel::Max)
    {
        FMT_LOG_ERROR("Log::CreateLoggerFromConfig: Wrong Log Level for logger {}", loggerName);
        return;
    }

    if (level < lowestLogLevel)
        lowestLogLevel = level;

    std::vector<spdlog::sink_ptr> sinkList;

    auto const& sinksName = GetPositionOptions(options, LoggerOptions::SinkName);

    for (auto const& sinkName : Warhead::Tokenize(sinksName, ' ', false))
    {
        auto loggerSink = GetSink(std::string(sinkName));
        if (!loggerSink)
            continue;

        sinkList.emplace_back(std::move(loggerSink));
    }

    try
    {
        auto logger = std::make_shared<spdlog::logger>(loggerName);
        logger->set_level(static_cast<spdlog::level::level_enum>(level));
        logger->sinks().swap(sinkList);
        logger->flush_on(spdlog::level::level_enum(LogLevel::Error));
        spdlog::register_logger(logger);
    }
    catch (const std::exception& e)
    {
        FMT_LOG_ERROR("Log::CreateLogger - {}", e.what());
    }
}

void Log::CreateSinksFromConfig(std::string const& loggerSinkName)
{
    if (loggerSinkName.empty())
        return;

    std::string const& options = sConfigMgr->GetOption<std::string>(loggerSinkName, "", false);
    std::string const& sinkName = loggerSinkName.substr(PREFIX_SINK_LENGTH);

    if (options.empty())
    {
        FMT_LOG_ERROR("{}: Missing config option Sink.{}", __FUNCTION__, sinkName);
        return;
    }

    auto const& tokens = Warhead::Tokenize(options, ',', true);
    if (tokens.size() < static_cast<size_t>(SinkOptions::Pattern) + 1 || tokens.size() > static_cast<size_t>(SinkOptions::Max))
    {
        FMT_LOG_ERROR("{}: Wrong config option Sink.{}={}", __FUNCTION__, sinkName, options);
        return;
    }

    auto channelType = Warhead::StringTo<uint8>(GetPositionOptions(options, SinkOptions::Type));
    if (!channelType || channelType >= static_cast<uint8>(SinkType::Max))
    {
        FMT_LOG_ERROR("{}: Wrong sink type for Sink.{}\n", __FUNCTION__, sinkName);
        return;
    }

    LogLevel level = static_cast<LogLevel>(Warhead::StringTo<uint8>(GetPositionOptions(options, SinkOptions::Level)).value_or(static_cast<uint8>(LogLevel::Max)));
    if (level >= LogLevel::Max)
    {
        FMT_LOG_ERROR("{}: Wrong Log Level for Sink.{}", __FUNCTION__, sinkName);
        return;
    }

    auto pattern = GetPositionOptions(options, SinkOptions::Pattern);
    if (pattern.empty())
    {
        FMT_LOG_ERROR("{}: Empty pattern for Sink.{}", __FUNCTION__, sinkName);
        return;
    }

    if (channelType.value() == static_cast<uint8>(SinkType::Console))
    {
        try
        {
            // Configuration console channel
            auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            consoleSink->set_level(spdlog::level::level_enum(level));
            consoleSink->set_pattern(std::string(pattern));

            // Init Colors
            auto colorOptions = GetPositionOptions(options, SinkOptions::Option1);

            if (!colorOptions.empty())
            {
                auto const& tokensColor = Warhead::Tokenize(colorOptions, ' ', false);
                if (tokensColor.size() == static_cast<uint64>(LogLevel::Max) - 1)
                {
                    consoleSink->set_color(spdlog::level::level_enum::trace, GetColorCode(tokensColor[0]));
                    consoleSink->set_color(spdlog::level::level_enum::debug, GetColorCode(tokensColor[1]));
                    consoleSink->set_color(spdlog::level::level_enum::info, GetColorCode(tokensColor[2]));
                    consoleSink->set_color(spdlog::level::level_enum::warn, GetColorCode(tokensColor[3]));
                    consoleSink->set_color(spdlog::level::level_enum::err, GetColorCode(tokensColor[4]));
                    consoleSink->set_color(spdlog::level::level_enum::critical, GetColorCode(tokensColor[5]));
                }
            }

            AddSink(sinkName, consoleSink);
        }
        catch (const spdlog::spdlog_ex& ex)
        {
            FMT_LOG_ERROR("{}: Sink init failed: {}", __FUNCTION__, ex.what());
        }
    }
    else if (channelType.value() == static_cast<uint8>(SinkType::File))
    {
        if (tokens.size() < static_cast<uint64>(SinkOptions::Option1) + 1)
            ABORT("Bad file name for Sink.{}", sinkName);

        auto fileName = GetPositionOptions(options, SinkOptions::Option1);
        auto _rotateOnOpen = GetPositionOptions(options, SinkOptions::Option2);
        auto _maxFilesSize = GetPositionOptions(options, SinkOptions::Option3);
        auto _maxFiles = GetPositionOptions(options, SinkOptions::Option4);

        auto rotateOnOpen = Warhead::StringTo<bool>(_rotateOnOpen);
        if (!rotateOnOpen)
            ABORT("Bad value for option 'Rotate on open' - '{}' in Sink.{}", _rotateOnOpen, sinkName);

        auto maxFilesSize = Warhead::StringTo<uint64>(_maxFilesSize);
        if (!maxFilesSize)
            ABORT("Bad value for option 'Max files size' - '{}' in Sink.{}", _maxFilesSize, sinkName);

        auto maxFiles = Warhead::StringTo<uint64>(_maxFiles);
        if (!maxFiles)
            ABORT("Bad value for option 'Max files' - '{}' in Sink.{}", _maxFiles, sinkName);

        try
        {
            // Configuration file channel
            auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(m_logsDir + std::string(fileName), *maxFilesSize * 1048576, *maxFiles, *rotateOnOpen);
            fileSink->set_level(spdlog::level::level_enum(level));
            fileSink->set_pattern(std::string(pattern));

            AddSink(sinkName, fileSink);
        }
        catch (const spdlog::spdlog_ex& ex)
        {
            FMT_LOG_ERROR("{}: Sink init failed: {}", __FUNCTION__, ex.what());
        }
    }
    else
        ABORT("{}: Invalid channel type ({})", __FUNCTION__, *channelType);
}

void Log::Write(std::string_view filter, LogLevel const level, const char* file, int line, const char* function, std::string_view message)
{
    auto logger = GetLoggerByType(filter);
    if (!logger)
        return;

    logger->log(spdlog::source_loc{ file, line, function }, spdlog::level::level_enum(level), message);
}

void Log::AddSink(std::string const& sinkName, std::shared_ptr<spdlog::sinks::sink> sink)
{
    auto const& itr = _sinkList.find(sinkName);
    if (itr != _sinkList.end())
    {
        FMT_LOG_ERROR("> {}: Sink with name '{}' is exist!. Skip add", __FUNCTION__, sinkName);
        return;
    }

    _sinkList.emplace(sinkName, std::move(sink));
}

std::shared_ptr<spdlog::sinks::sink> Log::GetSink(std::string const& sinkName)
{
    auto const& itr = _sinkList.find(sinkName);
    if (itr == _sinkList.end())
    {
        FMT_LOG_ERROR("> {}: Sink with name '{}' is don't exist!", __FUNCTION__, sinkName);
        return nullptr;
    }

    return std::move(itr->second);
}

uint16 Log::GetColorCode(std::string_view colorName)
{
    if (StringEqualI(colorName, "black"))
        return static_cast<uint16>(ConsoleColor::Black);
    else if (StringEqualI(colorName, "red"))
        return static_cast<uint16>(ConsoleColor::Red);
    else if (StringEqualI(colorName, "green"))
        return static_cast<uint16>(ConsoleColor::Green);
    else if (StringEqualI(colorName, "brown"))
        return static_cast<uint16>(ConsoleColor::Brown);
    else if (StringEqualI(colorName, "blue"))
        return static_cast<uint16>(ConsoleColor::Blue);
    else if (StringEqualI(colorName, "magenta"))
        return static_cast<uint16>(ConsoleColor::Magenta);
    else if (StringEqualI(colorName, "cyan"))
        return static_cast<uint16>(ConsoleColor::Cyan);
    else if (StringEqualI(colorName, "gray"))
        return static_cast<uint16>(ConsoleColor::Gray);
    else if (StringEqualI(colorName, "darkGray"))
        return static_cast<uint16>(ConsoleColor::DarkGray);
    else if (StringEqualI(colorName, "lightRed"))
        return static_cast<uint16>(ConsoleColor::LightRed);
    else if (StringEqualI(colorName, "lightGreen"))
        return static_cast<uint16>(ConsoleColor::LightGreen);
    else if (StringEqualI(colorName, "yellow"))
        return static_cast<uint16>(ConsoleColor::Yellow);
    else if (StringEqualI(colorName, "lightBlue"))
        return static_cast<uint16>(ConsoleColor::LightBlue);
    else if (StringEqualI(colorName, "lightMagenta"))
        return static_cast<uint16>(ConsoleColor::LightMagenta);
    else if (StringEqualI(colorName, "lightCyan"))
        return static_cast<uint16>(ConsoleColor::LightCyan);
    else if (StringEqualI(colorName, "white"))
        return static_cast<uint16>(ConsoleColor::White);

    FMT_LOG_ERROR("> Invalid color '{}'", colorName);

    return static_cast<uint16>(ConsoleColor::White);
}
