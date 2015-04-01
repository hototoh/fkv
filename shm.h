#ifndef SHM_H
#define SHM_H

#include <limits.h> 
#include <linux/limits.h> // PATH_MAX
#include <stdlib.h> 

#define HUGEPAGE_PREFIX "/mnt/hugetlbfs"

typedef struct mem_allocator {
  char filename[PATH_MAX];
  int fd;
  uint64_t size;
  void* addr;
} mem_allocator;

#define NEW_mem_allocator create_mem_allocator
#define DEL_mem_allocator destroy_mem_allocator

mem_allocator*
create_mem_allocator_with_addr(char* filename, uint64_t mem_size, void* addr);

static mem_allocator*
create_mem_allocator(char* filename, uint64_t mem_size)
{
  return create_mem_allocator_with_addr(filename, mem_size, NULL);
}

void
destroy_mem_allocator(mem_allocator* allocator);




#endif
