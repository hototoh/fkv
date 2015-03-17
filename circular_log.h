#ifndef CIRCULAR_H
#define CIRCULAR_H

#include <stdint.h>
#include <stdbool.h>

#include "mem_alloc.h"

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
  uint64_t head;
  uint64_t tail;  
} circular_log;

circular_log*
create_circular_log(uint64_t log_size);

void
destroy_circular_log(circular_log* log_table);

bool
put_circular_log(circular_log* log_table, circular_log_entry* entry);

circular_log_entry*
get_circular_log(circular_log* log_table, circular_log_entry* entry);

bool
delete_circular_log(circular_log* log_table, circular_log_entry* entry);


#endif
