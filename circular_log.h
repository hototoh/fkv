#ifndef CIRCULAR_H
#define CIRCULAR_H

#include <stdint.h>
#include <stdbool.h>

#include "shm.h"
#include "bucket.h"
#include "mem_manager.h"

#define CIRCULAR_LOG_FILE "/mnt/hugetlbfs/circular_log"
#define CIRCULAR_LOG_SIZE 1ULL << 29


#define OPTIMISTIC_LOCK(v, vv, x)                                       \
  do {                                                                  \
    v = x->version & (~0x1UL);                                          \
    vv = version | 0x1UL;                                               \
  } while(__sync_bool_compare_and_swap(&x->version, v, vv));

#define OPTIMISTIC_UNLOCK(x)                    \
  do {                                          \
    __sync_add_and_fetch(&x->version, 1);       \
  } while(0)

/* circular_log_entry should be multiples of 8 bytea
 * for memory alingment !?
 */
typedef struct circular_log_entry {
  uint64_t initial_size;

  /** key-hash used for multiple purpose
   * # For server queue selection : 16bits
        partition(exclusive) | core(not exclusive) index as UDP port
   * # For bucket selection : (16, 32)? bits 
   * # For filtering lookup keys : (16, 32)? bits 
        match index_entry->tag.
   */
  uint64_t keyhash;
  uint64_t key_length:32;
  uint64_t val_length:32;
  uint64_t expire;

  /* [key][value] this length should be 8 multiple */  
  uint8_t data[0];
} circular_log_entry;

static inline bool
equal_circular_log_entry_key(circular_log_entry* entry1, 
                            circular_log_entry* entry2)
{
  return (entry1->key_length == entry2->key_length &&
          memcmp(entry1->data, entry2->data, entry1->key_length) == 0);
}

static inline bool
equal_circular_log_entry(circular_log_entry* entry1, 
                         circular_log_entry* entry2)
{
  uint64_t data1_length = entry1->key_length + entry1->val_length;
  uint64_t data2_length = entry2->key_length + entry2->val_length;
  return (data1_length == data2_length && 
          memcmp(entry1->data, entry2->data, data1_length) == 0);
}

static void
print_circular_log_entry(circular_log_entry* entry)
{
  char key[1024], val[1024];
  uint64_t val_start = (uint64_t) entry->data + entry->key_length;
  memcpy(key, entry->data, entry->key_length);
  memcpy(val, (void*) val_start, entry->val_length);
  printf("**** entry ****\n"
         "initial_size:%lu\n"
         "keyhash     :%lu\n"
         "key_length  :%u\n"
         "val_length  :%u\n"
         "key         :%s\n"
         "val         :%s\n",
         entry->initial_size, entry->keyhash, entry->key_length,
         entry->val_length, key, val);
}

typedef struct circular_log {
  segregated_fits *sfits;
  bucket_pool* bkt_pool;
  void* base_addr;
  void* head;
  uint64_t size;
} circular_log;

circular_log*
create_circular_log(void* base_addr, void* head, uint64_t size,
                    bucket_pool* bkt_pool);

void
destroy_circular_log(circular_log* log_table);

static inline bucket*
get_entry_bucket(circular_log* log_table, circular_log_entry* entry)
{
  bucket_pool* pool = log_table->bkt_pool;
  uint64_t bucket_mask = pool->main_size - 1;
  uint64_t index = bucket_hash_portion(entry->keyhash) & bucket_mask;
  return &pool->mains[index];
}

bool
put_circular_log_entry(circular_log* log_table, circular_log_entry* entry);

bool
get_circular_log_entry(circular_log* log_table, circular_log_entry* entry);

bool
remove_circular_log_entry(circular_log* log_table, circular_log_entry* entry);

typedef struct kv_table {
  bucket_pool* bkt_pool;
  mem_allocator* allocator;
  uint64_t log_size;
  circular_log* log[0];
} kv_table;

kv_table*
create_kv_table(char* file, uint32_t nthread,  uint32_t main_size,
                uint32_t spare_size);

void
destroy_kv_table(kv_table* table);

#endif
