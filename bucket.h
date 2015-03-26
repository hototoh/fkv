#ifndef BUCKET_H
#define BUCKET_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h> // PATH_MAX

#include "shm.h"

// 16bit
static inline uint64_t
lookup_keys_tag_hash_portion(uint64_t keyhash)
{
  uint64_t tag = keyhash & 0xffffUL;
  return tag == 0 ? ~tag : tag;
}

// 32bit
static inline uint64_t
bucket_hash_portion(uint64_t keyhash)
{
  return (keyhash >> 16) & 0xffffffffUL;
}

typedef struct index_entry {
  /* value 0 means empty */ 
  uint64_t tag:16;
  /* partition offset */
  uint64_t offset:48;
} index_entry;

static inline bool 
is_empty_entry(index_entry* entry)
{
  return ! entry->tag;
}

static inline bool
match_index_entry_tag(uint64_t tag, uint64_t keyhash)
{
  uint64_t tag1 = lookup_keys_tag_hash_portion(keyhash); 
  return !(tag ^ tag1);
}

static inline bool
match_index_entry(index_entry *entry, uint64_t keyhash,
                  uint64_t offset);

#define BUCKET_ENTRY_SIZE 15
// 128 (8 x 16) byte
typedef struct bucket {
  // during updating a bucket entry, keep version odd.
  // value 0 is special number, it means empty spare bucket
  uint32_t version;
  union {
    // if this is 0, no extra bucket.
    // else, the value denote the memory address offset.
    // 32bit = 4GB offset enough, maybe
    uint32_t offset;
    uint32_t cur;
  };

  index_entry entries[BUCKET_ENTRY_SIZE];
}__attribute__((aligned(128))) bucket;
bool
insert_index_entry(bucket* bucket, uint64_t keyhash,
                   uint64_t offset, int free_index);

/** XXX
 * Now we don't care the regression pattern.
 * For example, the case that a spare bucket is full but 
 * main bucket is empty will occur.
 */
typedef struct bucket_pool {
  uint32_t main_size;
  uint32_t spare_size;
  uint32_t spare_cur;
  mem_allocator* allocator;
  bucket* mains;
  bucket* spares;
} bucket_pool;

#endif
