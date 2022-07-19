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

#include "DiscordDatabase.h"
#include "MySQLPreparedStatement.h"

void DiscordDatabaseConnection::DoPrepareStatements()
{
    if (!m_reconnecting)
        m_stmts.resize(MAX_DISCORD_DATABASE_STATEMENTS);

    PrepareStatement(DISCORD_SEL_ACCOUNT_INFO_BY_NAME, "SELECT `a`.`ID`, `a`.`Salt`, `a`.`Verifier`, `a`.`RealmName`, `a`.`LastIP`, `a`.`CoreName`, `a`.`ModuleVersion`, "
        "`ab`.`bandate`, `ab`.`unbandate` "
        "FROM `account` a LEFT JOIN `account_banned` ab ON `a`.`id` = `ab`.`id` AND `ab`.`active` = 1 WHERE `a`.`Name` = ? LIMIT 1", CONNECTION_ASYNC);
    PrepareStatement(DISCORD_INS_ACCOUNT, "INSERT INTO account (`Name`, `Salt`, `Verifier`, `GuildID`, `RealmName`, `JoinDate`) VALUES (?, ?, ?, ?, ?, NOW())", CONNECTION_ASYNC);
    PrepareStatement(DISCORD_SEL_IP_INFO, "SELECT unbandate > UNIX_TIMESTAMP() OR unbandate = bandate, unbandate = bandate FROM ip_banned WHERE ip = ?", CONNECTION_ASYNC);
    PrepareStatement(DISCORD_SEL_ACCOUNT_ID_BY_USERNAME, "SELECT `ID` FROM `account` WHERE `Name` = ?", CONNECTION_ASYNC);
    PrepareStatement(DISCORD_SEL_ACCOUNTS, "SELECT `ID`, `Name`, `GuildID`, `RealmName` FROM account", CONNECTION_SYNCH);
    PrepareStatement(DISCORD_UPD_LOGON, "UPDATE `account` SET `Salt` = ?, `Verifier` = ? WHERE `ID` = ?", CONNECTION_ASYNC);

    // Ban manager
    PrepareStatement(DISCORD_DEL_BAN_ACCOUNT, "DELETE FROM `account_banned` WHERE `id` = ?", CONNECTION_ASYNC);
    PrepareStatement(DISCORD_DEL_BAN_IP, "DELETE FROM `ip_banned` WHERE `ip` = ?", CONNECTION_ASYNC);
    PrepareStatement(DISCORD_INS_ACCOUNT_BAN, "INSERT INTO `account_banned` (`id`, `bandate`, `unbandate`, `bannedby`, `banreason`, `active`) VALUES (?, UNIX_TIMESTAMP(), UNIX_TIMESTAMP() + ?, ?, ?, 1)", CONNECTION_ASYNC);
    PrepareStatement(DISCORD_INS_IP_BAN, "INSERT INTO `ip_banned` (`ip`, `bandate`, `unbandate`, `bannedby`, `banreason`, `active`) VALUES (?, UNIX_TIMESTAMP(), UNIX_TIMESTAMP() + ?, ?, ?, 1)", CONNECTION_ASYNC);
    PrepareStatement(DISCORD_UPD_ACCOUNT_BAN_EXPIRED, "UPDATE `account_banned` SET `active` = 0 WHERE `unbandate` <= UNIX_TIMESTAMP() AND `unbandate` <> `bandate`", CONNECTION_ASYNC);
    PrepareStatement(DISCORD_UPD_ACCOUNT_BAN_EXPIRED, "UPDATE `ip_banned` SET `active` = 0 WHERE `unbandate` <= UNIX_TIMESTAMP() AND `unbandate` <> `bandate`", CONNECTION_ASYNC);
}

DiscordDatabaseConnection::DiscordDatabaseConnection(MySQLConnectionInfo& connInfo) : MySQLConnection(connInfo)
{
}

DiscordDatabaseConnection::DiscordDatabaseConnection(ProducerConsumerQueue<SQLOperation*>* q, MySQLConnectionInfo& connInfo) : MySQLConnection(q, connInfo)
{
}

DiscordDatabaseConnection::~DiscordDatabaseConnection()
{
}
