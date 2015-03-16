#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h> // getpagesize
#include <assert.h>

#include "common.h"
#include "mem_alloc.h"

#define FILENAME "/mnt/hugetlbfs/huge/test"
#define MM_SIZE (256UL*1024*1024)

void write_data(mem_allocator* allocator) {
  uint64_t size = allocator->size;
  char* addr = allocator->addr;
  for(uint64_t i = 0; i < size; i++) {
    addr[i] = i;
  }
}


void check_data(mem_allocator* allocator) {
  uint64_t size = allocator->size;
  char* addr = allocator->addr;
  for(uint64_t i = 0; i < size; i++) {
    assert(addr[i] == i);
  }
}   

int main() {
  mem_allocator* allocator = create_mem_allocator(FILENAME, MM_SIZE);
  write_data(allocator);
  sleep(10);
  check_data(allocator);
  destroy_mem_allocator(allocator);
  return 0;
}
