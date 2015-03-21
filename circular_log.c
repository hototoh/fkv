#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "common.h"
#include "mem_alloc.h"
#include "circular_log.h"

/***** bucket *****/
static inline uint64_t 
bucket_index_tag(index_entry* keyhash);

static inline bool
is_empty_entry(index_entry* entry);

static inline bool
match_index_entry_tag(uint64_t tag, uint64_t keyhash);

static inline bool
insert_index_entry(bucket* bucket, uint64_t keyhash, uint64_t offset);

/***** circular_log_entry *****/
static inline bool
equal_circular_log_entry_key(circular_log_entry* entry1, 
                             circular_log_entry* entry2);

static inline bool
match_circular_log_entry_key(circular_log_entry* entry1, 
                             uint64_t addr, uint64_t offset);

static inline uint64_t
rss_queue_hash_portion(uint64_t keyhash);

static inline uint64_t
bucket_hash_portion(uint64_t keyhash);

static inline uint64_t
lookup_keys_tag_hash_portion(uint64_t keyhash);

/***** circular_log *****/
static inline bool
__get_circular_log_entry(circular_log* log_table, bucket* bucket, 
                         circular_log_entry* entry);

static inline void
delete_first_circular_log_entry(circular_log* log_table);

static inline uint64_t
empty_size(circular_log* log_table);

static inline void 
update_log_table_tail(circular_log* log_table, uint64_t offset);

/***** key-value table *****/
static inline bucket*
get_entry_bucket(kv_table* table, circular_log_entry* entry);

/***** bucket *****/
static inline uint64_t 
bucket_index_tag(index_entry* keyhash)
{
  return keyhash->tag;
}

static inline bool 
is_empty_entry(index_entry* entry)
{
  return ! entry->tag;
}

static inline bool
match_index_entry_tag(uint64_t tag, uint64_t keyhash)
{
  return !(tag ^ bucket_index_tag((index_entry*) keyhash));
}

static inline bool
insert_index_entry(bucket* bucket, uint64_t keyhash, uint64_t offset)
{
  index_entry entry = {
    .tag = lookup_keys_tag_hash_portion(keyhash),
    .offset = offset,
  };
  index_entry* entries = bucket->entries;
  for(int i = 0; i < BUCKET_ENTRY_SIZE; i++) {
    index_entry tmp = entries[i];
    if (is_empty_entry(&tmp)) {
      entries[i] = entry;
      return true;
    }
  }
  // XXX 
  return false;
}

/***** circular_log_entry *****/
static inline bool
equal_circular_log_entry_key(circular_log_entry* entry1, 
                            circular_log_entry* entry2)
{
  return (entry1->key_length == entry2->key_length &&
          memcmp(entry1, entry2, entry1->key_length) == 0);
}

static inline bool
match_circular_log_entry_key(circular_log_entry* entry1, 
                             uint64_t addr, uint64_t offset)
{
  circular_log_entry* entry2;
  entry2 = (circular_log_entry*) addr + offset;
  return equal_circular_log_entry_key(entry1, entry2);
}

// 16bit
static inline uint64_t
rss_queue_hash_portion(uint64_t keyhash)
{
  return (keyhash >> 48) & 0xffff;
}

// 32bit
static inline uint64_t
bucket_hash_portion(uint64_t keyhash)
{
  return (keyhash >> 16) & 0xffffffff;
}

// 16bit
static inline uint64_t
lookup_keys_tag_hash_portion(uint64_t keyhash)
{
  return keyhash & 0xffff;
}

/***** circular_log *****/
#if 0
static inline void
delete_index_entry(bucket* bucket, index_entry entry)
{
  index_entry* entries = bucket->entries;
  
}
#endif
static inline void
delete_first_circular_log_entry(circular_log* log_table)
{
  
}

static inline uint64_t
empty_size(circular_log* log_table)
{
  uint64_t size = log_table->tail - log_table->head;
  return size >= 0 ? size : size + log_table->len;
}

static inline void 
update_log_table_tail(circular_log* log_table, uint64_t offset)
{
  // XXX 
  uint64_t length = log_table->len;
  log_table->tail += offset;
  if (log_table->tail > length)
    log_table->tail -= length;
}

circular_log*
create_circular_log(char* filename, uint64_t log_mem_size)
{
  circular_log* log_table = (circular_log*) malloc(sizeof(circular_log));
  if (log_table == NULL) goto error0;
  memset(log_table, 0, sizeof(circular_log));

  mem_allocator* allocator;
  allocator = create_mem_allocator_with_addr(filename, log_mem_size,
                                             (void*)0x0UL);
  if (allocator == NULL) goto error1;

  log_table->allocator = allocator;
  return log_table;
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

  mem_allocator* allocator = log_table->allocator;
  destroy_mem_allocator(allocator);
  free(log_table);
}

/* before calling this function, MUST check if the key exist already in the log
 * or not and get the bucket pointer at that time.
 */
bool
put_circular_log_entry(circular_log* log_table, bucket* bucket, 
                       circular_log_entry* entry)
{
  index_entry tmp;
  int free_index = -1, bucket_index = -1;
  for (int i = 0; i < BUCKET_ENTRY_SIZE; i++) {
    tmp = bucket->entries[i];
    // get the minimum free index of entry.
    if (is_empty_entry(&tmp)){ 
      if(free_index < 0)
        free_index = i;
      continue;
    }

    // if the same key entry exists, preserve the index.
    if (match_index_entry_tag(tmp.tag, entry->keyhash)) {
      if (!match_circular_log_entry_key(entry, log_table->addr, tmp.offset))
        continue;

      bucket_index = i;
      break;
    }
  }
  if(bucket_index < 0) {
    if (free_index < 0) {
      // XXX
      free_index = 0;
    }
    bucket_index = free_index;
  }
  
  uint64_t version, new_version;
  OPTIMISTIC_LOCK(version, new_version, bucket);
  // make sure to have enough space to put an entry.
  while (entry->initial_size < empty_size(log_table)) {
    delete_first_circular_log_entry(log_table);
  }
  
  // copy data to the log
  memcpy((void*) (log_table->tail+log_table->addr), entry, entry->initial_size);
  
  // update log tail & bucket
  // XXX TAG+offset = keyhash(!?)
  insert_index_entry(bucket, entry->keyhash, log_table->tail);
  update_log_table_tail(log_table, entry->initial_size);

  OPTIMISTIC_UNLOCK(bucket);
  
  return true;
}

static inline bool
__get_circular_log_entry(circular_log* log_table, bucket* bucket, 
                         circular_log_entry* entry)
{
  index_entry tmp;
  for(int i = 0; i < BUCKET_ENTRY_SIZE; i++) {
    tmp = bucket->entries[i];
    if (match_index_entry_tag(tmp.tag, entry->keyhash)) {
      if (!match_circular_log_entry_key(entry, log_table->addr, tmp.offset))
        continue;
      
      circular_log_entry* _entry;
      _entry = (circular_log_entry*) log_table->addr + tmp.offset;
      memcpy(entry, _entry, _entry->initial_size);
      return true;
    }
  }
  return false;
}

/* search the key in circular_log.
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
    version_begining= bucket->version;
    ret = __get_circular_log_entry(log_table, bucket, entry);
    version_end = bucket->version;
  } while(version_begining != version_end);
  return ret;
}

bool
remove_circular_log_entry(circular_log* log_table, bucket* bucket, 
                          circular_log_entry* entry)
{  
  // update log_table head & tail
  return true;
}

/***** key-value table *****/
static inline bucket*
get_entry_bucket(kv_table* table, circular_log_entry* entry)
{
  uint64_t bucket_size = table->bucket_size;
  uint64_t index = bucket_hash_portion(entry->keyhash) ^ bucket_size;  
  return &table->buckets[index];
}

kv_table*
create_kv_table(uint64_t bucket_size, circular_log* log)
{
  kv_table* table;
  table = (kv_table*) malloc(sizeof(kv_table)+ sizeof(bucket)* bucket_size);
  if (table == NULL) goto error0;

  table->bucket_size = bucket_size;
  if (log == NULL) {
    log = create_circular_log(CIRCULAR_LOG_FILE, CIRCULAR_LOG_SIZE);
    if (log == NULL) goto error1;    
  }
  table->log = log;
  return table;
error1:
  free(table);
error0:
  return NULL;
}

void
destroy_kv_table(kv_table* table)
{
  destroy_circular_log(table->log);  
  free(table);
}

bool
put_kv_table(kv_table* table, circular_log_entry* entry)
{
  bucket* bucket = get_entry_bucket(table, entry);
  return put_circular_log_entry(table->log, bucket, entry);  
}

bool
get_kv_table(kv_table* table, circular_log_entry* entry)
{
  bucket* bucket = get_entry_bucket(table, entry);
  return get_circular_log_entry(table->log, bucket, entry);
}

bool
delete_kv_table(kv_table* table, circular_log_entry* entry)
{
  bucket* bucket = get_entry_bucket(table, entry);  
  return remove_circular_log_entry(table->log, bucket, entry);
}
