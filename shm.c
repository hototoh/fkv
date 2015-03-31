#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/mman.h>
#include <linux/limits.h> // PATH_MAX

#include "common.h"
#include "shm.h"

#define MM_PROTECTION (PROT_READ | PROT_WRITE)
#define MM_FLAGS (MAP_SHARED)

static void hugepage_path(char* path, char* file)
{
  snprintf(path, PATH_MAX, "%s/%sfile", HUGEPAGE_PREFIX, file);
}

mem_allocator*
create_mem_allocator_with_addr(char* filename, uint64_t _mem_size, void* _addr)
{
  char path[PATH_MAX];
  hugepage_path(path, filename);

  int fd;
  fd = open(path, O_CREAT | O_RDWR, 0755);
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
  //print_addr(addr, "mmapped address");
  D("mmapped address: %lx", addr);
  
  mem_allocator* allocator;
  allocator = (mem_allocator*) malloc(sizeof(mem_allocator));
  if (allocator == NULL) {
    D("Fail to open %s", filename);
    goto error2;
  }

  allocator->fd = fd;
  allocator->size = mem_size;
  allocator->addr = addr;
  strncpy(allocator->filename, path, PATH_MAX);
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
