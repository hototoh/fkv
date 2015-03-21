#ifndef MEM_ALLOC_H
#define MEM_ALLOC_H

#include <stdint.h>
#include <stdbool.h>

#define SIZE_FILENAME 64


typedef struct mem_allocator {
  char filename[SIZE_FILENAME];
  int fd;
  uint64_t size;
  void* addr;
} mem_allocator;

#define NEW_mem_allocator create_mem_allocator
#define DEL_mem_allocator destroy_mem_allocator

mem_allocator*
create_mem_allocator_with_addr(char* filename, uint64_t _mem_size, void* _addr);

static mem_allocator*
create_mem_allocator(char* filename, uint64_t mem_size)
{
  return create_mem_allocator_with_addr(filename, mem_size, NULL);
}

void
destroy_mem_allocator(mem_allocator* allocator);

#endif
