#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "common.h"
#include "shm.h"
#include "bucket.h"
#include "mem_manager.h"
#include "circular_log.h"

#define SEGREGATED_MAX_DATA_SIZE 1280 // 1472

/***** circular_log_entry *****/
static inline bool
match_circular_log_entry_key(circular_log_entry* entry1, 
                             uint64_t addr, uint64_t offset);

static inline uint64_t
rss_queue_hash_portion(uint64_t keyhash);

/***** circular_log *****/
static inline bool
__get_circular_log_entry(circular_log* log_table, bucket* bucket, 
                         circular_log_entry* entry);

//static inline void
//delete_firstn_circular_log_entry(circular_log* log_table);

/***** key-value table *****/
static inline bucket*
get_entry_bucket(kv_table* table, circular_log_entry* entry);

/***** circular_log_entry *****/
static inline bool
match_circular_log_entry_key(circular_log_entry* entry1, 
                             uint64_t addr, uint64_t offset)
{
  circular_log_entry* entry2;
  entry2 = (circular_log_entry*) (addr + offset);
  return equal_circular_log_entry_key(entry1, entry2);
}

// 16bit
static inline uint64_t
rss_queue_hash_portion(uint64_t keyhash)
{
  return (keyhash >> 48) & 0xffffUL;
}

/***** circular_log *****/
circular_log*
create_circular_log(char* filename, uint64_t log_mem_size)
{
  circular_log* log_table = (circular_log*) malloc(sizeof(circular_log));
  if (log_table == NULL) goto error0;
  memset(log_table, 0, sizeof(circular_log));

  mem_allocator* allocator;
  allocator = create_mem_allocator(filename, log_mem_size);  
  if (allocator == NULL) goto error1;

  segregated_fits* sfits = create_segregated_fits(SEGREGATED_MAX_DATA_SIZE);
  if (sfits == NULL) goto error2;

  sfits->addr = (uint64_t) allocator->addr;
  sfits->addr_size = (uint32_t) allocator->size;
  void* addr = allocator->addr;
  segregated_fits_reclassing(sfits, &addr, (uint32_t*) &log_mem_size);
  
  log_table->sfits = sfits;
  log_table->allocator = allocator;
  log_table->addr = (uint64_t) allocator->addr;
  log_table->len  = log_mem_size;
  log_table->head = log_table->tail = 0;
  return log_table;
error2:
  D("error2");
  destroy_mem_allocator(allocator);
error1:
  D("error1");
  free(log_table);
error0:
  D("error0");
  return NULL;
}

void
destroy_circular_log(circular_log* log_table)
{  
  if(log_table == NULL) return;

  destroy_segregated_fits(log_table->sfits);
  destroy_mem_allocator(log_table->allocator);
  free(log_table);
}

/**
 * before calling this function, MUST check if the key exist already in the log
 * or not and get the bucket pointer at that time.
 */
bool
put_circular_log_entry(circular_log* log_table, bucket* bucket, 
                       circular_log_entry* entry)
{
  uint64_t version, new_version;
  OPTIMISTIC_LOCK(version, new_version, bucket);
  
  remove_circular_log_entry(log_table, bucket, entry);
  segregated_fits *sfits = log_table->sfits;
  uint32_t entry_size = (uint32_t) entry->initial_size;
  void* new_addr = get_segregated_fits_block(sfits, entry_size);
  if (new_addr == NULL) {
    return false;
  }

  memcpy(new_addr, entry, entry_size);
  OPTIMISTIC_UNLOCK(bucket);

  return true;
}

static inline bool
__get_circular_log_entry(circular_log* log_table, bucket* bucket, 
                         circular_log_entry* entry)
{
  int index = 0;
  index_entry *i_entry;
  uint64_t keyhash = entry->keyhash;
  circular_log_entry* dst_entry;
  
  do {
    i_entry = search_index_entry(&bucket, keyhash, &index);
    if (i_entry != NULL) {
      if (match_index_entry_tag(i_entry->tag, keyhash)) {
        // calc from offset and base pointer
        uint64_t offset = bucket->entries[index].offset;
        // check the key is the same
        if (match_circular_log_entry_key(entry, log_table->addr, offset)) {
          dst_entry = (circular_log_entry*)((uint64_t)log_table->addr + offset);
          memcpy(entry, dst_entry, dst_entry->initial_size);
          return true;
        }
      }
    }
  } while(i_entry != NULL);

  D("404: NOT FOUND ");
  return false;
}

/**
 * search the key in circular_log.
 * @entry:input set key & hashkey
 * @entry:ouput matched log entry.
 */
bool
get_circular_log_entry(circular_log* log_table, bucket* bucket,
                       circular_log_entry* entry)
{
  bool ret;
  uint64_t version_begining, version_end;
  do {
    version_begining = bucket->version;
    ret = __get_circular_log_entry(log_table, bucket, entry);
    version_end = bucket->version;
  } while(version_begining != version_end);
  return ret;
}

/**
 * This function must call in Optimistic lock
 */
bool
remove_circular_log_entry(circular_log* log_table, bucket* bucket, 
                          circular_log_entry* entry)
{  
  int index = 0;
  index_entry *i_entry;
  uint64_t keyhash = entry->keyhash;
  void* old_entry = NULL;
  //bucket_pool* bkt_pool = log_table->bkt_pool;
  
  do {
    i_entry = search_index_entry(&bucket, keyhash, &index);
    if (i_entry != NULL) {
      if (match_index_entry_tag(i_entry->tag, keyhash)) {
        uint64_t offset = bucket->entries[index].offset;
        if (match_circular_log_entry_key(entry, log_table->addr, offset)) {
          delete_index_entry_with_index(bucket, index);
          old_entry = (void*)((uint64_t)log_table->addr + offset);
          free_segregated_fits_block(log_table->sfits, 
                                     (segregated_fits_list*)old_entry);
          return true;
        }
      }
    }
  } while(i_entry != NULL);
  return false;
}

/***** key-value table *****/
static inline bucket*
get_entry_bucket(kv_table* table, circular_log_entry* entry)
{
  bucket_pool* pool = table->bkt_pool;
  uint64_t bucket_mask = pool->main_size - 1;
  uint64_t index = bucket_hash_portion(entry->keyhash) & bucket_mask;
  return &pool->mains[index];
}

kv_table*
create_kv_table(char* file, uint32_t nthread,  uint32_t main_size,
                uint32_t spare_size)
{
  
  kv_table* table = (kv_table*) malloc(sizeof(kv_table) +
                                       sizeof(circular_log*) * nthread);
  if (table == NULL) goto error0;
  memset(table, 0, sizeof(kv_table) + sizeof(circular_log*) * nthread);

  bucket_pool* bkt_pool = create_bucket_pool(file, main_size, spare_size);
  if (bkt_pool == NULL) goto error1;

  for(int i = 0; i < nthread; i++) {
    char filename[PATH_MAX];
    sprintf(filename, "%s_%d", file, i);
    circular_log* log_table = create_circular_log(filename, CIRCULAR_LOG_SIZE);
    if (log_table == NULL) {
      goto error2;
    }
    log_table->bkt_pool = bkt_pool;
    table->log[i] = log_table; 
  }

  table->bkt_pool = bkt_pool;
  table->log_size = nthread;
  return table;
error2:
  for(int i = 0; i < nthread; i++) {
    if (table->log[i] != NULL) {
      destroy_circular_log(table->log[i]);
    }
  }
  destroy_bucket_pool(bkt_pool);
error1:
  free(table);
error0:
  return NULL;
}

void
destroy_kv_table(kv_table* table)
{
  for(int i = 0; i < table->log_size; i++) {
    if (table->log[i] != NULL) {
      destroy_circular_log(table->log[i]);
    }
  }
  free(table);
}

bool
put_kv_table(kv_table* table, circular_log* log,  circular_log_entry* entry)
{
  bucket* bucket = get_entry_bucket(table, entry);
  return put_circular_log_entry(log, bucket, entry);  
}

bool
get_kv_table(kv_table* table, circular_log* log,  circular_log_entry* entry)
{
  bucket* bucket = get_entry_bucket(table, entry);
  return get_circular_log_entry(log, bucket, entry);
}

bool
delete_kv_table(kv_table* table, circular_log* log,  circular_log_entry* entry)
{
  bucket* bucket = get_entry_bucket(table, entry);  
  return remove_circular_log_entry(log, bucket, entry);
}
