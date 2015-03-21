#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#include "common.h"
#include "circular_log.h"

#define LOG_MEM_SIZE (1UL << 30) // 1GB
#define LOG_FILE_NAME ("/mnt/hugetlbfs/huge/test_circular_log")

void test_circular_log() {
  circular_log* log_table;
  log_table = create_circular_log(LOG_FILE_NAME,
                                  LOG_MEM_SIZE);
  if(log_table == NULL) {
    printf("Failed to create circular_log.");
    return -1;
  }
    
  D("addr: %lx", (uint64_t) log_table->allocator->addr);
}

void test_kv_table() {

}

int
main(int argc, char** argv)
{
  D("TEST circular_log");
  test_circular_log();
  D("TEST key-value table");
  test_kv_table();  

  return 0;
}
