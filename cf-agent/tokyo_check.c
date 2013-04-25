#include "array_map_priv.h"
#include "hash_map_priv.h"
#include "map.h"
#include "string_lib.h"
#include "logging.h"

/*
 * The idea behind the following code comes from : copiousfreetime@github
 */

/*
 * Fields we will need from a TC record
 */
typedef struct tcrec {
  uint64_t offset;
  uint64_t length;

  uint64_t left;
  uint64_t right;

  uint32_t key_size;
  uint32_t rec_size;
  uint16_t pad_size;

  uint8_t  magic;
  uint8_t  hash;
} tcrec;

/* meta information from the Hash Database
 * used to coordinate the other operations
 */
typedef struct db_meta {
  uint64_t bucket_count;         /* Number of hash buckets                */
  uint64_t bucket_offset;        /* Start of the bucket list              */

  uint64_t record_count;         /* Number of records                     */
  uint64_t record_offset;        /* First record  offset in file          */

  short    alignment_pow;        /* power of 2 for calculating offsets */
  short    bytes_per;            /* 4 or 8 */
  char     dbpath[PATH_MAX+1];   /* full pathname to the database file */

  int      fd;

  StringMap* offset_map;
  StringMap* record_map;
} db_meta_t;

static db_meta_t* dbmeta_new_direct( const char* dbfilename )
{
  char hbuf[256];
  db_meta_t *dbmeta;

  dbmeta = (db_meta_t*)xcalloc( 1, sizeof( db_meta_t ));
  if(!dbmeta) {
    printf("Err:Error allocating memory : %s\n", strerror(errno));
    return NULL;
  }

  realpath( dbfilename, dbmeta->dbpath );
  if ( -1 == ( dbmeta->fd = open( dbmeta->dbpath, O_RDONLY) ) ) {
    printf("Err:Failure opening file [%s] : %s\n", dbmeta->dbpath, strerror( errno ));
    if(dbmeta) free(dbmeta);
    return NULL;
  }

  if ( 256 != read(dbmeta->fd, hbuf, 256 ) ) {
    printf("Err:Failure reading from database [%s] : %s\n", dbmeta->dbpath, strerror( errno ));
    close( dbmeta->fd );
    if(dbmeta) free(dbmeta);
    return NULL;
  }

  memcpy(&(dbmeta->bucket_count), hbuf + 40 , sizeof(uint64_t));
  dbmeta->bucket_offset = 256;
  uint8_t opts;
  memcpy(&opts, hbuf + 36, sizeof(uint8_t));
  dbmeta->bytes_per     = (opts & (1<<0)) ? sizeof(uint64_t) : sizeof(uint32_t);

  memcpy(&(dbmeta->record_count), hbuf + 48, sizeof(uint64_t));
  memcpy(&(dbmeta->record_offset), hbuf + 64, sizeof(uint64_t));
  memcpy(&(dbmeta->alignment_pow), hbuf + 34, sizeof(uint8_t));
  dbmeta->offset_map   = StringMapNew();
  dbmeta->record_map   = StringMapNew();
  printf("OMAP = %x RMAP = %x\n", dbmeta->offset_map, dbmeta->record_map);

  printf("Vrb:Database            : %s\n",   dbmeta->dbpath );
  printf("Vrb:  number of buckets : %llu\n", (long long unsigned)dbmeta->bucket_count );
  printf("Vrb:  offset of buckets : %llu\n", (long long unsigned)dbmeta->bucket_offset );
  printf("Vrb:  bytes per pointer : %llu\n", (long long unsigned)dbmeta->bytes_per );
  printf("Vrb:  alignment power   : %llu\n", (long long unsigned)dbmeta->alignment_pow);
  printf("Vrb:  number of records : %llu\n", (long long unsigned)dbmeta->record_count );
  printf("Vrb:  offset of records : %llu\n", (long long unsigned)dbmeta->record_offset );

  return dbmeta;
}

static void dbmeta_free( db_meta_t* dbmeta )
{
  StringMapDestroy( dbmeta->offset_map );
  StringMapDestroy( dbmeta->record_map );

  close( dbmeta->fd );

  if(dbmeta) free( dbmeta );
}

static int add_offset_to_map_unless_exists( StringMap** tree, uint64_t offset, int64_t bucket_index )
{
  char *tmp;
  xasprintf(&tmp, "%llu",offset);
  printf("Will add %s in %x\n", tmp, *tree);
  char *val;
  if ( StringMapHasKey(*tree, tmp) == false ) {
    xasprintf(&val, "%llu", bucket_index);
    StringMapInsert( *tree, tmp, val );
  } else {
    printf("Err:Duplicate offset for value %llu at index %lld, other value %llu, other index %s\n", 
        (long long unsigned)offset, (long long)bucket_index,
        (long long unsigned)offset   , (char *)StringMapGet(*tree, tmp));
    //if(new_node) free( new_node );
  }
  return 0;
}

static int dbmeta_populate_offset_tree( db_meta_t* dbmeta )
{
  uint64_t i;

  if(lseek( dbmeta->fd, dbmeta->bucket_offset, SEEK_SET )==-1) {
    printf("Err:Error traversing bucket section to find record offsets : %s\n", strerror(errno));
    return 1;
  }

  for( i = 0 ; i < dbmeta->bucket_count ; i++ ) {
    uint64_t offset = 0LL;
    int           b = read( dbmeta->fd, &offset, dbmeta->bytes_per);

    if ( b != dbmeta->bytes_per ) {
      printf("Err:Read the wrong number of bytes (%d)\n", b );
      return 2;
    }

    /* if the value is > 0 then we have a number so do something with it */
    if ( offset > 0 ) {
      offset = offset << dbmeta->alignment_pow;
      if(add_offset_to_map_unless_exists( &(dbmeta->offset_map), offset, i )) {
        return 3;
      }
    }
 }

 printf("Vrb:Found %llu buckets with offsets\n", (long long unsigned)StringMapSize( dbmeta->offset_map ));
 return 0;
}

enum {                                  // enumeration for magic data
 MAGIC_DATA_BLOCK = 0xc8,               // for data block
 MAGIC_FREE_BLOCK = 0xb0                // for free block
};

static int read_vary_int( int fd, uint32_t* result )
{
  uint64_t      num = 0;
  unsigned int base = 1;
  unsigned int    i = 0;
  int    read_bytes = 0;
  char c;

  while ( true ) {
    read_bytes += read( fd, &c, 1 );
    if ( c >= 0 ) {
      num += (c * base);
      break;
    }
    num += ( base * ( c + 1 ) * -1 );
    base <<= 7;
    i += 1;
  }

  *result = num;

  return read_bytes;
}


static bool dbmeta_read_one_rec( db_meta_t *dbmeta, tcrec* rec )
{
  if(lseek( dbmeta->fd, rec->offset, SEEK_SET )==-1) {
    printf("Err:Error traversing record section to find records : \n" );
  }

  while( true ) {
    // get the location of the current read
    //rec->offset = lseek64( dbmeta->fd, 0, SEEK_CUR );
    rec->offset = lseek( dbmeta->fd, 0, SEEK_CUR );
    if(rec->offset == (off_t) -1) {
      printf("Err:Error traversing record section to find records : \n" );
    }
    //rec->offset = ftell( dbmeta->fd );

    if ( 1 != read(dbmeta->fd, &(rec->magic), 1 ) ) {
      printf("Err:ERROR: Failure reading 1 byte, %s\n", strerror( errno ));
      return false;
    }

    if ( MAGIC_DATA_BLOCK ==  rec->magic ) {
      printf("Vrb:off=%llu[c8]\n", rec->offset);
      int length = 1;
  
      length += read( dbmeta->fd, &(rec->hash), 1 );
      length += read( dbmeta->fd, &(rec->left), dbmeta->bytes_per );
      rec->left = rec->left << dbmeta->alignment_pow;
  
      length += read( dbmeta->fd, &(rec->right), dbmeta->bytes_per );
      rec->right = rec->right << dbmeta->alignment_pow;
  
      length += read( dbmeta->fd, &(rec->pad_size), 2 );
      length += rec->pad_size;
     
      length += read_vary_int( dbmeta->fd, &(rec->key_size ));
      length += read_vary_int( dbmeta->fd, &(rec->rec_size ));
  
      rec->length = length + rec->key_size + rec->rec_size;
      return true;
  
    } else if ( MAGIC_FREE_BLOCK == rec->magic ) {
      printf("Vrb:off=%llu[b0]\n", rec->offset);
      uint32_t length;
      rec->length = 1;
      rec->length += read(dbmeta->fd, &length, sizeof( length ));
      rec->length += length;
      return true;
  
    } else {
      // read a non-magic byte, so skip it
      /*
      printf("Err:\nERROR : Read the start of a record at offset %llu, got %x instead of %x or %x\n",
            (long long unsigned)rec->offset, rec->magic, MAGIC_DATA_BLOCK, MAGIC_FREE_BLOCK );
      return false;
      */
    }
  }
  printf("Err:\nERROR : read loop reached here.\n");
  return false;
}

static int dbmeta_populate_record_tree( db_meta_t* dbmeta )
{
  off_t   offset;
  uint64_t data_blocks = 0;
  uint64_t free_blocks = 0;
  struct stat st;

  offset = dbmeta->record_offset;
  if(fstat( dbmeta->fd, &st ) == -1) {
    printf("Err:Error getting file stats :%s\n", strerror(errno));
    return 1;
  }

  while( offset < st.st_size ) {

    tcrec new_rec;
    memset(&new_rec, 0, sizeof(tcrec));
    new_rec.offset = offset;

    // read a variable-length record
    if( !dbmeta_read_one_rec( dbmeta, &new_rec )) { 
      printf("Err:Unable to fetch a new record from DB file\n");
      return 2;
    } else {
      offset = new_rec.offset + new_rec.length;
    }

    // if it is a data record then:
    // for the record, its left and right do:
    // look up that record in the offset tree
    // 1) remove it if it exists
    // 2) add it to the record_tree if it doesn't
 
    if ( MAGIC_DATA_BLOCK == new_rec.magic ) {

      if ( new_rec.offset > 0 ) {

        char *key;
        xasprintf(&key, "%llu", new_rec.offset);
        if ( StringMapHasKey(dbmeta->offset_map, key) == true) { 
          if(key) free( key );
        } else {
          //char *key;
          //xasprintf(&key, "%llu", new_rec.offset );
          StringMapInsert(dbmeta->record_map, key, "0");
        }
      } else {
        printf("Err:How do you have a new_rec.offset that is <= 0 ???\n");
      }

      if ( new_rec.left > 0 ) {
        printf("Vrb:>>> handle left %llu\n", new_rec.left);
        if( add_offset_to_map_unless_exists( &(dbmeta->offset_map), new_rec.left, -1 )) {
          return 4;
        }
      }

      if ( new_rec.right > 0 ) {
        printf("Vrb:>>> handle right %llu\n", new_rec.right);
        if(add_offset_to_map_unless_exists( &(dbmeta->offset_map), new_rec.right, -1 )) {
          return 4;
        }
      }

      data_blocks++;
    } else if ( MAGIC_FREE_BLOCK == new_rec.magic ) {
      // if it is a fragment record, then skip it
      free_blocks++;
    } else {
      printf("Err:NO record found at offset %llu\n", (long long unsigned)new_rec.offset );
    }
  }

  // if we are not at the end of the file, output the current file offset
  // with an appropriate message and return
  printf("Vrb:Found %llu data records and %llu free block records\n", data_blocks, free_blocks);

  return 0;
}

static int dbmeta_get_results( db_meta_t *dbmeta )
{
  uint64_t buckets_no_record = StringMapSize( dbmeta->offset_map) ;
  uint64_t records_no_bucket = StringMapSize( dbmeta->record_map) ;
  int ret = 0;

  printf("Vrb:Found %llu offsets listed in buckets that do not have records\n", buckets_no_record);
  printf("Vrb:Found %llu records in data that do not have an offset pointing to them\n", records_no_bucket);

  if ( buckets_no_record > 0 ) {
    ret += 1;
  }

  if ( records_no_bucket > 0 ) {
    ret += 2;
  }
  return ret;
}

int CheckTokyoDBCoherence( char *path )
{
  int ret = 0;
  db_meta_t *dbmeta;

  dbmeta = dbmeta_new_direct( path );
  if(dbmeta==NULL) {
    return 1;
  }

  printf("Vrb:Populating with bucket section offsets\n");
  ret = dbmeta_populate_offset_tree( dbmeta );
  if(ret) goto clean;

  printf("Vrb:Populating with record section offsets\n");
  ret = dbmeta_populate_record_tree( dbmeta );
  if(ret) goto clean;

  ret = dbmeta_get_results( dbmeta );

clean:
  if(dbmeta) dbmeta_free( dbmeta );

  return ret;
}

int main(int argc, char **argv ) {
  if(argc<2) {printf("usage dbpath\n"); return 1;}
  CheckTokyoDBCoherence("/var/cfengine/cf_classes.tcdb");
  return 0;
}
