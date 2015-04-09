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

extern bool DEBUG;

/***** circular_log_entry *****/
static inline bool
match_circular_log_entry_key(circular_log_entry* entry1, 
                             uint64_t addr, uint64_t offset);

/***** circular_log *****/
static inline bool
__remove_circular_log_entry(circular_log* log_table, bucket* bkt,
                            circular_log_entry* entry);

static inline bool
__get_circular_log_entry(circular_log* log_table, bucket* bucket, 
                         circular_log_entry* entry);

/***** circular_log_entry *****/
static inline bool
match_circular_log_entry_key(circular_log_entry* entry1, 
                             uint64_t addr, uint64_t offset)
{
  circular_log_entry* entry2;
  entry2 = (circular_log_entry*) (addr + offset);
  return equal_circular_log_entry_key(entry1, entry2);
}

/***** circular_log *****/
circular_log*
create_circular_log(void* base_addr, void* head, uint64_t size,
                    bucket_pool* bkt_pool)
{
  uint32_t log_mem_size = (uint32_t) size;
  circular_log* log_table = (circular_log*) malloc(sizeof(circular_log));
  if (log_table == NULL) goto error0;
  memset(log_table, 0, sizeof(circular_log));

  segregated_fits* sfits = create_segregated_fits(SEGREGATED_MAX_DATA_SIZE);
  if (sfits == NULL) goto error1;

  sfits->addr = (uint64_t) head;
  sfits->addr_size = log_mem_size;
  void* addr = head;
  do {
    int res = segregated_fits_reclassing(sfits, &addr, &log_mem_size);
    if (res > 0) break;
  } while(1);

  log_table->sfits = sfits;
  log_table->bkt_pool = bkt_pool;
  log_table->base_addr = base_addr;
  log_table->head = head;
  log_table->size  = size;
  return log_table;
error1:
  free(log_table);
error0:
  return NULL;
}

void
destroy_circular_log(circular_log* log_table)
{  
  if(log_table == NULL) return;

  destroy_segregated_fits(log_table->sfits);
  free(log_table);
}

/**
 * This function must call in Optimistic lock
 */
static inline bool
__remove_circular_log_entry(circular_log* log_table, bucket* bkt,
                            circular_log_entry* entry)
{  
  int index = 0;
  index_entry *i_entry;
  uint64_t keyhash = entry->keyhash;
  void* old_entry = NULL;
  
  do {
    i_entry = search_index_entry(&bkt, keyhash, &index);
    if (i_entry != NULL) {
      if (match_index_entry_tag(i_entry->tag, keyhash)) {
        uint64_t offset = bkt->entries[index].offset;
        if (match_circular_log_entry_key(entry, (uint64_t) log_table->base_addr,
                                         offset)) {
          delete_index_entry_with_index(bkt, index);
          old_entry = (void*)((uint64_t)log_table->base_addr + offset);
          free_segregated_fits_block(log_table->sfits, 
                                     (segregated_fits_list*)old_entry);
          return true;
        }
      }
    }
    index++;
  } while(i_entry != NULL);

  return false;
}

bool
remove_circular_log_entry(circular_log* log_table, circular_log_entry* entry)
{  
  bucket* bkt = get_entry_bucket(log_table, entry);
  uint64_t version, new_version;
  OPTIMISTIC_LOCK(version, new_version, bkt);

  bool res = __remove_circular_log_entry(log_table, bkt, entry);

  OPTIMISTIC_UNLOCK(bkt);
  return res;
}


/**
 * before calling this function, MUST check if the key exist already in the log
 * or not and get the bucket pointer at that time.
 */
bool
put_circular_log_entry(circular_log* log_table, circular_log_entry* entry)
{
  bucket* bkt = get_entry_bucket(log_table, entry);
  uint64_t version, new_version;
  OPTIMISTIC_LOCK(version, new_version, bkt);
  __remove_circular_log_entry(log_table, bkt, entry);

  segregated_fits *sfits = log_table->sfits;
  uint32_t entry_size = (uint32_t) entry->initial_size;
  void* new_addr = get_segregated_fits_block(sfits, entry_size);
  if (new_addr == NULL) {
    if (DEBUG)
      printf("could not get new addr\n");
    return false;
  }

  uint64_t offset = (uint64_t) new_addr - (uint64_t) log_table->base_addr;
  memcpy(new_addr, entry, entry_size);
  insert_index_entry(log_table->bkt_pool, bkt, entry->keyhash, offset);

  OPTIMISTIC_UNLOCK(bkt);

  return true;
}

/**
 * This function callee must check the version consistency of bucket.
 */
static inline bool
__get_circular_log_entry(circular_log* log_table, bucket* bkt, 
                         circular_log_entry* entry)
{
  int index = 0;
  index_entry *i_entry;
  uint64_t keyhash = entry->keyhash;
  circular_log_entry* dst_entry;
  if(DEBUG)
    D("[SEARCHING] key:%lu keyhash: %lu", *(uint64_t*)entry->data, entry->keyhash);
  do {
    i_entry = search_index_entry(&bkt, keyhash, &index);
    if (i_entry != NULL) {
      if (match_index_entry_tag(i_entry->tag, keyhash)) {
        // calc from offset and base pointer
        uint64_t offset = bkt->entries[index].offset;
        // check the key is the same
        if (match_circular_log_entry_key(entry, (uint64_t)log_table->base_addr,
                                         offset)) {
          dst_entry = (circular_log_entry*)((uint64_t)log_table->base_addr
                                            + offset);
          memcpy(entry, dst_entry, dst_entry->initial_size);
          return true;
        }
      }
    }
    index++;
  } while(i_entry != NULL);
  
  if(DEBUG)
    D("404: NOT FOUND key:%lu keyhash:%lx", 
    *(uint64_t*)entry->data,
    entry->keyhash);

  return false;
}

/**
 * search the key in circular_log.
 * @entry:input set key & hashkey
 * @entry:ouput matched log entry.
 */
bool
get_circular_log_entry(circular_log* log_table, circular_log_entry* entry)
{
  bool ret;
  uint64_t version_begining, version_end;
  bucket* bkt = get_entry_bucket(log_table, entry);
  do {
    version_begining = bkt->version;
    ret = __get_circular_log_entry(log_table, bkt, entry);
    version_end = bkt->version;
  } while(version_begining != version_end);
  return ret;
}

/***** key-value table *****/
kv_table*
create_kv_table(char* file, uint32_t nthread, uint32_t main_size,
                uint32_t spare_size)
{  
  kv_table* table = (kv_table*) malloc(sizeof(kv_table) +
                                       sizeof(circular_log*) * nthread);
  if (table == NULL) goto error0;
  memset(table, 0, sizeof(kv_table) + sizeof(circular_log*) * nthread);

  bucket_pool* bkt_pool = create_bucket_pool(file, main_size, spare_size);
  if (bkt_pool == NULL) goto error1;

  mem_allocator* allocator;
  allocator = create_mem_allocator(file, CIRCULAR_LOG_SIZE * nthread);
  if (allocator == NULL) goto error2;

  for(int i = 0; i < nthread; i++) {
    void* head = (void*)((uint64_t) allocator->addr + CIRCULAR_LOG_SIZE * i);
    circular_log* log_table = create_circular_log(allocator->addr, head,
                                                  CIRCULAR_LOG_SIZE, bkt_pool);
    if (log_table == NULL) {
      goto error3;
    }
    table->log[i] = log_table;
  }

  table->allocator = allocator;
  table->bkt_pool = bkt_pool;
  table->log_size = nthread;
  return table;
error3:
  for(int i = 0; i < nthread; i++)
    if (table->log[i] != NULL)
      destroy_circular_log(table->log[i]);
  destroy_mem_allocator(allocator);
error2:
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
  destroy_mem_allocator(table->allocator);
  destroy_bucket_pool(table->bkt_pool);
  free(table);
}
