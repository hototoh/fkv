#ifndef HASH_INDEX_H
#define HASH_INDEX_H

#include <stdint.h>
#include <stdbool.h>

typedef struct index_entry {
  uint64_t tag:16;
  uint64_t offset:48;
} index_entry;

#define ENTRY_SIZE 15
typedef struct bucket {
  uint64_t version;
  index_entry entries[ENTRY_SIZE];
} bucket;


#endif
