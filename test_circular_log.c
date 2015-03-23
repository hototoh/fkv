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

#define LOG_MEM_SIZE (1UL << 30) // 512MB
#define LOG_FILE_NAME ("/mnt/hugetlbfs/test_circular_log")
#define BUCKET_SIZE 10

#define ITER_NUM 64
#define MAX_DATA_SIZE 1024

void test_index_entry()
{
  for(int i = 0; i < ITER_NUM; i++) {
    uint64_t index = 0xf0001UL;
    index_entry* entry = (index_entry*) &index;
    D("tag: %lu, offset, %Lu", entry->tag, entry->offset);
    entry->tag = i;
    D("tag: %lu, offset, %Lu", entry->tag, entry->offset);
  }
}

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
  D("START of put CKCV");
  for(int i = 0; i < ITER_NUM; i++) {
    circular_log_entry *entry = (circular_log_entry*) ((uint64_t) entries +
                                i * (sizeof(circular_log_entry) +
                                     sizeof(uint8_t) * MAX_DATA_SIZE));
    entry->key_length = key_length;
    rand_string((char*)entry->data, key_length);
    entry->val_length = val_length;
    rand_string((char*)entry->data+key_length, val_length);
    entry->keyhash = CityHash64((char*)entry->data, key_length);
    D("#%d data:%s, hash:%lu", i, entry->data, entry->keyhash);
    entry->initial_size = sizeof(circular_log_entry) + entry->key_length
                          + entry->val_length;
    put_kv_table(table, entry);
  }
  D("END of put CKCV");
}

void test_kv_put_CKVV(kv_table* table, circular_log_entry *entries,
                      uint32_t key_length)
{}

void test_kv_put_VKCV(kv_table* table, circular_log_entry *entries,
                      uint32_t val_length)
{}

void test_kv_put_VKVV(kv_table* table, circular_log_entry *entries)
{}
int j;

void test_kv_get(kv_table* table, circular_log_entry *entries)
{
  D("START of get");
  for(int i = 0; i < ITER_NUM; i++) {
    circular_log_entry* get_entry;
    circular_log_entry* entry;
    get_entry = (circular_log_entry*) malloc(sizeof(circular_log_entry) +
                                             sizeof(uint8_t) * MAX_DATA_SIZE);
    entry = (circular_log_entry*) ((uint64_t) entries +
                                   i * (sizeof(circular_log_entry) +
                                        sizeof(uint8_t) * MAX_DATA_SIZE));    
    // copy the key and other necessary information for GET
    uint64_t key_length = entry->key_length;
    memcpy((void*) get_entry->data, entry->data, key_length);    
    get_entry->key_length = key_length;
    get_entry->keyhash = CityHash64((char*)get_entry->data, key_length);
    get_kv_table(table, get_entry);
    
    if( i == 0 ) {
      print_circular_log_entry(entry);
      print_circular_log_entry(get_entry);
    }

    bool ret = equal_circular_log_entry(entry, get_entry);
    if(!ret) {
      D("NOT exists****************************************");
      j++;
      print_circular_log_entry(entry);
      print_circular_log_entry(get_entry);
    }
  }
  D("END of get");
}

void test_kv_table(uint8_t bucket_bits)
{
  circular_log_entry *entries;
  entries = (circular_log_entry*) malloc(ITER_NUM * (
      sizeof(circular_log_entry) +
      sizeof(uint8_t) * MAX_DATA_SIZE));
  if(entries == NULL) {
    D("allocation error");
    return ;
  }
  D("allocate %lu", sizeof(circular_log_entry)+sizeof(uint8_t) * MAX_DATA_SIZE);

  kv_table *table = create_kv_table(bucket_bits, NULL);
  if (table == NULL) {
    D("Fail to create kv_table");
    assert(false);
  }
  D("SUCCESS to create kv_table");

  print_addr(table->log, "log");
  D("bucket_size: %ld", (uint64_t) table->bucket_size);

  uint32_t key_length = 10, val_length = 15;
  test_kv_put_CKCV(table, entries, key_length, val_length);
  test_kv_get(table, entries);
  destroy_kv_table(table);
  free(entries);
  D("SUCESS to destroy kv_table");
}

int
main(int argc, char** argv)
{
  //test_index_entry();
  D("TEST circular_log");
  test_circular_log();
  D("TEST key-value table");
  test_kv_table(BUCKET_SIZE);  
  D("%d", j);
  return 0;
}
