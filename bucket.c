#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "common.h"
#include "shm.h"
#include "bucket.h"


#define BUCKET_MEM_SIZE(x) ((x >> 17)+1) << 20
/**
 * 1 bucket is 8 byte (2 << 3)
 * @main_size + @spare_size : must be multiples of 2 << 20
 */
bucket_pool*
create_bucket_pool(char* file, uint32_t main_size, uint32_t spare_size)
{
  bucket_pool* bucket_pool;
  bucket_pool = (bucket_pool*) malloc(sizeof(bucket_pool));
  if (bucket_pool == NULL) {
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
 
  bucket_pool->main_size = main_size;
  bucket_pool->spare_size = spare_size;
  bucket_pool->allocator = allocator;
  bucket_pool->spare_cur = 0;
  bucket_pool->mains = allocator->addr;
  bucket_pool->spare = allocator->addr + sizeof(bucket_size) * main_size;
  return bucket_pool;
  
error1:
  free(bucket_pool);
error0:
  return NULL;
}

static inline bucket*
next_emptry_spare_bucket(bucket_pool* bucket_pool)
{
  bucket* bucket;
  for(int i = 0; i < spare_size; i++) {
    bucket = &bucket_pool->spares[i];
    if (bucket->version != 0) continue;
      
    if (__sync_bool_compare_and_swap(&bucket->version, 0, 2))
      return bucket;
  }
  return NULL;
}

static inline bucket*
next_spare_bucket(bucket* bucket)
{
  return (bucket*)((uint64_t) bucket + (uint64_t) bucket->offset);
}

void
destroy_bucket_pool(bucket_pool* bucket_pool)
{
  destroy_mem_allocator(bucket_pool->allocator);
  free(bucket_pool);
}

index_entry*
search_index_entry(bucket** _bucket, uint64_t keyhash, int* index)
{
  bucket* bucket = *_bucket;
  do {
    index_entry* entries = bucket->entries;
    for(int i = *index; i < BUCKET_ENTRY_SIZE; i++) {
      if(match_index_entry_tag(entries[i].tag, keyhash)) {
        *index = i + 1; // next index
        return &entries[i];
      }
    }
    
    if(bucket->offset == 0) break;

    bucket = *_bucket = next_spare_bucket(bucket);
    *index = 0;
  } while(bucket != NULL);

  return NULL;
}

void
delete_index_entry(bucket_pool* bucket_pool, bucket* _bucket, uint64_t keyhash,
                   uint64_t offset)
{
  bucket* bucket = _bucket;
  do {
    index_entry* entries = bucket->entries;
    for(int i = 0; i < BUCKET_ENTRY_SIZE; i++) {
      if (!match_index_entry(&entries[i], keyhash, offset)) {
        entries[i].tag = 0;
        entries[i].offset = 0;
        return ;
      }
    }
    
    if(bucket->offset == 0) break;

    bucket = next_spare_bucket(bucket);
  } while(bucket != NULL);
}

/**
 * we don't care the same key exists.
 * Before call this function, must check it out.
 */
bool
insert_index_entry(bucket_pool* bucket_pool, bucket* _bucket, uint64_t keyhash,
                   uint64_t offset)
{
  bucket* bucket = _bucket;
  do {
    index_entry* entries = bucket->entries;
    for(int i = 0; i < BUCKET_ENTRY_SIZE; i++) {
      if(is_empty_entry(&entries[i])) {
        entries[i].tag = lookup_keys_tag_hash_portion(keyhash);
        entries[i].offset = offset;
        return true;
      }
    }
    
    if(bucket->offset != 0) {// if sapre bucket exists
      bucket = next_spare_bucket(bucket);
      continue;
    }

    bucket* spare_bucket = next_empty_spare_bucket(bucket_pool);
    if (spare_bucket != NULL) {
      bucket->offset = (uint64_t)spare_bucket - (uint64_t)bucket;
      bucket = spare_bucket;
    } else {
      bucket = NULL;
    }
  } while(bucket != NULL);

  return false;
}

/**
 * insert operation in store mode.
 */
static inline bool
__insert_index_entry(bucket* bucket, uint64_t keyhash, uint64_t offset, 
                   int free_index)
{
  index_entry entry = {
    .tag = (uint16_t) lookup_keys_tag_hash_portion(keyhash),
    .offset = offset,
  };
  if (free_index >= 0) {
    bucket->entries[free_index] = entry;
    return true;
  }
  /** XXX 
   * in cache operation mode, evict the oldest item,
   *
   */
  return false;
}
