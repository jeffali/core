/*
   Copyright (C) Cfengine AS

   This file is part of Cfengine 3 - written and maintained by Cfengine AS.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; version 3.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA

  To the extent this program is licensed as part of the Enterprise
  versions of Cfengine, the applicable Commerical Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#include "cf3.defs.h"

#include "lastseen.h"
#include "conversion.h"
#include "logging_old.h"
#include "files_hashes.h"
#include "locks.h"
#include "item_lib.h"

void UpdateLastSawHost(const char *hostkey, const char *address,
                       bool incoming, time_t timestamp);

/*
 * Lastseen database schema (version 1):
 *
 * Version entry
 *
 * key: "version\0"
 * value: "1\0"
 *
 * "Quality of connection" entries
 *
 * key: q<direction><hostkey> (direction: 'i' for incoming, 'o' for outgoing)
 * value: struct KeyHostSeen
 *
 * "Hostkey" entries
 *
 * key: k<hostkey> ("MD5-ffffefefeefef..." or "SHA-abacabaca...")
 * value: <address> (IPv4 or IPv6)
 *
 * "Address", or reverse, entries (auxiliary)
 *
 * key: a<address> (IPv6 or IPv6)
 * value: <hostkey>
 *
 *
 *
 * Schema version 0 mapped direction + hostkey to address + quality of
 * connection. This approach had a number of drawbacks:
 *  - There were two potentially conflicting addresses for given hostkey.
 *  - There was no way to quickly lookup hostkey by address.
 *  - Address update required traversal of the whole database.
 *
 * In order to overcome these limitations, new schema normalized (in relational
 * algebra sense) the data relations.
 */

/*****************************************************************************/

void LastSaw(char *ipaddress, unsigned char digest[EVP_MAX_MD_SIZE + 1], LastSeenRole role)
{
    char databuf[EVP_MAX_MD_SIZE * 4];
    char *mapip;

    if (strlen(ipaddress) == 0)
    {
        CfOut(OUTPUT_LEVEL_INFORM, "", "LastSeen registry for empty IP with role %d", role);
        return;
    }

    HashPrintSafe(CF_DEFAULT_DIGEST, digest, databuf);

    mapip = MapAddress(ipaddress);

    UpdateLastSawHost(databuf, mapip, role == LAST_SEEN_ROLE_ACCEPT, time(NULL));
}

/*****************************************************************************/

void UpdateLastSawHost(const char *hostkey, const char *address,
                       bool incoming, time_t timestamp)
{
    DBHandle *db = NULL;
    if (!OpenDB(&db, dbid_lastseen))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", " !! Unable to open last seen db");
        return;
    }

    /* Update quality-of-connection entry */

    char quality_key[CF_BUFSIZE];
    snprintf(quality_key, CF_BUFSIZE, "q%c%s", incoming ? 'i' : 'o', hostkey);

    KeyHostSeen newq = { .lastseen = timestamp };

    KeyHostSeen q;
    if (ReadDB(db, quality_key, &q, sizeof(q)))
    {
        newq.Q = QAverage(q.Q, newq.lastseen - q.lastseen, 0.4);
    }
    else
    {
        /* FIXME: more meaningful default value? */
        newq.Q = QDefinite(0);
    }
    WriteDB(db, quality_key, &newq, sizeof(newq));

    /* Update forward mapping */

    char hostkey_key[CF_BUFSIZE];
    snprintf(hostkey_key, CF_BUFSIZE, "k%s", hostkey);

    WriteDB(db, hostkey_key, address, strlen(address) + 1);

    /* Update reverse mapping */

    char address_key[CF_BUFSIZE];
    snprintf(address_key, CF_BUFSIZE, "a%s", address);

    WriteDB(db, address_key, hostkey, strlen(hostkey) + 1);

    CloseDB(db);
}
/*****************************************************************************/

static bool Address2HostkeyInDB(DBHandle *db, const char *address, char *result)
{
    char address_key[CF_BUFSIZE];
    char hostkey[CF_BUFSIZE];

    /* Address key: "a" + address */
    snprintf(address_key, CF_BUFSIZE, "a%s", address);

    if (!ReadDB(db, address_key, &hostkey, sizeof(hostkey)))
    {
        return false;
    }

    char hostkey_key[CF_BUFSIZE];
    char back_address[CF_BUFSIZE];

    /* Hostkey key: "k" + hostkey */
    snprintf(hostkey_key, CF_BUFSIZE, "k%s", hostkey);

    if (!ReadDB(db, hostkey_key, &back_address, sizeof(back_address)))
    {
        /* There is no key -> address mapping. Remove reverse mapping and return failure. */
        DeleteDB(db, address_key);
        return false;
    }

    if (strcmp(address, back_address) != 0)
    {
        /* Forward and reverse mappings do not match. Remove reverse mapping and return failure. */
        DeleteDB(db, address_key);
        return false;
    }

    strlcpy(result, hostkey, CF_BUFSIZE);
    return true;
}

/*****************************************************************************/

bool Address2Hostkey(const char *address, char *result)
{
    result[0] = '\0';
    if ((strcmp(address, "127.0.0.1") == 0) || (strcmp(address, "::1") == 0) || (strcmp(address, VIPADDRESS) == 0))
    {
        if (PUBKEY)
        {
            unsigned char digest[EVP_MAX_MD_SIZE + 1];
            HashPubKey(PUBKEY, digest, CF_DEFAULT_DIGEST);
            HashPrintSafe(CF_DEFAULT_DIGEST, digest, result);
            return true;
        }
        else
        {
            return false;
        }
    }

    DBHandle *db;
    if (!OpenDB(&db, dbid_lastseen))
    {
        return false;
    }

    bool ret = Address2HostkeyInDB(db, address, result);
    CloseDB(db);
    return ret;
}

/*****************************************************************************/
bool IsLastSeenCoherent(void)
{
    DBHandle *db;
    DBCursor *cursor;
    bool res = true;

    if (!OpenDB(&db, dbid_lastseen))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "!! Unable to open lastseen database");
        return false;
    }

    if (!NewDBCursor(db, &cursor))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", " !! Unable to create lastseen database cursor");
        CloseDB(db);
        return false;
    }

    char *key;
    void *value;
    int ksize, vsize;

    Item *qkeys=NULL;
    Item *akeys=NULL;
    Item *kkeys=NULL;
    Item *ahosts=NULL;
    Item *khosts=NULL;

    char val[CF_BUFSIZE];
    while (NextDB(cursor, &key, &ksize, &value, &vsize))
    {
        if (key[0] != 'k' && key[0] != 'q' && key[0] != 'a' )
        {
            continue;
        }

        if (key[0] == 'q' )
        {
          if(strncmp(key,"qiSHA=",6)==0 || strncmp(key,"qoSHA=",6)==0 ||
             strncmp(key,"qiMD5=",6)==0 || strncmp(key,"qoMD5=",6)==0)
          {
            if(IsItemIn(qkeys, key+2)==false)
            {
               PrependItem(&qkeys, key+2, NULL);
            }
          }
        }

        if (key[0] == 'k' )
        {
          if(strncmp(key, "kSHA=", 5)==0 || strncmp(key, "kMD5=", 5)==0)
          {
            PrependItem(&kkeys, key+1, NULL);
            if (ReadDB(db, key, &val, vsize))
            {
              PrependItem(&khosts, val, NULL);
            }
          }
        }

        if (key[0] == 'a' )
        {
            PrependItem(&ahosts, key+1, NULL);
            if (ReadDB(db, key, &val, vsize))
            {
              PrependItem(&akeys, val, NULL);
            }
        }

    }

    DeleteDBCursor(cursor);
    CloseDB(db);

    printf("%d %d %d\n", ListLen(qkeys), ListLen(akeys), ListLen(kkeys));
    printf("%d %d\n", ListLen(khosts), ListLen(ahosts));

    DumpItemList(qkeys);
    DumpItemList(akeys);
    DumpItemList(kkeys);
    DumpItemList(ahosts);
    DumpItemList(khosts);

    if (ListsCompare(ahosts, khosts) == false)
    {
        printf("Problem1: Hosts differ\n");
        res = false;
        goto clean;
    }
    if (ListsCompare(akeys, kkeys) == false)
    {
        printf("Problem2: Keys differ\n");
        res = false;
        goto clean;
    }

  clean:
    DeleteItemList(qkeys);
    DeleteItemList(akeys);
    DeleteItemList(kkeys);
    DeleteItemList(ahosts);
    DeleteItemList(khosts);

    if(res == true) printf("Yeah it's gooooooooood\n");
    return res;
}
/*****************************************************************************/
bool DeleteIpFromLastSeen(const char *ip, char *digest)
{
    DBHandle *db;
    DBCursor *cursor;
    bool res = false;

    if (!OpenDB(&db, dbid_lastseen))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "!! Unable to open lastseen database");
        return false;
    }

    char bufkey[CF_BUFSIZE + 1];
    char bufhost[CF_BUFSIZE + 1];

    strcpy(bufhost, "a");
    strlcat(bufhost, ip, CF_BUFSIZE);

    char key[CF_BUFSIZE];
    if (ReadDB(db, bufhost, &key, sizeof(key)) == true)
    {
        strcpy(bufkey, "k");
        strlcat(bufkey, key, CF_BUFSIZE);
        if (HasKeyDB(db, bufkey, strlen(bufkey) + 1) == false)
        {
            res = false;
            goto clean;
        }
        else
        {
            if (digest != NULL)
            {
                strcpy(digest, bufkey);
            }
            DeleteDB(db, bufkey);
            DeleteDB(db, bufhost);
            res = true;
        }
    }
    else
    {
        res = false;
        goto clean;
    }

    strcpy(bufkey, "qi");
    strlcat(bufkey, key, CF_BUFSIZE);
    DeleteDB(db, bufkey);

    strcpy(bufkey, "qo");
    strlcat(bufkey, key, CF_BUFSIZE);
    DeleteDB(db, bufkey);

clean:
    CloseDB(db);
    return res;
}
/*****************************************************************************/
bool DeleteDigestFromLastSeen(const char *key, char *ip)
{
    DBHandle *db;
    DBCursor *cursor;
    bool res = false;

    if (!OpenDB(&db, dbid_lastseen))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "!! Unable to open lastseen database");
        return false;
    }
    char bufkey[CF_BUFSIZE + 1];
    char bufhost[CF_BUFSIZE + 1];

    strcpy(bufkey, "k");
    strlcat(bufkey, key, CF_BUFSIZE);

    char host[CF_BUFSIZE];
    if (ReadDB(db, bufkey, &host, sizeof(host)) == true)
    {
        strcpy(bufhost, "a");
        strlcat(bufhost, host, CF_BUFSIZE);
        if (HasKeyDB(db, bufhost, strlen(bufhost) + 1) == false)
        {
            printf("Grr: No a$host entry in DB\n");
            res = false;
            goto clean;
        }
        else
        {
            if (ip != NULL)
            {
                strcpy(ip, host);
            }
            DeleteDB(db, bufhost);
            DeleteDB(db, bufkey);
            res = true;
        }
    }
    else
    {
        printf("Grr: No k$key entry in DB\n");
        res = false;
        goto clean;
    }

    strcpy(bufkey, "qi");
    strlcat(bufkey, key, CF_BUFSIZE);
    DeleteDB(db, bufkey);

    strcpy(bufkey, "qo");
    strlcat(bufkey, key, CF_BUFSIZE);
    DeleteDB(db, bufkey);

clean:
    CloseDB(db);
    return res;
}

/*****************************************************************************/
bool ScanLastSeenQuality(LastSeenQualityCallback callback, void *ctx)
{
    DBHandle *db;
    DBCursor *cursor;

    if (!OpenDB(&db, dbid_lastseen))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "!! Unable to open lastseen database");
        return false;
    }

    if (!NewDBCursor(db, &cursor))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", " !! Unable to create lastseen database cursor");
        CloseDB(db);
        return false;
    }

    char *key;
    void *value;
    int ksize, vsize;

    while (NextDB(cursor, &key, &ksize, &value, &vsize))
    {
        /* Only look for "keyhost" entries */
        if (key[0] != 'k')
        {
            continue;
        }

        const char *hostkey = key + 1;
        const char *address = value;

        char incoming_key[CF_BUFSIZE];
        snprintf(incoming_key, CF_BUFSIZE, "qi%s", hostkey);
        KeyHostSeen incoming;

        if (ReadDB(db, incoming_key, &incoming, sizeof(incoming)))
        {
            if (!(*callback)(hostkey, address, true, &incoming, ctx))
            {
                break;
            }
        }

        char outgoing_key[CF_BUFSIZE];
        snprintf(outgoing_key, CF_BUFSIZE, "qo%s", hostkey);
        KeyHostSeen outgoing;

        if (ReadDB(db, outgoing_key, &outgoing, sizeof(outgoing)))
        {
            if (!(*callback)(hostkey, address, false, &outgoing, ctx))
            {
                break;
            }
        }
    }

    DeleteDBCursor(cursor);
    CloseDB(db);

    return true;
}

/*****************************************************************************/

int LastSeenHostKeyCount(void)
{
    CF_DB *dbp;
    CF_DBC *dbcp;
    QPoint entry;
    char *key;
    void *value;
    int ksize, vsize;

    int count = 0;

    if (OpenDB(&dbp, dbid_lastseen))
    {
        memset(&entry, 0, sizeof(entry));

        if (NewDBCursor(dbp, &dbcp))
        {
            while (NextDB(dbcp, &key, &ksize, &value, &vsize))
            {
                /* Only look for valid "hostkey" entries */

                if ((key[0] != 'k') || (value == NULL))
                {
                    continue;
                }

                count++;
            }

            DeleteDBCursor(dbcp);
        }

        CloseDB(dbp);
    }

    return count;
}

