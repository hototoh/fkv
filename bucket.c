#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "common.h"
#include "shm.h"
#include "bucket.h"

#define BUCKET_MEM_SIZE(x) (((x >> 17)+1) << 20)

/**
 * 1 bucket is 8 byte (2 << 3)
 * @main_size + @spare_size : must be multiples of 2 << 20
 */
bucket_pool*
create_bucket_pool(char* file, uint32_t main_size, uint32_t spare_size)
{
  bucket_pool* bkt_pool;
  bkt_pool = (bucket_pool*) malloc(sizeof(bucket_pool));
  if (bkt_pool == NULL) {
    D("Fail to create bucket_pool");
    goto error0;
  }

  uint32_t total_size = main_size + spare_size;
  uint64_t mem_size = BUCKET_MEM_SIZE(total_size);
  mem_allocator* allocator = create_mem_allocator(file, mem_size);
  if(allocator == NULL) {
    D("Fail to create mem_allocator for bucket");
    goto error1;
  }
 
  bkt_pool->main_size = main_size;
  bkt_pool->spare_size = spare_size;
  bkt_pool->allocator = allocator;
  bkt_pool->spare_cur = 0;
  bkt_pool->mains = allocator->addr;
  bkt_pool->spares = allocator->addr + sizeof(bucket) * main_size;
  return bkt_pool;  
error1:
  free(bkt_pool);
error0:
  return NULL;
}

static inline bucket*
next_empty_spare_bucket(bucket_pool* bkt_pool)
{
  bucket* bucket;
  uint32_t spare_size = bkt_pool->spare_size;
  for(unsigned int i = 0; i < spare_size; i++) {
    bucket = &bkt_pool->spares[i];
    if (bucket->version != 0) continue;
      
    if (__sync_bool_compare_and_swap(&bucket->version, 0, 2))
      return bucket;
  }
  return NULL;
}

static inline bucket*
next_spare_bucket(bucket* bkt)
{
  if(bkt->offset == 0)
    return NULL;
  return (bucket*)((uint64_t) bkt + (uint64_t) bkt->offset);  
}

void
destroy_bucket_pool(bucket_pool* bkt_pool)
{
  destroy_mem_allocator(bkt_pool->allocator);
  free(bkt_pool);
}

/**
 * we can call this function until return NULL.
 * we can search from where we left.
 * @_bucket: main or spare bucket.
 * @index: default 0.
 */
index_entry*
search_index_entry(bucket** _bucket, uint64_t keyhash, int* index)
{
  bucket* bkt = *_bucket;
  do {
    index_entry* entries = bkt->entries;
    for(int i = *index; i < BUCKET_ENTRY_SIZE; i++) {
      if(match_index_entry_tag(entries[i].tag, keyhash)) {
        // set current index to call next time.
        *index = i; 
        return &entries[i];
      }
    }
    
    *index = 0;
    bkt = *_bucket = next_spare_bucket(bkt);
  } while(bkt != NULL);

  return NULL;
}

void
delete_index_entry(bucket_pool* bkt_pool, bucket* _bucket, uint64_t keyhash,
                   uint64_t offset)
{
  bucket* bkt = _bucket;
  do {
    index_entry* entries = bkt->entries;
    for(int i = 0; i < BUCKET_ENTRY_SIZE; i++) {
      if (match_index_entry(&entries[i], keyhash, offset)) {
        entries[i].tag = 0;
        entries[i].offset = 0;
        return ;
      }
    }
    
    bkt = next_spare_bucket(bkt);
  } while(bkt != NULL);
  D("fail to delete index entry\n");
}

/**
 * XXX: we don't care the same key exists.
 * Before call this function, must check it out.
 */
bool
insert_index_entry(bucket_pool* bkt_pool, bucket* _bucket, uint64_t keyhash,
                   uint64_t offset)
{
  bucket* bkt = _bucket;
  do {
    index_entry* entries = bkt->entries;
    for(int i = 0; i < BUCKET_ENTRY_SIZE; i++) {
      if(is_empty_entry(&entries[i])) {
        entries[i].offset = offset;
        entries[i].tag = lookup_key_tag_hash_portion(keyhash);
        return true;
      }
    }
    
    bucket* next = next_spare_bucket(bkt);
    if(next != NULL) {
      bkt = next;
      continue;
    }
    
    next = next_empty_spare_bucket(bkt_pool);    
    if (next != NULL) {
      bkt->offset = (uint64_t)next - (uint64_t)bkt;
    } else {
      printf("no extra spare bucket\n");
    }
    bkt = next;
  } while(bkt != NULL);

  return false;
}

void
dump_bucket(bucket* bkt)
{
  do {
    for(int i = 0; i < BUCKET_ENTRY_SIZE; i++) {
      print_index_entry(&bkt->entries[i]);
    }
  
    bkt = next_spare_bucket(bkt);
    if(bkt != NULL)
      printf("******************* spare ***********************\n");
  } while (bkt != NULL);  
}
