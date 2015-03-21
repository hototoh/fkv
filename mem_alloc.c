#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h> // getpagesize
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "common.h"
#include "mem_alloc.h"

//#define FILE_NAME "/mnt/hugetlbfs/huge/hugepagefile"
//#define MM_SIZE (256UL*1024*1024)
#define MM_PROTECTION (PROT_READ | PROT_WRITE)
#define MM_FLAGS (MAP_SHARED)

mem_allocator*
create_mem_allocator_with_addr(char* filename, uint64_t _mem_size, void* _addr)
{
  int fd;
  fd = open(filename, O_CREAT | O_RDWR, 0755);
  if (fd < 0) {
    D("Fail to open %s", filename);
    goto error0;
  }

  // XXX mem_size must be multiples of 2MiB
  uint64_t mem_size = _mem_size;  
  void* addr = mmap(_addr, mem_size, MM_PROTECTION, MM_FLAGS, fd, 0);
  if(addr == MAP_FAILED) {
    D("Fail to mmap file.");
    goto error1;
  }
  print_addr(addr, "mmapped address");
  
  mem_allocator* allocator;
  allocator = (mem_allocator*) malloc(sizeof(mem_allocator));
  if (allocator == NULL) {
    D("Fail to open %s", filename);
    goto error2;
  }

  allocator->fd = fd;
  allocator->size = mem_size;
  allocator->addr = addr;
  strcpy(allocator->filename, filename);
  return allocator;
error2:
  munmap(addr, mem_size);
error1:
  close(fd);
  unlink(filename);
error0:
  return NULL;
}

void
destroy_mem_allocator(mem_allocator* allocator) {
  munmap(allocator->addr, allocator->size);
  close(allocator->fd);
  unlink(allocator->filename);
}
