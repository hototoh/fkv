#ifndef MEM_ALLOC_H
#define MEM_ALLOC_H

#include <stdint.h>
#include <stdbool.h>

#define SIZ_FILENAME 64


typedef struct mem_allocator {
  char filename[SIZ_FILENAME];
  int fd;
  uint64_t size;
  void* addr;
} mem_allocator;

#define NEW_mem_allocator create_mem_allocator
#define DEL_mem_allocator destroy_mem_allocator

mem_allocator*
create_mem_allocator(char* filename, uint64_t mem_size);

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
