#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "mem_alloc.h"
#include "circular_log.h"

circular_log*
create_circular_log(char* filename, uint64_t log_mem_size)
{
  char filename[SIZE_FILENAME];
  circular_log* log_table = (circular_log*) malloc(sizeof(circular_log));
  if (log_table == NULL) goto error0;
  memset(log_table, 0, sizeof(circular_log));

  mem_allocator* allocator;
  allocator = create_mem_allocator_with_addr(filename, log_mem_size, 0x0UL);
  if (allocator == NULL) goto error1;

  log_table->allocator = allocator;
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

  mem_allocator* allocator = log_table->allocator;
  destroy_mem_allocator(allocator);
  free(table);
}

bool
put_circular_log(circular_log* log_table, circular_log_entry* entry)
{
  
  // update log_table head & tail
  return true;
}

circular_log_entry*
get_circular_log(circular_log* log_table, circular_log_entry* entry)
{
  
  return NULL;
}

bool
delete_circular_log(circular_log* log_table, circular_log_entry* entry)
{
  
  // update log_table head & tail
  return true;
}
