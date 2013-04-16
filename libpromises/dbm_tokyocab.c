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

/*
 * Implementation using Tokyo Cabinet hash API.
 */

#include "cf3.defs.h"

#include "dbm_priv.h"
#include "dbm_lib.h"
#include "cfstream.h"

#ifdef TCDB

# include <tcutil.h>
# include <tchdb.h>

struct DBPriv_
{
    /*
     * This mutex prevents destructive modifications of the database (removing
     * records) while the cursor is active on it.
     */
    pthread_mutex_t cursor_lock;

    TCHDB *hdb;
};

struct DBCursorPriv_
{
    DBPriv *db;

    char *current_key;
    int current_key_size;
    char *curval;

    /*
     * Removing a key underneath the active cursor stops the database iteration,
     * so if key needs to be deleted while database is iterated, this fact is
     * remembered and once iterator advances to next key, this pending delete is
     * executed.
     *
     * Writes to key underneath the active cursor are safe, so only deletes are
     * tracked.
     */
    bool pending_delete;
};

/******************************************************************************/

static bool LockCursor(DBPriv *db)
{
    int ret = pthread_mutex_lock(&db->cursor_lock);
    if (ret != 0)
    {
        errno = ret;
        CfOut(OUTPUT_LEVEL_ERROR, "pthread_mutex_lock",
              "Unable to obtain cursor lock for Tokyo Cabinet database");
        return false;
    }
    return true;
}

static void UnlockCursor(DBPriv *db)
{
    int ret = pthread_mutex_unlock(&db->cursor_lock);
    if (ret != 0)
    {
        errno = ret;
        CfOut(OUTPUT_LEVEL_ERROR, "pthread_mutex_unlock",
              "Unable to release cursor lock for Tokyo Cabinet database");
    }
}

const char *DBPrivGetFileExtension(void)
{
    return "tcdb";
}

static const char *ErrorMessage(TCHDB *hdb)
{
    return tchdberrmsg(tchdbecode(hdb));
}

static bool OpenTokyoDatabase(const char *filename, TCHDB **hdb)
{
    *hdb = tchdbnew();

    if (!tchdbsetmutex(*hdb))
    {
        return false;
    }

    if (!tchdbopen(*hdb, filename, HDBOWRITER | HDBOCREAT))
    {
        return false;
    }

    return true;
}

DBPriv *DBPrivOpenDB(const char *dbpath)
{
    DBPriv *db = xcalloc(1, sizeof(DBPriv));

    pthread_mutex_init(&db->cursor_lock, NULL);

    if (!OpenTokyoDatabase(dbpath, &db->hdb))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "!! Could not open database %s: %s",
              dbpath, ErrorMessage(db->hdb));

        int errcode = tchdbecode(db->hdb);

        if(errcode != TCEMETA && errcode != TCEREAD)
        {
            goto err;
        }

        tchdbdel(db->hdb);

        CfOut(OUTPUT_LEVEL_ERROR, "", "!! Database \"%s\" is broken, recreating...", dbpath);

        DBPathMoveBroken(dbpath);

        if (!OpenTokyoDatabase(dbpath, &db->hdb))
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "!! Could not open database %s after recreate: %s",
                  dbpath, ErrorMessage(db->hdb));
            goto err;
        }
    }

    return db;

err:
    pthread_mutex_destroy(&db->cursor_lock);
    tchdbdel(db->hdb);
    free(db);
    return NULL;
}

void DBPrivCloseDB(DBPriv *db)
{
    int ret;

    if ((ret = pthread_mutex_destroy(&db->cursor_lock)) != 0)
    {
        errno = ret;
        CfOut(OUTPUT_LEVEL_ERROR, "pthread_mutex_destroy",
              "Unable to destroy mutex during Tokyo Cabinet database handle close");
    }

    if (!tchdbclose(db->hdb))
    {
    CfOut(OUTPUT_LEVEL_ERROR, "", "!! tchdbclose: Closing database failed: %s",
              ErrorMessage(db->hdb));
    }

    tchdbdel(db->hdb);
    free(db);
}

bool DBPrivHasKey(DBPriv *db, const void *key, int key_size)
{
    // FIXME: distinguish between "entry not found" and "error occured"

    return tchdbvsiz(db->hdb, key, key_size) != -1;
}

int DBPrivGetValueSize(DBPriv *db, const void *key, int key_size)
{
    return tchdbvsiz(db->hdb, key, key_size);
}

bool DBPrivRead(DBPriv *db, const void *key, int key_size, void *dest, int dest_size)
{
    if (tchdbget3(db->hdb, key, key_size, dest, dest_size) == -1)
    {
        if (tchdbecode(db->hdb) != TCENOREC)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "ReadComplexKeyDB(%s): Could not read: %s\n", (const char *)key, ErrorMessage(db->hdb));
        }
        return false;
    }

    return true;
}

static bool Write(TCHDB *hdb, const void *key, int key_size, const void *value, int value_size)
{
    if (!tchdbput(hdb, key, key_size, value, value_size))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "!! tchdbput: Could not write key to DB \"%s\": %s",
              tchdbpath(hdb), ErrorMessage(hdb));
        return false;
    }
    return true;
}

static bool Delete(TCHDB *hdb, const void *key, int key_size)
{
    if (!tchdbout(hdb, key, key_size) && tchdbecode(hdb) != TCENOREC)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "!! tchdbout: Could not delete key: %s",
              ErrorMessage(hdb));
        return false;
    }

    return true;
}

/*
 * This one has to be locked against cursor, or interaction between
 * write/pending delete might yield surprising results.
 */
bool DBPrivWrite(DBPriv *db, const void *key, int key_size, const void *value, int value_size)
{
    /* FIXME: get a cursor and see what is the current key */

    int ret = Write(db->hdb, key, key_size, value, value_size);

    return ret;
}

/*
 * This one has to be locked against cursor -- deleting entries might interrupt
 * iteration.
 */
bool DBPrivDelete(DBPriv *db, const void *key, int key_size)
{
    if (!LockCursor(db))
    {
        return false;
    }

    int ret = Delete(db->hdb, key, key_size);

    UnlockCursor(db);
    return ret;
}

DBCursorPriv *DBPrivOpenCursor(DBPriv *db)
{
    if (!LockCursor(db))
    {
        return false;
    }

    /*
     * Be more defensive when it comes to Tokyo Cabinet.
     * Ensure that the k/v store is optimized whenever a cursor is opened.
     */
    if (!tchdboptimize(db->hdb, -1, -1, -1, false))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "!! tchdboptimize failed: %s",
              ErrorMessage(db->hdb));
    }

    DBCursorPriv *cursor = xcalloc(1, sizeof(DBCursorPriv));
    cursor->db = db;

    /* Cursor remains locked */
    return cursor;
}

bool DBPrivAdvanceCursor(DBCursorPriv *cursor, void **key, int *key_size,
                     void **value, int *value_size)
{
    *key = tchdbgetnext3(cursor->db->hdb,
                         cursor->current_key, cursor->current_key_size,
                         key_size, (const char **)value, value_size);

    /*
     * If there is pending delete on the key, apply it
     */
    if (cursor->pending_delete)
    {
        Delete(cursor->db->hdb, cursor->current_key, cursor->current_key_size);
    }

    /* This will free the value as well: tchdbgetnext3 returns single allocated
     * chunk of memory */

    free(cursor->current_key);

    cursor->current_key = *key;
    cursor->current_key_size = *key_size;
    cursor->pending_delete = false;

    return *key != NULL;
}

bool DBPrivDeleteCursorEntry(DBCursorPriv *cursor)
{
    cursor->pending_delete = true;
    return true;
}

bool DBPrivWriteCursorEntry(DBCursorPriv *cursor, const void *value, int value_size)
{
    /*
     * If a pending deletion of entry has been requested, cancel it
     */
    cursor->pending_delete = false;

    return Write(cursor->db->hdb, cursor->current_key, cursor->current_key_size,
                 value, value_size);
}

void DBPrivCloseCursor(DBCursorPriv *cursor)
{
    DBPriv *db = cursor->db;

    if (cursor->pending_delete)
    {
        Delete(db->hdb, cursor->current_key, cursor->current_key_size);
    }

    free(cursor->current_key);
    free(cursor);

    /* Cursor lock was obtained in DBPrivOpenCursor */
    UnlockCursor(db);
}

/*
 * XXXX
 * YYYY
 */

#define SWAB64(TC_num) \
  ( \
   ((TC_num & 0x00000000000000ffULL) << 56) | \
   ((TC_num & 0x000000000000ff00ULL) << 40) | \
   ((TC_num & 0x0000000000ff0000ULL) << 24) | \
   ((TC_num & 0x00000000ff000000ULL) << 8) | \
   ((TC_num & 0x000000ff00000000ULL) >> 8) | \
   ((TC_num & 0x0000ff0000000000ULL) >> 24) | \
   ((TC_num & 0x00ff000000000000ULL) >> 40) | \
   ((TC_num & 0xff00000000000000ULL) >> 56) \
  )

static int mentol() {
  FILE *fd;
  fd = fopen("/cf_classes.tcdb", "r");
  printf("fd=%d\n", fd);
  uint64_t i;
  while(fread(&i, sizeof(uint64_t),1,fd) == 1) {
    printf("where=%u valx=%llx vald=%llu\n", ftell(fd), SWAB64(i), SWAB64(i)); 
    if(ftell(fd)>96) break;
  }
  close(fd);
  return 0;
}

bool DBPrivDiagnose(const char *path)
{
    FILE *fd;
    char hbuf[256];
    int ret;
    fd=fopen(path,"r");
    //check size is > minimal
    fseek(fd, 0, SEEK_END);
    int size = ftell(fd);
    printf("sz %d OFFSET=%u\n", size, 56);
    ret=fread(&hbuf, 256, 1, fd);
    uint64_t llnum = 0;
    memcpy(&llnum, hbuf + 64, sizeof(uint64_t));
    printf("ret=%d size = %llu xsize=%llx\n", ret, llnum, llnum);
    printf("ret=%d size = %llu xsize=%llx\n", ret, SWAB64(llnum), SWAB64(llnum));
    //check magic string
    //get file size reported in the header
    //get actual file size
    //if actual == reprted great
    //else
    //  check for indianness
    //  if different indianness, then indianness problem
    //  else corruption problem
    return ret;
#if 0
  char hbuf[HDBHEADSIZ];
  memset(hbuf, 0, HDBHEADSIZ);
  sprintf(hbuf, "%s\n%s:%d\n", HDBMAGICDATA, _TC_FORMATVER, _TC_LIBVER);
  memcpy(hbuf + HDBTYPEOFF, &(hdb->type), sizeof(hdb->type));
  memcpy(hbuf + HDBFLAGSOFF, &(hdb->flags), sizeof(hdb->flags));
  memcpy(hbuf + HDBAPOWOFF, &(hdb->apow), sizeof(hdb->apow));
  memcpy(hbuf + HDBFPOWOFF, &(hdb->fpow), sizeof(hdb->fpow));
  memcpy(hbuf + HDBOPTSOFF, &(hdb->opts), sizeof(hdb->opts));
  uint64_t llnum;
  llnum = hdb->bnum;
  llnum = TCHTOILL(llnum);
  memcpy(hbuf + HDBBNUMOFF, &llnum, sizeof(llnum));
  llnum = hdb->rnum;
  llnum = TCHTOILL(llnum);
  memcpy(hbuf + HDBRNUMOFF, &llnum, sizeof(llnum));
  llnum = hdb->fsiz;
  llnum = TCHTOILL(llnum);
  memcpy(hbuf + HDBFSIZOFF, &llnum, sizeof(llnum));
  llnum = hdb->frec;
  llnum = TCHTOILL(llnum);
  memcpy(hbuf + HDBFRECOFF, &llnum, sizeof(llnum));
typedef struct {
  uint8_t type;                          /* database type */
  uint8_t flags;                         /* additional flags */
  uint64_t bnum;                         /* number of the bucket array */
  uint8_t apow;                          /* power of record alignment */
  uint8_t fpow;                          /* power of free block pool number */
  uint8_t opts;                          /* options */
  uint32_t omode;                        /* open mode */
  uint64_t rnum;                         /* number of the records */
  uint64_t fsiz;                         /* size of the database file */
  uint64_t frec;                         /* offset of the first record */
} walo;
#endif

}

#endif
