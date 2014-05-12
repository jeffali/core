/*
   Copyright (C) CFEngine AS

   This file is part of CFEngine 3 - written and maintained by CFEngine AS.

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
  versions of CFEngine, the applicable Commercial Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

/*
 * Implementation using LMDB API.
 */

#include "cf3.defs.h"
#include "dbm_priv.h"
#include "logging.h"
#include "string_lib.h"
#include "known_dirs.h"

#ifdef LMDB

#include <lmdb.h>

struct DBPriv_
{
    /******** Global to the thread ************/
    MDB_env *env;
    MDB_dbi dbi;
#ifndef NDEBUG
    // Used to detect misuses of DBRead/DBWrite inside cursor loops.
    // We set this to 0x1 when a thread creates a cursor, and back to 0x0 when
    // it is destroyed.
    pthread_key_t cursor_active_key;
#endif
    pthread_key_t txn_key;
};

typedef struct DB_txn_
{
    /******** Specific to the thread **********/
    bool       write_txn;
    MDB_txn    *txn;
    DBCursorPriv *cursor;
} DB_txn;

#ifndef NDEBUG
#define ASSERT_CURSOR_NOT_ACTIVE() assert(pthread_getspecific(db->cursor_active_key) == NULL)
#else
#define ASSERT_CURSOR_NOT_ACTIVE() (void)(0)
#endif

struct DBCursorPriv_
{
    DBPriv *db;
    MDB_cursor *mc;
    MDB_val delkey;
    void *curkv;
    bool pending_delete;
};

/******************************************************************************/
static int GetReadTransaction(DBPriv *db, MDB_txn **txn)
{
    DB_txn *db_txn = pthread_getspecific(db->txn_key);
    int rc = MDB_SUCCESS;

    if (!db_txn)
    {
        db_txn = xcalloc(1, sizeof(DB_txn));
        pthread_setspecific(db->txn_key, db_txn);
    }

    if (!db_txn->txn)
    {
        rc = mdb_txn_begin(db->env, NULL, MDB_RDONLY, &db_txn->txn);
        if (rc != MDB_SUCCESS)
        {
            Log(LOG_LEVEL_ERR, "Unable to open read transaction: %s", mdb_strerror(rc));
        }
    }

    *txn = db_txn->txn;

    return rc;
}

static int GetWriteTransaction(DBPriv *db, MDB_txn **txn)
{
    DB_txn *db_txn = pthread_getspecific(db->txn_key);
    int rc = MDB_SUCCESS;

    if (!db_txn)
    {
        db_txn = xcalloc(1, sizeof(DB_txn));
        pthread_setspecific(db->txn_key, db_txn);
    }

    if (db_txn->txn && !db_txn->write_txn)
    {
        mdb_txn_commit(db_txn->txn);
        db_txn->txn = NULL;
    }

    if (!db_txn->txn)
    {
        rc = mdb_txn_begin(db->env, NULL, 0, &db_txn->txn);
        if (rc == MDB_SUCCESS)
        {
            db_txn->write_txn = true;
        }
        else
        {
            Log(LOG_LEVEL_ERR, "Unable to open write transaction: %s", mdb_strerror(rc));
        }
    }

    *txn = db_txn->txn;

    return rc;
}

static int GetTransactionCursor(DBPriv *db, DBCursorPriv **cur)
{
    DB_txn *db_txn = pthread_getspecific(db->txn_key);
    int rc = MDB_SUCCESS;
#if 0

    if (!db_txn)
    {
        db_txn = xcalloc(1, sizeof(DB_txn));
        pthread_setspecific(db->txn_key, db_txn);
    }

    if (db_txn->txn && !db_txn->write_txn)
    {
        mdb_txn_commit(db_txn->txn);
        db_txn->txn = NULL;
    }

    if (!db_txn->txn)
    {
        rc = mdb_txn_begin(db->env, NULL, 0, &db_txn->txn);
        if (rc == MDB_SUCCESS)
        {
            db_txn->write_txn = true;
        }
        else
        {
            Log(LOG_LEVEL_ERR, "Unable to open write transaction: %s", mdb_strerror(rc));
        }
    }
#endif
    *cur = db_txn->cursor;

    return rc;
}
static void AbortTransaction(DBPriv *db)
{
    DB_txn *db_txn = pthread_getspecific(db->txn_key);
    mdb_txn_abort(db_txn->txn);
    db_txn->txn = NULL;
    db_txn->write_txn = false;
}

static int CommitTransaction(DBPriv *db)
{
    DB_txn *db_txn = pthread_getspecific(db->txn_key);
    int rc = mdb_txn_commit(db_txn->txn);
    db_txn->txn = NULL;
    db_txn->write_txn = false;
    return rc;
}

const char *DBPrivGetFileExtension(void)
{
    return "lmdb";
}

#ifndef LMDB_MAXSIZE
#define LMDB_MAXSIZE    104857600
#endif

/* Lastseen default number of maxreaders = 4x the default lmdb maxreaders */
#define DEFAULT_LASTSEEN_MAXREADERS (126*4)

DBPriv *DBPrivOpenDB(const char *dbpath, dbid id)
{
    DBPriv *db = xcalloc(1, sizeof(DBPriv));

    MDB_txn *txn = NULL;
    int rc;
    rc = pthread_key_create(&db->txn_key, NULL);
    if (rc)
    {
        Log(LOG_LEVEL_ERR, "Could not create transaction key. (pthread_key_create: '%s')",
            GetErrorStrFromCode(rc));
        free(db);
        return NULL;
    }

    rc = mdb_env_create(&db->env);
    if (rc)
    {
        Log(LOG_LEVEL_ERR, "Could not create handle for database %s: %s",
              dbpath, mdb_strerror(rc));
        goto err;
    }
    rc = mdb_env_set_mapsize(db->env, LMDB_MAXSIZE);
    if (rc)
    {
        Log(LOG_LEVEL_ERR, "Could not set mapsize for database %s: %s",
              dbpath, mdb_strerror(rc));
        goto err;
    }
    if (id == dbid_lastseen)
    {
        /* lastseen needs by default 4x more reader locks than other DBs*/
        rc = mdb_env_set_maxreaders(db->env, DEFAULT_LASTSEEN_MAXREADERS);
        if (rc)
        {
            Log(LOG_LEVEL_ERR, "Could not set maxreaders for database %s: %s",
                  dbpath, mdb_strerror(rc));
            goto err;
        }
    }
    if (id != dbid_locks)
    {
        rc = mdb_env_open(db->env, dbpath, MDB_NOSUBDIR, 0644);
    }
    else
    {
        rc = mdb_env_open(db->env, dbpath, MDB_NOSUBDIR|MDB_NOSYNC, 0644);
    }
    if (rc)
    {
        Log(LOG_LEVEL_ERR, "Could not open database %s: %s",
              dbpath, mdb_strerror(rc));
        goto err;
    }
    rc = mdb_txn_begin(db->env, NULL, MDB_RDONLY, &txn);
    if (rc)
    {
        Log(LOG_LEVEL_ERR, "Could not open database txn %s: %s",
              dbpath, mdb_strerror(rc));
        goto err;
    }
    rc = mdb_open(txn, NULL, 0, &db->dbi);
    if (rc)
    {
        Log(LOG_LEVEL_ERR, "Could not open database dbi %s: %s",
              dbpath, mdb_strerror(rc));
        mdb_txn_abort(txn);
        goto err;
    }
    rc = mdb_txn_commit(txn);
    if (rc)
    {
        Log(LOG_LEVEL_ERR, "Could not commit database dbi %s: %s",
              dbpath, mdb_strerror(rc));
        goto err;
    }

#ifndef NDEBUG
    pthread_key_create(&db->cursor_active_key, NULL);
#endif

    return db;

err:
    if (db->env)
    {
        mdb_env_close(db->env);
    }
pthread_key_delete(db->txn_key);
    free(db);
    if (rc == MDB_INVALID)
    {
        return DB_PRIV_DATABASE_BROKEN;
    }
    return NULL;
}

void DBPrivCloseDB(DBPriv *db)
{
    if (db->env)
    {
        mdb_env_close(db->env);
    }
pthread_key_delete(db->txn_key);
    free(db);
}

bool DBPrivHasKey(DBPriv *db, const void *key, int key_size)
{
    ASSERT_CURSOR_NOT_ACTIVE();

    MDB_val mkey, data;
    MDB_txn *txn;
    int rc;
    // FIXME: distinguish between "entry not found" and "error occured"

    rc = GetReadTransaction(db, &txn);
    if (rc == MDB_SUCCESS)
    {
        mkey.mv_data = (void *)key;
        mkey.mv_size = key_size;
        rc = mdb_get(txn, db->dbi, &mkey, &data);
        if (rc && rc != MDB_NOTFOUND)
        {
            Log(LOG_LEVEL_ERR, "Could not read (check for key): %s", mdb_strerror(rc));
        }
        AbortTransaction(db);
    }
    else
    {
        Log(LOG_LEVEL_ERR, "Could not create read txn: %s", mdb_strerror(rc));
    }

    return rc == MDB_SUCCESS;
}

int DBPrivGetValueSize(DBPriv *db, const void *key, int key_size)
{
    ASSERT_CURSOR_NOT_ACTIVE();

    MDB_val mkey, data;
    MDB_txn *txn;
    int rc;

    data.mv_size = 0;

    rc = GetReadTransaction(db, &txn);
    if (rc == MDB_SUCCESS)
    {
        mkey.mv_data = (void *)key;
        mkey.mv_size = key_size;
        rc = mdb_get(txn, db->dbi, &mkey, &data);
        if (rc && rc != MDB_NOTFOUND)
        {
            Log(LOG_LEVEL_ERR, "Could not fetch value size: %s", mdb_strerror(rc));
        }
        AbortTransaction(db);
    }
    else
    {
        Log(LOG_LEVEL_ERR, "Could not create read txn: %s", mdb_strerror(rc));
        AbortTransaction(db);
    }

    return data.mv_size;
}

bool DBPrivRead(DBPriv *db, const void *key, int key_size, void *dest, int dest_size)
{
    ASSERT_CURSOR_NOT_ACTIVE();

    MDB_val mkey, data;
    MDB_txn *txn;
    int rc;
    bool ret = false;

    //TODO: reopen txn if it is a write transaction ??
    rc = GetReadTransaction(db, &txn);
    if (rc == MDB_SUCCESS)
    {
        mkey.mv_data = (void *)key;
        mkey.mv_size = key_size;
        rc = mdb_get(txn, db->dbi, &mkey, &data);
        if (rc == MDB_SUCCESS)
        {
            if (dest_size > data.mv_size)
            {
                dest_size = data.mv_size;
            }
            memcpy(dest, data.mv_data, dest_size);
            ret = true;
        }
        else if (rc != MDB_NOTFOUND)
        {
            Log(LOG_LEVEL_ERR, "Could not read entry: %s", mdb_strerror(rc));
        }
        AbortTransaction(db);
    }
    else
    {
        Log(LOG_LEVEL_ERR, "Could not create read txn: %s", mdb_strerror(rc));
    }
    return ret;
}

bool DBPrivWrite(DBPriv *db, const void *key, int key_size, const void *value, int value_size)
{
    ASSERT_CURSOR_NOT_ACTIVE();

    MDB_val mkey, data;
    MDB_txn *txn;
    int rc = GetWriteTransaction(db, &txn);
    if (rc == MDB_SUCCESS)
    {
        mkey.mv_data = (void *)key;
        mkey.mv_size = key_size;
        data.mv_data = (void *)value;
        data.mv_size = value_size;
        rc = mdb_put(txn, db->dbi, &mkey, &data, 0);
        if (rc == MDB_SUCCESS)
        {
            rc = CommitTransaction(db);
            if (rc)
            {
                Log(LOG_LEVEL_ERR, "Could not commit: %s", mdb_strerror(rc));
            }
        }
        else
        {
            Log(LOG_LEVEL_ERR, "Could not write: %s", mdb_strerror(rc));
            AbortTransaction(db);
        }
    }
    else
    {
        Log(LOG_LEVEL_ERR, "Could not create write txn: %s", mdb_strerror(rc));
    }
    return rc == MDB_SUCCESS;
}

bool DBPrivDelete(DBPriv *db, const void *key, int key_size)
{
    ASSERT_CURSOR_NOT_ACTIVE();

    MDB_val mkey;
    MDB_txn *txn;
    int rc = GetWriteTransaction(db, &txn);
    if (rc == MDB_SUCCESS)
    {
        mkey.mv_data = (void *)key;
        mkey.mv_size = key_size;
        rc = mdb_del(txn, db->dbi, &mkey, NULL);
        if (rc == MDB_SUCCESS)
        {
            rc = CommitTransaction(db);
            if (rc)
            {
                Log(LOG_LEVEL_ERR, "Could not commit: %s", mdb_strerror(rc));
            }
        }
        else if (rc == MDB_NOTFOUND)
        {
            Log(LOG_LEVEL_DEBUG, "Entry not found: %s", mdb_strerror(rc));
            AbortTransaction(db);
        }
        else
        {
            Log(LOG_LEVEL_ERR, "Could not delete: %s", mdb_strerror(rc));
            AbortTransaction(db);
        }
    }
    else
    {
        Log(LOG_LEVEL_ERR, "Could not create write txn: %s", mdb_strerror(rc));
    }
    return rc == MDB_SUCCESS;
}

static DBCursorPriv *DBPrivOpenCursorInternal(DBPriv *db, bool write)
{
    ASSERT_CURSOR_NOT_ACTIVE();

    //DBCursorPriv *cursor = NULL;
    MDB_txn *txn;
    int rc;
    MDB_cursor *mc;
    DB_txn *db_txn = NULL;

    if (write)
    {
        rc = GetWriteTransaction(db, &txn);
    }
    else
    {
        rc = GetReadTransaction(db, &txn);
    }
    if (rc == MDB_SUCCESS)
    {
        rc = mdb_cursor_open(txn, db->dbi, &mc);
        if (rc == MDB_SUCCESS)
        {
            db_txn = pthread_getspecific(db->txn_key);
            //TODO: Check db_txn is not null
            //TODO: Free cursor if NOT NULL
            db_txn->cursor = xcalloc(1, sizeof(DBCursorPriv));
            db_txn->cursor->db = db;
            db_txn->cursor->mc = mc;
        }
        else
        {
            Log(LOG_LEVEL_ERR, "Could not open cursor: %s", mdb_strerror(rc));
            AbortTransaction(db);
        }
        /* txn remains with cursor */
    }
    else
    {
        Log(LOG_LEVEL_ERR, "Could not create cursor txn: %s", mdb_strerror(rc));
    }

#ifndef NDEBUG
    // We use dummy pointer, we just need to know that it's not null.
    pthread_setspecific(db->cursor_active_key, (const void *)0x1);
#endif
printf("dbxo=%p,curo=%p,rc=%d\n",db_txn,db_txn->cursor,rc);

    return db_txn->cursor;
}

DBCursorPriv *DBPrivOpenCursor(DBPriv *db)
{
    return DBPrivOpenCursorInternal(db, true);
}

DBCursorPriv *DBPrivOpenWriteCursor(DBPriv *db)
{
    return DBPrivOpenCursorInternal(db, true);
}

bool DBPrivAdvanceCursor(DBCursorPriv *cursor, void **key, int *key_size,
                     void **value, int *value_size)
{
    MDB_val mkey, data;
    int rc;
    bool retval = false;

    DB_txn *db_txn = pthread_getspecific(cursor->db->txn_key);

    if (cursor->curkv)
    {
        free(cursor->curkv);
        cursor->curkv = NULL;
    }
    if ((rc = mdb_cursor_get(cursor->mc, &mkey, &data, MDB_NEXT)) == MDB_SUCCESS)
    {
        cursor->curkv = xmalloc(mkey.mv_size + data.mv_size);
        memcpy(cursor->curkv, mkey.mv_data, mkey.mv_size);
        *key = cursor->curkv;
        *key_size = mkey.mv_size;
        *value_size = data.mv_size;
        memcpy((char *)cursor->curkv+mkey.mv_size, data.mv_data, data.mv_size);
        *value = (char *)cursor->curkv + mkey.mv_size;
        retval = true;
    }
    else if (rc != MDB_NOTFOUND)
    {
        Log(LOG_LEVEL_ERR, "Could not advance cursor: %s", mdb_strerror(rc));
    }
    if (cursor->pending_delete)
    {
        int r2;
        /* Position on key to delete */
        r2 = mdb_cursor_get(cursor->mc, &cursor->delkey, NULL, MDB_SET);
        if (r2 == MDB_SUCCESS)
        {
            r2 = mdb_cursor_del(cursor->mc, 0);
        }
        /* Reposition the cursor if it was valid before */
        if (rc == MDB_SUCCESS)
        {
            mkey.mv_data = *key;
            rc = mdb_cursor_get(cursor->mc, &mkey, NULL, MDB_SET);
        }
        cursor->pending_delete = false;
    }
    return retval;
}

bool DBPrivDeleteCursorEntry(DBCursorPriv *cursor)
{
    DB_txn *db_txn = pthread_getspecific(cursor->db->txn_key);
    int rc = mdb_cursor_get(cursor->mc, &cursor->delkey, NULL, MDB_GET_CURRENT);
printf("dbxd=%p,curd=%p,rc=%d\n",db_txn,cursor,rc);
    if (rc == MDB_SUCCESS)
    {
        cursor->pending_delete = true;
    }
    return rc == MDB_SUCCESS;
}

bool DBPrivWriteCursorEntry(DBCursorPriv *cursor, const void *value, int value_size)
{
    MDB_val data;
    int rc;
    DB_txn *db_txn = pthread_getspecific(cursor->db->txn_key);

    cursor->pending_delete = false;
    data.mv_data = (void *)value;
    data.mv_size = value_size;

    if ((rc = mdb_cursor_put(cursor->mc, NULL, &data, MDB_CURRENT)) != MDB_SUCCESS)
    {
        Log(LOG_LEVEL_ERR, "Could not write cursor entry: %s", mdb_strerror(rc));
    }
printf("dbxi=%p,curi=%p,rc=%d\n",db_txn,cursor,rc);
    return rc == MDB_SUCCESS;
}

void DBPrivCloseCursor(DBCursorPriv *cursor)
{
    MDB_txn *txn;
    int rc;
    DB_txn *db_txn = pthread_getspecific(cursor->db->txn_key);

    if (cursor->curkv)
    {
        free(cursor->curkv);
    }

    if (cursor->pending_delete)
    {
        mdb_cursor_del(cursor->mc, 0);
    }

    txn = mdb_cursor_txn(cursor->mc);
    mdb_cursor_close(cursor->mc);
    rc = mdb_txn_commit(txn);
    if (rc)
    {
        Log(LOG_LEVEL_ERR, "Could not commit cursor txn: %s", mdb_strerror(rc));
    }
#ifndef NDEBUG
    pthread_setspecific(cursor->db->cursor_active_key, NULL);
#endif
    free(cursor);
}

char *DBPrivDiagnose(const char *dbpath)
{
    return StringFormat("Unable to diagnose LMDB file (not implemented) for '%s'", dbpath);
}

int UpdateLastSeenMaxReaders(int maxreaders)
{
    int rc = 0;
    /* We assume that every cf_lastseen DB has already a minimum of 504 maxreaders */
    if (maxreaders > DEFAULT_LASTSEEN_MAXREADERS)
    {
        char workbuf[CF_BUFSIZE];
        MDB_env *env = NULL;
        rc = mdb_env_create(&env);
        if (rc)
        {
            Log(LOG_LEVEL_ERR, "Could not create lastseen database env : %s",
                mdb_strerror(rc));
            goto err;
        }

        rc = mdb_env_set_maxreaders(env, maxreaders);
        if (rc)
        {
            Log(LOG_LEVEL_ERR, "Could not change lastseen maxreaders to %d : %s",
                maxreaders, mdb_strerror(rc));
            goto err;
        }

        snprintf(workbuf, CF_BUFSIZE, "%s%ccf_lastseen.lmdb", GetWorkDir(), FILE_SEPARATOR);
        rc = mdb_env_open(env, workbuf, MDB_NOSUBDIR, 0644);
        if (rc)
        {
            Log(LOG_LEVEL_ERR, "Could not open lastseen database env : %s",
                mdb_strerror(rc));
        }
err:
        if (env)
        {
            mdb_env_close(env);
        }
    }
    return rc;
}
#endif
