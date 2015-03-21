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

#ifndef DEBUG
static inline void print_addr(void* addr, char* var) {
  printf("addr: 0b");
  for(int i = 0; i < 64; i++) {
    uint64_t _addr = (uint64_t) addr >> i;
    printf("%1d", (int) _addr & 1);
  }
  printf(" : %s\n", var);
}
#endif

#endif
