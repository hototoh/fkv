#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#include "common.h"
#include "city.h"
#include "util.h"
#include "circular_log.h"

#define LOG_MEM_SIZE (1UL << 29) // 1GB
#define LOG_FILE_NAME ("/mnt/hugetlbfs/test_circular_log")
#define BUCKET_SIZE 1024

#define ITER_NUM 1024
#define MAX_DATA_SIZE 1024

void test_circular_log()
{
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

/** 
 * C: Constant
 * V: Variable
 * K: Key
 * V: Value
 */
void test_kv_put_CKCV(kv_table* table, circular_log_entry *entries,
                      uint32_t key_length, uint32_t val_length)
{
  for(int i = 0; i < ITER_NUM; i++) {
    circular_log_entry *entry = &entries[i];
    entry->key_length = key_length;    
    rand_string((char*)entry->data, key_length);
    entry->val_length = val_length;
    rand_string((char*)entry->data+key_length, val_length);
    entry->keyhash = CityHash64((char*)entry->data, key_length);
    put_kv_table(table, entry);
  }
}

void test_kv_put_CKVV(kv_table* table, circular_log_entry *entries,
                      uint32_t key_length)
{}

void test_kv_put_VKCV(kv_table* table, circular_log_entry *entries,
                      uint32_t val_length)
{}

void test_kv_put_VKVV(kv_table* table, circular_log_entry *entries)
{}

void test_kv_get(kv_table* table, circular_log_entry *entries)
{
  for(int i = 0; i < ITER_NUM; i++) {
    circular_log_entry entry;
    circular_log_entry* _entry = &entries[i];
    uint64_t key_length = _entry->key_length;
    memcpy((char*)entry.data, _entry->data, key_length);
    entry.keyhash = CityHash64((char*)entry.data, key_length);
    get_kv_table(table, &entry);
    bool ret = equal_circular_log_entry(&entries[i], &entry);
    if(!ret)
      print_circular_log_entry(&entry);
  }
}

void test_kv_table(uint64_t bucket_size)
{
  circular_log_entry *entries;
  entries = (circular_log_entry*) calloc(ITER_NUM, 
                                         sizeof(circular_log_entry) +
                                         sizeof(uint8_t) * MAX_DATA_SIZE);
                                
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

  uint32_t key_length = 10, val_length = 10;
  test_kv_put_CKCV(table, entries, key_length, val_length);
  test_kv_get(table, entries);
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
