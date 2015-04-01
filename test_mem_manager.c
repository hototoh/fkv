#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <strings.h>
#include <unistd.h>
#include <unistd.h>
#include <assert.h>

#include "common.h"
#include "shm.h"
#include "mem_manager.h"

#define MAX_DATA_SIZE 1152
#define TEST_FILE "test_mem_manager"
#define MEM_SIZE (1 << 15)

void segregated_fits_test()
{
  segregated_fits* sfits = create_segregated_fits(MAX_DATA_SIZE);
  assert(sfits != NULL);
  mem_allocator* allocator = create_mem_allocator(TEST_FILE, MEM_SIZE);
  sfits->addr = allocator->addr;
  sfits->addr_size = allocator->size;

  assert(allocator != NULL);
  uint32_t size = allocator->size;
  do {
    int res = segregated_fits_reclassing(sfits, &allocator->addr, &size);
    if (res > 0) break;
  } while(1);
  dump_segregated_fits(sfits);
 

#define GET_NUM 8
  void* addrs[GET_NUM];
  uint32_t sizes[GET_NUM];
  for (int i = 0; i < GET_NUM; i++) {    
    sizes[i] = 1 << i;
    addrs[i] = get_segregated_fits_block(sfits, sizes[i]);
    assert(addrs[i] != NULL);
    D("GET: size: %u, address: 0x%lx", sizes[i], addrs[i]);
  }
 
  dump_segregated_fits(sfits);
  for (int i = 0; i < GET_NUM; i++) {    
    free_segregated_fits_block(sfits, (segregated_fits*) addrs[i]);
  }
 
  sleep(1);
  dump_segregated_fits(sfits);
 
  destroy_segregated_fits(sfits);  
}

int main()
{
  assert(segregated_fits_class_size(4) == 1);
  assert(segregated_fits_class_size(8) == 1);
  assert(segregated_fits_class_size(10) == 2);
  assert(segregated_fits_class_size(13) == 2);
  assert(segregated_fits_class_size(16) == 2);
  assert(segregated_fits_class_size(24) == 3);
  assert(segregated_fits_class_size(32) == 4);
  assert(segregated_fits_class_size(40) == 5);
  assert(segregated_fits_class_size(48) == 6);
  assert(segregated_fits_class_size(56) == 7);
  assert(segregated_fits_class_size(60) == 8);
  assert(segregated_fits_class_size(64) == 8);
  assert(segregated_fits_class_size(128) == 16);
  assert(segregated_fits_class_size(256) == 32);
  assert(segregated_fits_class_size(512) == 64);
  assert(segregated_fits_class_size(1024) == 128);
  assert(segregated_fits_class_size(1536) == 192);
  assert(segregated_fits_class_size(1600) == 200);
  assert(segregated_fits_class_size(2048) == 256);

  assert(segregated_fits_class(0) == 8);
  assert(segregated_fits_class(1) == 16);
  assert(segregated_fits_class(2) == 24);
  assert(segregated_fits_class(3) == 32);
  assert(segregated_fits_class(4) == 40);
  assert(segregated_fits_class(5) == 48);
  assert(segregated_fits_class(6) == 56);
  assert(segregated_fits_class(7) == 64);
  assert(segregated_fits_class(8) == 72);
  assert(segregated_fits_class(9) == 80);
  assert(segregated_fits_class(10) == 88);
  assert(segregated_fits_class(11) == 96);

  assert(ffs(1U) == 1);
  assert(ffs(3U) == 1);
  assert(ffs(8U) == 4);
  assert(ffs(16U) == 5);
  assert(fls(1U) == 1);
  assert(fls(3U) == 2);
  assert(fls(8U) == 4);
  assert(fls(16U) == 5);

  segregated_fits_test();

  D("fin.");
  return 0;
}
