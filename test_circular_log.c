#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#include "common.h"
#include "circular_log.h"

#define LOG_MEM_SIZE (1UL << 29) // 1GB
#define LOG_FILE_NAME ("/mnt/hugetlbfs/test_circular_log")
#define BUCKET_SIZE 1024

void test_circular_log() {
  circular_log* log_table;
  log_table = create_circular_log(LOG_FILE_NAME,
                                  LOG_MEM_SIZE);
  if(log_table == NULL) {
    printf("Failed to create circular_log.");
    return ;
  }
    
  D("addr: %lx", (uint64_t) log_table->allocator->addr);
  destroy_circular_log(log_table);
}

void test_kv_table(uint64_t bucket_size) {
  kv_table *table = create_kv_table(bucket_size, NULL);
  if (table == NULL) {
    D("Fail to create kv_table");
    assert(false);
  }
  D("SUCCESS to create kv_table");

  D("member access check");
  print_addr(table->log, "log");
  D("bucket_size: %ld", (uint64_t) table->bucket_size);
  D("bucket: %lx", (uint64_t) table->buckets);  

  destroy_kv_table(table);
  D("SUCESS to destroy kv_table");
}

int
main(int argc, char** argv)
{
  D("TEST circular_log");
  test_circular_log();
  D("TEST key-value table");
  test_kv_table(BUCKET_SIZE);  

  return 0;
}
