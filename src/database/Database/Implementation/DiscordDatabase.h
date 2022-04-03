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

#ifndef _DISCORD_DATABASE_H_
#define _DISCORD_DATABASE_H_

#include "MySQLConnection.h"

enum DiscordDatabaseStatements : uint32
{
    /*  Naming standard for defines:
        {DB}_{SEL/INS/UPD/DEL/REP}_{Summary of data changed}
        When updating more than one field, consider looking at the calling function
        name for a suiting suffix.
    */

    DISCORD_SEL_ACCOUNT_INFO_BY_NAME,
    DISCORD_SEL_IP_INFO,

    MAX_DISCORD_DATABASE_STATEMENTS
};

class WH_DATABASE_API DiscordDatabaseConnection : public MySQLConnection
{
public:
    using Statements = DiscordDatabaseStatements;

    //- Constructors for sync and async connections
    DiscordDatabaseConnection(MySQLConnectionInfo& connInfo);
    DiscordDatabaseConnection(ProducerConsumerQueue<SQLOperation*>* q, MySQLConnectionInfo& connInfo);
    ~DiscordDatabaseConnection() override;

    //- Loads database type specific prepared statements
    void DoPrepareStatements() override;
};

#endif
