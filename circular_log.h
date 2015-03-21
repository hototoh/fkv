#ifndef CIRCULAR_H
#define CIRCULAR_H

#include <stdint.h>
#include <stdbool.h>

#include "mem_alloc.h"

#define CIRCULAR_LOG_FILE "/mnt/hugetlbfs/huge/circular_log"
#define CIRCULAR_LOG_SIZE 1ULL << 30

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


#endif
