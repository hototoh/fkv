#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <assert.h>

#include "common.h"
#include "shm.h"
#include "bucket.h"
#include "util.h"

#define ITER_NUM 1 << 16
#define MAIN_SIZE 8192
#define SPARE_SIZE 8192

int main() {
  bucket_pool* bkt_pool = create_bucket_pool("test_bkt", MAIN_SIZE, SPARE_SIZE);
  assert(bkt_pool != NULL);

  uint64_t *keyhash = calloc(ITER_NUM, sizeof(uint64_t));
  uint64_t *offset  = calloc(ITER_NUM, sizeof(uint64_t));
  assert(keyhash != NULL);
  assert(offset !=NULL);
  
  D("generate keyhash and offset");
  static int b = 0;
  for (int i = 0; i < ITER_NUM; i++) {
    keyhash[i] = rand_fast_integer(64);
    offset[i] = i;
  }
   
  D("insert index entries");
  sleep(1);
  for (int i = 0; i < ITER_NUM; i++) {
    uint64_t bucket_mask = MAIN_SIZE - 1;
    uint64_t index = bucket_hash_portion(keyhash[i]) & bucket_mask;
    bucket* bkt = &bkt_pool->mains[index];
    insert_index_entry(bkt_pool, bkt, keyhash[i], offset[i]);
  }
  
  D("dump bucket");
  sleep(1);
  for (int i = 0; i < MAIN_SIZE; i++) {
    bucket* bkt = &bkt_pool->mains[i];
    dump_bucket(bkt);
  }  

  D("search entries");
  sleep(1);
  for (int i = 0; i < ITER_NUM; i++) {
    bool flag = true;
    int j = 0;
    uint64_t bucket_mask = MAIN_SIZE - 1;
    uint64_t index = bucket_hash_portion(keyhash[i]) & bucket_mask;
    bucket* bkt = &bkt_pool->mains[index];    
    index_entry* entry;
    int bucket_index = 0;
    do {
      entry = search_index_entry(&bkt, keyhash[i], &bucket_index);
      if (entry != NULL) {
        if (match_index_entry(entry, keyhash[i], offset[i])) {
          flag = true;
          break;
        } else {
          printf("********** same tag entry ***********\n");
        }
      }
    } while(entry == NULL);
    assert(flag);
  }
  
  D("delete index entries");
  sleep(1);
  for (int i = 0; i < ITER_NUM; i++) {
    uint64_t bucket_mask = MAIN_SIZE - 1;
    uint64_t index = bucket_hash_portion(keyhash[i]) & bucket_mask;
    bucket* bkt = &bkt_pool->mains[index];
    delete_index_entry(bkt_pool, bkt, keyhash[i], offset[i]);
  }

  D("dump bucket.");
  sleep(1);
  for (int i = 0; i < MAIN_SIZE; i++) {
    bucket* bkt = &bkt_pool->mains[i];
    dump_bucket(bkt);
  }
  return 0;
}
