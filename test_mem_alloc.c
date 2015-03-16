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
  uint8_t* addr = (uint8_t*) allocator->addr;
  for(uint64_t i = 0; i < size; i++) {
    //D("%d = %d", *(addr+i), (int) (i & 0xff));
    *(addr+i) = (i & 0xff);
  }
}


void check_data(mem_allocator* allocator) {
  uint64_t size = allocator->size;
  uint8_t* addr = (uint8_t*) allocator->addr;
  for(uint64_t i = 0; i < size; i++) {
    //D("%d == %d !?", *(addr+i), (int) (i & 0xff));
    assert(*(addr+i) == (i & 0xff));
  }
}   

int main() {
  mem_allocator* allocator = create_mem_allocator(FILENAME, MM_SIZE);
  if (allocator == NULL) {
    D("fail to create memory allocator.");
    exit(1);
  }
  write_data(allocator);
  check_data(allocator);
  destroy_mem_allocator(allocator);
  return 0;
}
