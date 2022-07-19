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

#include "DiscordBot.h"
#include "AsyncCallbackMgr.h"
#include "AccountMgr.h"
#include "ChatCommandHandler.h"
#include "DatabaseEnv.h"
#include "Discord.h"
#include "DiscordConfig.h"
#include "GameTime.h"
#include "GitRevision.h"
#include "Log.h"
#include "StopWatch.h"
#include "StringConvert.h"
#include "Tokenize.h"
#include "UpdateTime.h"
#include "TaskScheduler.h"
#include <dpp/dpp.h>

namespace
{
    constexpr auto DEFAULT_CATEGORY_NAME = "core-logs";

    // General
    constexpr auto CHANNEL_NAME_SERVER_STATUS = "server-status";
    constexpr auto CHANNEL_NAME_COMMANDS = "commands";

    // Chat logs
    constexpr auto CHANNEL_NAME_CHAT_SAY = "chat-say";
    constexpr auto CHANNEL_NAME_CHAT_CHANNEL = "chat-channel";

    // Login
    constexpr auto CHANNEL_NAME_LOGIN_PLAYER = "login-player";
    constexpr auto CHANNEL_NAME_LOGIN_GM = "login-gm";
    constexpr auto CHANNEL_NAME_LOGIN_ADMIN = "login-admin";

    // Owner
    constexpr auto OWNER_ID = 365169287926906883; // Winfidonarleyan | <@365169287926906883>
    constexpr auto OWNER_MENTION = "<@365169287926906883>";

    // Warhead guild
    constexpr auto WARHEAD_GUILD_ID = 572275951879192588;
    constexpr auto LOGS_CHANNEL_GUILD_ADD_ID = 981904498991714324;
    constexpr auto LOGS_CHANNEL_GUILD_DELETE_ID = 981904522546933760;
    constexpr auto LOGS_CHANNEL_MEMBERS_UPDATE_ID = 981906826436173935;
    constexpr auto LOGS_CHANNEL_OTHER_ID = 981628140772270160;

    constexpr DiscordChannelType GetDiscordChannelType(std::string_view channelName)
    {
        if (channelName == CHANNEL_NAME_SERVER_STATUS)
            return DiscordChannelType::ServerStatus;

        if (channelName == CHANNEL_NAME_COMMANDS)
            return DiscordChannelType::Commands;

        if (channelName == CHANNEL_NAME_CHAT_SAY)
            return DiscordChannelType::ChatSay;

        if (channelName == CHANNEL_NAME_CHAT_CHANNEL)
            return DiscordChannelType::ChatChannel;

        if (channelName == CHANNEL_NAME_LOGIN_PLAYER)
            return DiscordChannelType::LoginPlayer;

        if (channelName == CHANNEL_NAME_LOGIN_GM)
            return DiscordChannelType::LoginGM;

        if (channelName == CHANNEL_NAME_LOGIN_ADMIN)
            return DiscordChannelType::LoginAdmin;

        return DiscordChannelType::MaxType;
    }

    std::string GetChannelName(DiscordChannelType type)
    {
        switch (type)
        {
        case DiscordChannelType::ServerStatus:
            return CHANNEL_NAME_SERVER_STATUS;
        case DiscordChannelType::Commands:
            return CHANNEL_NAME_COMMANDS;
        case DiscordChannelType::ChatSay:
            return CHANNEL_NAME_CHAT_SAY;
        case DiscordChannelType::ChatChannel:
            return CHANNEL_NAME_CHAT_CHANNEL;
        case DiscordChannelType::LoginPlayer:
            return CHANNEL_NAME_LOGIN_PLAYER;
        case DiscordChannelType::LoginGM:
            return CHANNEL_NAME_LOGIN_GM;
        case DiscordChannelType::LoginAdmin:
            return CHANNEL_NAME_LOGIN_ADMIN;
        default:
            return "";
        }
    }
}

DiscordBot* DiscordBot::instance()
{
    static DiscordBot instance;
    return &instance;
}

void DiscordBot::Start()
{
    _isEnable = CONF_GET_BOOL("Discord.Bot.Enable");

    if (!_isEnable)
        return;

    std::string botToken = CONF_GET_STR("Discord.Bot.Token");
    if (botToken.empty())
    {
        LOG_FATAL("discord", "> Empty bot token for discord. Disable system");
        _isEnable = false;
        return;
    }

    _bot = std::make_unique<dpp::cluster>(botToken, dpp::i_all_intents);
    _scheduler = std::make_unique<TaskScheduler>();

    // Load clients from DB
    LoadClients();

    // Prepare logs
    ConfigureLogs();

    // Prepare commands
    ConfigureCommands();

    _bot->start(true);

    // Prepare hooks
    CheckClients();
}

void DiscordBot::Update(Milliseconds diff)
{
    if (_scheduler)
        _scheduler->Update(diff);
}

void DiscordBot::CheckClients()
{
    sAsyncCallbackMgr->AddAsyncCallback([this]()
    {
        auto const& guilds = _bot->current_user_get_guilds_sync();
        if (guilds.empty())
        {
            DeleteAllClients();
            return;
        }

        std::vector<uint64> _saveClientIds;

        for (auto const& itr : _guilds)
            _saveClientIds.emplace_back(itr.first);

        for (auto const& [guildID, guild] : guilds)
        {
            if (HasClient(guildID))
            {
                // Create commands for existing guild
                CreateCommands(guildID);

                std::erase(_saveClientIds, guildID);
                continue;
            }

            AddClient(guild.id, guild.name, guild.member_count, GameTime::GetGameTime(), true);
            LogAddClient(guild.id, DiscordMessageColor::Orange, guild.name, guild.get_icon_url(), guild.member_count, guild.get_creation_time());
        }

        if (!_saveClientIds.empty())
        {
            for (auto const& guildID : _saveClientIds)
            {
                auto clientInfo = GetClient(guildID);
                if (!clientInfo)
                {
                    LOG_FATAL("discord", "> Unknown client {}", guildID);
                    continue;
                }

                LogDeleteClient(guildID, DiscordMessageColor::Orange, "", clientInfo->GuildName);
                DeleteClient(guildID);
            }
        }

        ConfigureGuildInviteHooks();
    });
}

void DiscordBot::SendDefaultMessage(int64 channelID, std::string_view message)
{
    if (!_isEnable)
        return;

    dpp::message discordMessage;
    discordMessage.channel_id = channelID;
    discordMessage.content = std::string(message);

    _bot->message_create(discordMessage);
}

void DiscordBot::SendEmbedMessage(int64 channelID, std::shared_ptr<dpp::embed> embed)
{
    if (!_isEnable || !embed)
        return;

    _bot->message_create(dpp::message(channelID, *embed));
}

void DiscordBot::ConfigureLogs()
{
    if (!_isEnable)
        return;

    _bot->on_ready([this](dpp::ready_t const&)
    {
        LOG_INFO("discord.bot", "> DiscordBot: Logged in as {}", _bot->me.username);
    });

    //_bot->on_log(dpp::utility::cout_logger());

    _bot->on_log([](const dpp::log_t& event)
    {
        switch (event.severity)
        {
        case dpp::ll_trace:
            LOG_TRACE("discord.bot", "> DiscordBot: {}", event.message);
            break;
        case dpp::ll_debug:
            LOG_DEBUG("discord.bot", "> DiscordBot: {}", event.message);
            break;
        case dpp::ll_info:
            LOG_INFO("discord.bot", "> DiscordBot: {}", event.message);
            break;
        case dpp::ll_warning:
            LOG_WARN("discord.bot", "> DiscordBot: {}", event.message);
            break;
        case dpp::ll_error:
            LOG_ERROR("discord.bot", "> DiscordBot: {}", event.message);
            break;
        case dpp::ll_critical:
            LOG_CRIT("discord.bot", "> DiscordBot: {}", event.message);
            break;
        default:
            break;
        }
    });
}

void DiscordBot::ConfigureCommands()
{
    if (!_isEnable)
        return;

    _bot->on_ready([this](dpp::ready_t const&)
    {
        sCommandHandler->SetApplicaionID(_bot.get());
    });

    sCommandHandler->AddCommand("account-create", "Создать аккаунт",
    {
        {  dpp::command_option(dpp::co_string, "name", "Название аккаунта", true), {} },

        {
            dpp::command_option(dpp::co_string, "сore_name", "Название используемого ядра", true),
            {
                dpp::command_option_choice("WarheadCore", std::string("WarheadCore")),
                dpp::command_option_choice("AzerothCore", std::string("AzerothCore"))
            },
        }
    },
    [this](dpp::slashcommand_t const& slashCommandEvent)
    {
        dpp::command_interaction cmdData = slashCommandEvent.command.get_command_interaction();
        if (cmdData.options.empty())
            return;

        auto const guildID = slashCommandEvent.command.guild_id;
        auto const userID = slashCommandEvent.command.member.user_id;

        LOG_DEBUG("discord", "> Using account create command. GuildID {}. UserID {}", guildID, userID);

        std::string accountName = std::get<std::string>(cmdData.options[0].value);
        std::string coreName = std::get<std::string>(cmdData.options[1].value);

        dpp::message errorMessage;
        errorMessage.set_flags(dpp::m_ephemeral);

        if (guildID == WARHEAD_GUILD_ID && userID != OWNER_ID)
        {
            errorMessage.set_content("Вы не можете создать аккаунт в этой гильдии, пригласите бота на свой сервер и создайте аккаунт там");
            slashCommandEvent.reply(errorMessage);
            return;
        }

        if (HasClient(guildID) && userID != OWNER_ID)
        {
            errorMessage.set_content("Эмм, вы уже создали аккаунт для этой гильдии");
            slashCommandEvent.reply(errorMessage);
            return;
        }

        std::string key = sAccountMgr->GetRandomKey();

        auto result = sAccountMgr->CreateAccount(accountName, key, guildID);
        if (result != AccountResponceResult::Ok)
        {
            std::string errorMsg;

            switch (result)
            {
            case AccountResponceResult::LongName:
                errorMsg = Warhead::StringFormat("Имя аккаунта слишком большое. Максимальная длина `{}`. Вы использовали `{}`", MAX_ACCOUNT_STR, accountName.length());
                break;
            case AccountResponceResult::NameAlreadyExist:
                errorMsg = Warhead::StringFormat("Аккаунт `{}` уже существует", accountName);
                break;
            default:
                errorMsg = "Неизвестная ошибка";
                break;
            }

            errorMessage.set_content(Warhead::StringFormat("Ошибка при создании аккаунта. {}", errorMsg));
            slashCommandEvent.reply(errorMessage);
            return;
        }

        dpp::embed embed;
        embed.set_color(static_cast<uint32>(DiscordMessageColor::Teal));
        embed.set_title("Создание аккаунта для Warhead Discord");
        embed.set_description(Warhead::StringFormat("Вы успешно создали аккаунт: `{}`", accountName));
        embed.add_field("Ваш ключ. Сохраните его!", Warhead::StringFormat("`{}`", key));
        embed.set_timestamp(GameTime::GetGameTime().count());
        embed.set_footer(Warhead::StringFormat("Guild ID: {}. Guild locale: {}", guildID, slashCommandEvent.command.guild_locale), "");

        dpp::message message;
        message.add_embed(embed);
        message.set_flags(dpp::m_ephemeral);
        slashCommandEvent.reply(message);
    });

    sCommandHandler->AddCommand("account-key-generate", "Создать новый ключ",
    {
        {  dpp::command_option(dpp::co_string, "name", "Название аккаунта", true), {} },
    },
    [this](dpp::slashcommand_t const& slashCommandEvent)
    {
        dpp::command_interaction cmdData = slashCommandEvent.command.get_command_interaction();
        if (cmdData.options.empty())
            return;

        auto const guildID = slashCommandEvent.command.guild_id;
        auto const userID = slashCommandEvent.command.member.user_id;

        LOG_DEBUG("discord", "> Using account key geneate command. GuildID {}. UserID {}", guildID, userID);

        auto const& cmdOptions = cmdData.options;
        if (cmdOptions.empty())
            return;

        dpp::message errorMessage;
        //errorMessage.set_flags(dpp::m_ephemeral);

        if (userID != OWNER_ID)
        {
            errorMessage.set_content("Что ты хотел сделать? Ахаха");
            slashCommandEvent.reply(errorMessage);
            return;
        }

        std::string accountName = std::get<std::string>(cmdOptions.at(0).value);

        auto newKey = sAccountMgr->GetRandomKey();

        auto result = sAccountMgr->ChangeKey(accountName, newKey);
        if (result != AccountResponceResult::Ok)
        {
            std::string errorMsg;

            switch (result)
            {
            case AccountResponceResult::LongKey:
                errorMsg = Warhead::StringFormat("Ошибка при создании нового пароля");
                break;
            case AccountResponceResult::NameNotExist:
                errorMsg = Warhead::StringFormat("Аккаунт `{}` не найден", accountName);
                break;
            default:
                errorMsg = "Неизвестная ошибка";
                break;
            }

            errorMessage.set_content(Warhead::StringFormat("Ошибка при генерации нового ключа для аккаунта. {}", errorMsg));
            slashCommandEvent.reply(errorMessage);
            return;
        }

        dpp::embed embed;
        embed.set_color(static_cast<uint32>(DiscordMessageColor::Teal));
        embed.set_title("Смена ключа для Warhead Discord");
        embed.set_description(Warhead::StringFormat("Вы успешно изменили ключ для аккаунта: `{}`", accountName));
        embed.add_field("Ваш ключ. Сохраните его!", Warhead::StringFormat("`{}`", newKey));
        embed.set_timestamp(GameTime::GetGameTime().count());
        embed.set_footer(Warhead::StringFormat("Guild ID: {}. Guild locale: {}", guildID, slashCommandEvent.command.guild_locale), "");

        dpp::message message;
        message.add_embed(embed);
        message.set_flags(dpp::m_ephemeral);
        slashCommandEvent.reply(message);
    });

    _bot->on_slashcommand([this](dpp::slashcommand_t const& event)
    {
        sCommandHandler->TryExecuteCommand(event);
    });
}

void DiscordBot::ConfigureGuildInviteHooks()
{
    _bot->on_guild_create([this](dpp::guild_create_t const& event)
    {
        if (HasClient(event.created->id))
            return;

        if (event.is_cancelled())
            return;

        AddClient(event.created->id, event.created->name, event.created->member_count, GameTime::GetGameTime());
        LogAddClient(event.created->id, DiscordMessageColor::Indigo, event.created->get_icon_url(),
            event.created->name, event.created->member_count, event.created->get_creation_time());
    });

    _bot->on_guild_delete([this](dpp::guild_delete_t const& event)
    {
        if (!HasClient(event.deleted->id))
            return;

        if (event.is_cancelled())
            return;

        DeleteClient(event.deleted->id);
        LogDeleteClient(event.deleted->id, DiscordMessageColor::Indigo, event.deleted->get_icon_url(), event.deleted->name);
    });

    _bot->on_guild_member_add([this](dpp::guild_member_add_t const& event)
    {
        if (event.is_cancelled())
            return;

        if (event.added.user_id == OWNER_ID)
        {
            auto embedMessage = std::make_shared<dpp::embed>();
            embedMessage->set_color(static_cast<uint32>(DiscordMessageColor::Indigo));
            embedMessage->set_title("Создатель, я увидел тебя!");
            embedMessage->set_description(Warhead::StringFormat("{}, я увидел тебя в гильдии: {}", OWNER_MENTION, event.adding_guild->name));
            embedMessage->set_timestamp(GameTime::GetGameTime().count());

            SendEmbedMessage(LOGS_CHANNEL_OTHER_ID, embedMessage);
        }
    });
}

void DiscordBot::CreateCommands(int64 guildID)
{
    LOG_DEBUG("discord", "> Create commands for guild id: {}", guildID);
    sCommandHandler->RefisterCommandsForGuild(_bot.get(), guildID);
}

void DiscordBot::LoadClients()
{
    StopWatch sw;

    _guilds.clear();

    LOG_INFO("discord", "Loading clients...");

    QueryResult result = DiscordDatabase.Query("SELECT `GuildID`, `GuildName`, `MembersCount`, UNIX_TIMESTAMP(`InviteDate`) FROM clients");
    if (!result)
    {
        LOG_WARN("sql.sql", ">> Loaded 0 clients. DB table `clients` is empty.");
        LOG_INFO("discord", "");
        return;
    }

    do
    {
        auto const& [guildID, guildName, membersCount, inviteDate] = result->FetchTuple<int64, std::string_view, uint32, Seconds>();
        
        _guilds.emplace(guildID, DiscordClients(guildID, guildName, membersCount, inviteDate));

    } while (result->NextRow());

    LOG_INFO("discord", ">> Loaded {} clients in {}", _guilds.size(), sw);
    LOG_INFO("discord", "");
}

void DiscordBot::CheckBotInGuild(int64 guildID, CompleteFunction&& execute)
{
    sAsyncCallbackMgr->AddAsyncCallback([this, guildID, execute = std::move(execute)]()
    {
        LOG_DEBUG("discord", "> Start check guild {} in bot", guildID);

        auto const& guilds = _bot->current_user_get_guilds_sync();
        if (guilds.empty())
        {
            execute(false);
            LOG_FATAL("discord", "> Empty guilds in bot. What?");
            return;
        }

        if (guilds.find(guildID) == guilds.end())
        {
            execute(false);
            LOG_ERROR("discord", "> Not found guild {} in bot", guildID);
            return;
        }

        execute(true);
        LOG_DEBUG("discord", "> Founded guild {} in bot", guildID);
    }, 1s);
}

void DiscordBot::CheckChannels(int64 guildID, CompleteChannelFunction&& execute)
{
    sAsyncCallbackMgr->AddAsyncCallback([this, guildID, execute = std::move(execute)]()
    {
        LOG_DEBUG("discord", "> Start check channels for guild id: {}", guildID);

        auto GetCategory = [this](dpp::channel_map const& channels) -> int64
        {
            for (auto const& [channelID, channel] : channels)
            {
                if (!channel.is_category())
                    continue;

                if (channel.name == DEFAULT_CATEGORY_NAME)
                {
                    LOG_DEBUG("discord", "> Category with name '{}' exist. ID {}", DEFAULT_CATEGORY_NAME, channelID);
                    return channelID;
                }
            }

            return 0;
        };

        auto CreateCategory = [this, guildID]() -> int64
        {
            try
            {
                dpp::channel categoryToCreate;
                categoryToCreate.set_guild_id(guildID);
                categoryToCreate.set_name(DEFAULT_CATEGORY_NAME);
                categoryToCreate.set_flags(dpp::CHANNEL_CATEGORY);

                auto createdCategory = _bot->channel_create_sync(categoryToCreate);
                return createdCategory.id;
            }
            catch (dpp::rest_exception const& error)
            {
                LOG_ERROR("discord", "DiscordBot::CheckChannels: Error at create category: {}", error.what());
                return 0;
            }

            return 0;
        };

        auto GetTextChannels = [this](dpp::channel_map const& channels, int64 findCategoryID, DiscordChannelsList& channelList)
        {
            for (auto const& [channelID, channel] : channels)
            {
                if (!channel.is_text_channel() || channel.parent_id != findCategoryID)
                    continue;

                auto channelType = GetDiscordChannelType(channel.name);
                if (channelType == DiscordChannelType::MaxType)
                    continue;

                channelList[static_cast<std::size_t>(channelType)] = channelID;
            }
        };

        auto CreateTextChannel = [this, guildID](DiscordChannelsList& channelList, int64 findCategoryID)
        {
            for (size_t i = 0; i < DEFAULT_CHANNELS_COUNT; i++)
            {
                auto& channelID = channelList[i];
                if (!channelID)
                {
                    auto channelName = GetChannelName(static_cast<DiscordChannelType>(i));
                    if (channelName.empty())
                    {
                        LOG_ERROR("discord", "> Empty get channel name for type {}", i);
                        continue;
                    }

                    try
                    {
                        dpp::channel channelToCreate;
                        channelToCreate.set_guild_id(guildID);
                        channelToCreate.set_name(channelName);
                        channelToCreate.set_flags(dpp::CHANNEL_TEXT);
                        channelToCreate.set_parent_id(findCategoryID);

                        auto createdChannel = _bot->channel_create_sync(channelToCreate);
                        channelID = createdChannel.id;
                        LOG_INFO("discord", "> Created channel {}. ID {}", channelName, createdChannel.id);
                    }
                    catch (dpp::rest_exception const& error)
                    {
                        LOG_ERROR("discord", "DiscordBot::CheckChannels: Error at create text channel: {}", error.what());
                        continue;
                    }
                }
            }
        };

        DiscordChannelsList channelsList{};

        try
        {
            int64 findCategoryID{ 0 };

            auto const& channels = _bot->channels_get_sync(guildID);
            if (channels.empty())
            {
                LOG_FATAL("discord", "> Empty channels in guild. Guild is new?");
                findCategoryID = CreateCategory();
            }

            // Exist any channel
            if (!findCategoryID)
                findCategoryID = GetCategory(channels);

            // Not found DEFAULT_CATEGORY_NAME
            if (!findCategoryID)
            {
                LOG_ERROR("discord", "> Category with name '{}' not found. Start creating", DEFAULT_CATEGORY_NAME);
                findCategoryID = CreateCategory();

                if (findCategoryID)
                    LOG_INFO("discord", "> Category with name '{}' created. ID: {}", DEFAULT_CATEGORY_NAME, findCategoryID);
                else
                {
                    LOG_INFO("discord", "> Error after create category with name '{}'", DEFAULT_CATEGORY_NAME);
                    execute(std::move(channelsList));
                    return;
                }
            }

            // Exist DEFAULT_CATEGORY_NAME
            GetTextChannels(channels, findCategoryID, channelsList);
            CreateTextChannel(channelsList, findCategoryID);
        }
        catch (dpp::rest_exception const& error)
        {
            LOG_ERROR("discord", "DiscordBot::CheckChannels: Error at check channels: {}", error.what());
            execute(std::move(channelsList));
            return;
        }

        LOG_DEBUG("discord", "> Founded {} text channels in guild", channelsList.size());
        execute(std::move(channelsList));
    });
}

void DiscordBot::Test()
{
    /*
    sDiscordBot->CheckBotInGuild(_warheadServerID, [this](bool isExist)
    {
        if (!isExist)
        {
            //SendAuthResponseError(DiscordAuthResponseCodes::BotNotFound);
            LOG_ERROR("network", "DiscordSocket::HandleAuthSession: Sent Auth Response (bot not found).");
            //DelayedCloseSocket();
            return;
        }

        sDiscordBot->CheckChannels(_warheadServerID, [this](DiscordChannelsList&& channelList)
        {
            if (channelList.empty())
            {
                //SendAuthResponseError(DiscordAuthResponseCodes::ChannelsNotFound);
                LOG_ERROR("network", "DiscordSocket::HandleAuthSession: Sent Auth Response (channels not found).");
                //DelayedCloseSocket();
                return;
            }

            if (channelList.size() != DEFAULT_CHANNELS_COUNT)
            {
                //SendAuthResponseError(DiscordAuthResponseCodes::ChannelsIncorrect);
                LOG_ERROR("network", "DiscordSocket::HandleAuthSession: Sent Auth Response (channels incorrect).");
                //DelayedCloseSocket();
                return;
            }

            LOG_INFO("network", "DiscordSocket::HandleAuthSession: Client authenticated successfully");

            LOG_INFO("discord", "--------------");

            for (auto const& itr : channelList)
                LOG_INFO("discord", "> {}", itr);

            LOG_INFO("discord", "--------------");
        });
    });
    */

    _bot->on_guild_member_add([](dpp::guild_member_add_t member)
    {
        LOG_WARN("discord", "-- on_guild_member_add --");
        LOG_INFO("discord", "> Guild name: {}", member.adding_guild->name);
        LOG_INFO("discord", "> Member id: {}", member.added.user_id);
    });

    /*sAsyncCallbackMgr->AddAsyncCallback([this]()
    {
        auto const& roles = _bot->roles_get_sync(_warheadServerID);

        for (auto const& [roleID, role] : roles)
        {
            LOG_INFO("discord", "> Role id: {}. Name: {}. Is admin {}", roleID, role.name, role.has_administrator());
        }
    }, 1s);*/

    //auto const& messages = _bot->messages_get_sync(TEST_CHANNEL_ID, 0, 0, 0, 100);

    //for (auto const& [messageID, message] : messages)
    //{
    //    /*LOG_INFO("discord", "> MessageID: {}", messageID);
    //    LOG_INFO("discord", "> User: {}", message.author.format_username());
    //    LOG_INFO("discord", "> User ID: {}. Is current bot? {}", message.author.id, _bot->me.id == message.author.id);
    //    LOG_INFO("discord", "--");*/

    //    if (_bot->me.id != message.author.id)
    //        continue;

    //    auto channel = _bot->channel_get_sync(message.channel_id);

    //    LOG_WARN("discord", "> Delete message {}", messageID);
    //    _bot->message_delete(messageID, message.channel_id);
    //}
}

bool DiscordBot::HasClient(int64 guildID)
{
    return _guilds.find(guildID) != _guilds.end();
}

DiscordClients* DiscordBot::GetClient(int64 guildID)
{
    auto const& itr = _guilds.find(guildID);
    if (itr == _guilds.end())
        return nullptr;

    return &itr->second;
}

void DiscordBot::AddClient(int64 guildID, std::string_view guildName, uint32 membersCount, Seconds inviteDate, bool atStartup /*= false*/)
{
    if (HasClient(guildID))
    {
        LOG_ERROR("discord", "> Try add existing client with guild id {}", guildID);
        return;
    }

    // Create commands for this guild
    CreateCommands(guildID);

    // Add in core cache
    _guilds.emplace(guildID, DiscordClients(guildID, guildName, membersCount, inviteDate));

    // Add to DB
    DiscordDatabase.Execute("INSERT INTO `clients` (`GuildID`, `GuildName`, `MembersCount`, `InviteDate`, `AddedAtStartup`) VALUES ({}, '{}', {}, FROM_UNIXTIME({}), {})",
        guildID, guildName, membersCount, inviteDate.count(), atStartup ? 1 : 0);
}

void DiscordBot::DeleteClient(int64 guildID)
{
    if (!HasClient(guildID))
    {
        LOG_ERROR("discord", "> Try delete non existing client with guild id {}", guildID);
        return;
    }

    // Delte from core cache
    _guilds.erase(guildID);

    // Delete from DB table
    DiscordDatabase.Execute("DELETE FROM `clients` WHERE `GuildID` = {}", guildID);
}

void DiscordBot::DeleteAllClients()
{
    // Clear core cache
    _guilds.clear();

    // Clear DB table
    DiscordDatabase.Execute("TRUNCATE `clients`");
}

void DiscordBot::LogAddClient(int64 guildID, DiscordMessageColor color, std::string_view icon, std::string_view guildName, uint32 membersCount, double creationDate)
{
    LOG_DEBUG("discord", "Join in new guild with name: {}", guildName);

    Seconds timeCreate = Seconds(static_cast<uint32>(creationDate));

    auto embedMessage = std::make_shared<dpp::embed>();
    embedMessage->set_color(static_cast<uint32>(color));
    embedMessage->set_author(std::string(guildName), "", std::string(icon));
    embedMessage->set_footer(Warhead::StringFormat("ID: {}", guildID), "");
    embedMessage->set_title("**Новая гильдия**");
    embedMessage->add_field("Название", Warhead::StringFormat("`{}`", guildName));
    embedMessage->add_field("Количество участников", Warhead::StringFormat("`{}`", membersCount));
    embedMessage->add_field("Дата создания", Warhead::StringFormat("`{}` | `{}`", Warhead::Time::TimeToHumanReadable(timeCreate), Warhead::Time::ToTimeString(GameTime::GetGameTime() - timeCreate)));
    embedMessage->set_timestamp(GameTime::GetGameTime().count());

    SendEmbedMessage(LOGS_CHANNEL_GUILD_ADD_ID, embedMessage);
}

void DiscordBot::LogDeleteClient(int64 guildID, DiscordMessageColor color, std::string_view icon, std::string_view guildName)
{
    LOG_DEBUG("discord", "Exit from guild with name: '{}'", guildName);

    auto embedMessage = std::make_shared<dpp::embed>();
    embedMessage->set_color(static_cast<uint32>(color));
    embedMessage->set_author(std::string(guildName), "", std::string(icon));
    embedMessage->set_footer(Warhead::StringFormat("ID: {}", guildID), "");
    embedMessage->set_title("**Выход из гильдии**");
    embedMessage->add_field("Название", Warhead::StringFormat("`{}`", guildName));
    embedMessage->set_timestamp(GameTime::GetGameTime().count());

    SendEmbedMessage(LOGS_CHANNEL_GUILD_DELETE_ID, embedMessage);
}
