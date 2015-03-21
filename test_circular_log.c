#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "circular_log.h"

int
main(int argc, char** argv)
{
  circular_log* log_table;
  log_table = create_circular_log("/mnt/hugetlbfs/huge/test_circular_log");
  if(log_table == NULL) {
    printf("Failed to create circular_log.");
    return -1;
  }
    
  D("addr: %lx", log_table->allocator->addr);
  
  destroy_mem_allocator(log_table);a
  return 0;
}
