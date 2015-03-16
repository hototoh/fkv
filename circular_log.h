#ifndef CIRCULAR_H
#define CIRCULAR_H

#include <stdint.h>
#include <stdbool.h>

#include "mem_alloc.h"

typedef struct index_entry {
  uint64_t tag:16;
  uint64_t offset:48;
} index_entry;

#define ENTRY_SIZE 15
typedef struct bucket {
  uint64_t version;
  index_entry entries[ENTRY_SIZE];
} bucket;

/* circular_log_entry should be multiples of 8 byte
 * for memory alingment !?
 */
typedef struct circular_log_entry {
  uint64_t initial_size;
  uint64_t keyhash;
  uint64_t kv_length;
  uint64_t expire;

  /* [key][value] this length should be 8 multiple */  
  uint8_t data[0];
} circular_log_entry;

typedef struct circular_log {
  mem_allocator* allocator;
  // circular log size
  uint64_t len; 
  uint64_t head;
  uint64_t tail;  
} circular_log;

circular_log*
create_circular_log(char* filename, uint64_t log_mem_size);

void
destroy_circular_log(circular_log* log_table);

bool
put_circular_log(circular_log* log_table, circular_log_entry* entry);

circular_log_entry*
get_circular_log(circular_log* log_table, circular_log_entry* entry);

bool
delete_circular_log(circular_log* log_table, circular_log_entry* entry);


#endif
