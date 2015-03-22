#ifndef CIRCULAR_H
#define CIRCULAR_H

#include <stdint.h>
#include <stdbool.h>

#include "mem_alloc.h"

#define CIRCULAR_LOG_FILE "/mnt/hugetlbfs/circular_log"
#define CIRCULAR_LOG_SIZE 1ULL << 29

typedef struct index_entry {
  /* value 0 means empty */ 
  uint64_t tag:16;
  uint64_t offset:48;
} index_entry;

#define BUCKET_ENTRY_SIZE 15
typedef struct bucket {
  uint64_t version;
  index_entry entries[BUCKET_ENTRY_SIZE];
} bucket;

#define OPTIMISTIC_LOCK(v, vv, x)                                       \
  do {                                                                  \
    v = x->version & (~0x1ULL);                                         \
    vv = version | 0x1ULL;                                              \
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
          memcmp(entry1, entry2, entry1->key_length) == 0);
}

static inline bool
equal_circular_log_entry(circular_log_entry* entry1, 
                         circular_log_entry* entry2)
{
  uint64_t data1_length = entry1->key_length + entry1->val_length;
  uint64_t data2_length = entry2->key_length + entry2->val_length;
  return (data1_length == data2_length && 
          memcmp(entry1, entry2, data1_length));
}

static void
print_circular_log_entry(circular_log_entry* entry)
{
  char key[1024], val[1024];
  memcpy(key, entry->data, entry->key_length);
  memcpy(val, entry->data+entry->key_length, entry->val_length);
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
  mem_allocator* allocator;
  // circular log size
  uint64_t addr;
  uint64_t len; 
  uint64_t head;
  uint64_t tail;  
} circular_log;

circular_log*
create_circular_log(char* filename, uint64_t log_mem_size);

void
destroy_circular_log(circular_log* log_table);

bool
put_circular_log_entry(circular_log* log_table, bucket* bucket,
                       circular_log_entry* entry);

bool
get_circular_log_entry(circular_log* log_table, bucket* bucket,
                       circular_log_entry* entry);

bool
remove_circular_log_entry(circular_log* log_table, bucket* bucket,
                          circular_log_entry* entry);

typedef struct kv_table {
  circular_log* log;
  uint64_t bucket_size;
  bucket buckets[0];
} kv_table;

kv_table*
create_kv_table(uint64_t bucket_size, circular_log* log);

void
destroy_kv_table(kv_table* table);

bool
put_kv_table(kv_table* table, circular_log_entry* entry);

bool
get_kv_table(kv_table* table, circular_log_entry* entry);

bool
delete_kv_table(kv_table* table, circular_log_entry* entry);

#endif
