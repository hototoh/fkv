#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <assert.h>

#include "common.h"
#include "city.h"
#include "util.h"
#include "bucket.h"
#include "mem_manager.h"
#include "circular_log.h"

//#define LOG_MEM_SIZE (1UL << 29) // 512MB
#define LOG_FILE_NAME ("/mnt/hugetlbfs/test_circular_log")
#define BUCKET_SIZE 14

#define ITER_NUM 10240000
#define ITER_NUM 491520
#define MAX_DATA_SIZE 1024

unsigned int miss_count = 0;;

/* void test_index_entry() */
/* { */
/*   for(int i = 0; i < ITER_NUM; i++) { */
/*     uint64_t index = 0xf0001UL; */
/*     index_entry* entry = (index_entry*) &index; */
/*     D("tag: %lu, offset, %Lu", entry->tag, entry->offset); */
/*     entry->tag = i; */
/*     D("tag: %lu, offset, %Lu", entry->tag, entry->offset); */
/*   } */
/* } */

/** 
 * C: Constant
 * V: Variable
 * K: Key
 * V: Value
 */
void test_kv_CKCV(kv_table* table, uint32_t key_length, uint32_t val_length)
{
  D("START of put CKCV");
  uint16_t thread_mask = (uint16_t) table->log_size - 1;
  for(uint64_t i = 0; i < ITER_NUM; i++) {
    circular_log_entry* entry = (circular_log_entry*) malloc(
        sizeof(circular_log) + sizeof(char) * (key_length + val_length));
    entry->key_length = key_length;
    entry->val_length = val_length;
    memcpy(entry->data, &i, key_length);
    memcpy((entry->data + key_length), &i, val_length);
    entry->keyhash = CityHash64((char*)entry->data, key_length);
    entry->initial_size = sizeof(circular_log_entry) + entry->key_length
                          + entry->val_length;
    uint16_t log_index = (uint16_t) rss_queue_hash_portion(entry->keyhash) 
                         & thread_mask;
    circular_log* log_table = table->log[log_index];
    // D("put entry to LOG[ %d ] ", log_index);
    put_circular_log_entry(log_table, entry);
  }
  D("END of put CKCV");

  D("START of get CKCV");
  uint64_t entry_size = sizeof(circular_log_entry) + key_length + val_length;
  for(uint64_t i = 0; i < ITER_NUM; i++) {
    circular_log_entry* entry = (circular_log_entry*) malloc(
        sizeof(circular_log_entry) + (sizeof(char) * (key_length + val_length))
                                                             );
    entry->key_length = key_length;
    memcpy(entry->data, &i, key_length);
    uint64_t keyhash = CityHash64((char*)entry->data, key_length);
    entry->keyhash = keyhash;
    uint16_t log_index = (uint16_t) rss_queue_hash_portion(entry->keyhash)
                         & thread_mask;
    circular_log* log_table = table->log[log_index];
    get_circular_log_entry(log_table, entry);
    assert(entry->initial_size == entry_size);
    assert(entry->key_length == key_length);
    assert(entry->val_length == val_length);
    assert(entry->keyhash == keyhash);
    assert((*(uint64_t*) entry->data) == i);
    assert((*((uint64_t*)(entry->data + key_length))) == i);
  }
  D("END of get CKCV");
}

void test_kv_CKVV(kv_table* table, circular_log_entry *entries,
                      uint32_t key_length)
{}

void test_kv_VKCV(kv_table* table, circular_log_entry *entries,
                      uint32_t val_length)
{}

void test_kv_VKVV(kv_table* table, circular_log_entry *entries)
{}

#define KV_FILE_NAME "test_kv_table"
void test_kv_table(uint8_t bucket_bits)
{
  uint32_t key_length = 8, val_length = 8;
  long nthread = sysconf(_SC_NPROCESSORS_ONLN);
  D("core num: %u", nthread);
  uint32_t main_bucket_size = 6 << 18;
  uint32_t spare_bucket_size = 2 << 18;
  kv_table *table = create_kv_table("test_kv_table", nthread, 
                                    main_bucket_size, spare_bucket_size);
  if (table == NULL) {
    assert("Fail to create kv_table" && false);
  }
  D("SUCCESS to create kv_table");
  
  for ( uint32_t i = 0; i < main_bucket_size; i++) {
    //D("TABLE main bucket[%d] addr: %lx", i, &table->bkt_pool->mains[i]);   
  }
  
  test_kv_CKCV(table, key_length, val_length);

  destroy_kv_table(table);
  D("SUCESS to destroy kv_table");
}

int
main(int argc, char** argv)
{
  //test_index_entry();
  D("TEST key-value table");
  test_kv_table(BUCKET_SIZE);  
  D("misscount: %u / %u", miss_count, ITER_NUM);
  return 0;
}
