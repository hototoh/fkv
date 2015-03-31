#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>

#include "common.h"
#include "shm.h"
#include "mem_manager.h"

static inline void
segregated_fits_insert_block_head(segregated_fits_head* head,
                                  segregated_fits_list* block)
{
  uint32_t data_size = head->mem_size;
  uint32_t mem_size  = data_size + SEGREGATED_SIZE_SPACE;
  uint64_t block_addr = (uint64_t) block;
  *((uint64_t*)(block_addr - 8)) = mem_size & (~0U - 2);
  *((uint64_t*)(block_addr + data_size)) = mem_size & (~0U - 2); 
  block->next = head->head;
  head->head = block;
}

static inline int
class_ceil_index(uint32_t size)
{
  int class_index = ((int) size >> DIFF_MAGIC) - 1;
  return (size & 0b111)  ? class_index + 1
      :  class_index < 0 ? 0 : class_index;
}

static inline void
mask_allocated_flag(segregated_fits_list* block)
{
  uint64_t mem_size = (uint64_t) block - BOUNDARY_TAG_SIZE;
  *((uint64_t*)((uint64_t) block - BOUNDARY_TAG_SIZE)) 
      = *((uint64_t*)((uint64_t) block - SEGREGATED_SIZE_SPACE + mem_size)) 
      = mem_size | 1;
}

static inline void
mask_free_8_block(void* block)
{
  uint64_t* size_addr = (uint64_t*) block;
  *size_addr = 8;
}

static inline void
mask_free_16_block(void* block)
{
  uint64_t* size_addr = (uint64_t*) block;  
  *size_addr = 16;  
  *(size_addr+1) = 16;
}

segregated_fits*
create_segregated_fits(uint32_t max_data_size)
{
  uint32_t class_size;
  class_size = segregated_fits_class_size(max_data_size);
   
  segregated_fits* sfits;
  sfits = (segregated_fits*) malloc(sizeof(segregated_fits) + 
                                    sizeof(segregated_fits_head) * class_size);
  if (sfits == NULL) {
    D("Fail to allocate segreagated_fits");
    return NULL;
  }
  
  sfits->len = class_size;
  for(uint32_t i = 0; i < class_size; i++) {
    segregated_fits_head* head = &sfits->heads[i];
    head->mem_size = segregated_fits_class(i);
    head->version  = 0;
    head->head = &head->head;
    D("index: %u, mem_size: %u, addr: %lx",
      i, head->mem_size, head);
  }

  return sfits;
}

int
segregated_fits_reclassing(segregated_fits* sfits, void** addr_ptr,
                           uint32_t* size)
{
  uint32_t mem_size;  
  char* addr = (char*) *addr_ptr;
  int class_index = (int)(*size >> DIFF_MAGIC) - SEGREGATED_SIZE_SPACE_BITS - 1;
  class_index = class_index > (int)(sfits->len) ?
                (int) sfits->len - 1 : class_index;  
  mem_size = ((uint32_t)class_index +SEGREGATED_SIZE_SPACE_BITS+ 1)
             << DIFF_MAGIC;
  D("index: %d, addr_ptr: %lx, size: %u, block_size: %u", 
      class_index, addr, *size, mem_size);

  if (class_index < 0) { // size < 24
    assert(class_index >= -3);
    if(class_index == -2) { // 8 <= size < 16
      mask_free_8_block(*addr_ptr);
    } else if(class_index == -1) { // 16 <= size < 24
      mask_free_16_block(*addr_ptr);
    }
    *addr_ptr = (void*) (addr + mem_size);
    *size = *size - mem_size;
    return ENOMEM;
  }
  /**
   * mem_size includes header and footer (suffix and prefix size area).
   * [header_addr(8bytes)]
   * [....... list ......]
   * [......        .....]
   * [footer_addr(8bytes)]
   */
  segregated_fits_head *head = &sfits->heads[class_index];
  char* data_addr = addr + BOUNDARY_TAG_SIZE;
  segregated_fits_insert_block_head(head, (segregated_fits_list*) data_addr);
  assert((*((uint64_t*)(data_addr - 8))) == 
         *((uint64_t*)(data_addr + mem_size - 16)));
  assert((*((uint64_t*)(data_addr - 8))) == mem_size);
  *addr_ptr = (void*) (addr + mem_size);
  *size = *size - mem_size;
  return *size == 0 ? ENOMEM : 0;
}

static inline bool
segregated_fits_divide(segregated_fits* sfits, int class_index)
{
  uint32_t len = sfits->len;
  /* we need header and footer size in addition to data */
  for (int i = class_index+1; i < len; i++) {
    segregated_fits_head* head = &sfits->heads[i];    
    if(segregated_fits_is_empty(head)) continue;    

    int buddy_class_index = (i - class_index) - 3;
    assert(buddy_class_index >= -2);
    
    segregated_fits_list* block, *block_A, *block_B;
    block = get_segregated_fits_block(sfits, (uint32_t) i << DIFF_MAGIC);
    block_A = block;
    block_B = (segregated_fits_list *)(
        (uint64_t) block + SEGREGATED_SIZE_SPACE_BITS
        +(segregated_fits_class(class_index)));

    // divide block into block_A and block_B
    segregated_fits_head* head_A = &sfits->heads[class_index];
    segregated_fits_insert_block_head(head_A, block_A);
    if (buddy_class_index >= 0) {
      segregated_fits_head* head_B = &sfits->heads[buddy_class_index];
      segregated_fits_insert_block_head(head_B, block_B);
    } else if(class_index == -2) { // 8 <= size < 16
      mask_free_8_block((void*) block_B);
    } else if(class_index == -1) { // 16 <= size < 24
      mask_free_16_block((void*) block_B);
    } else {
      assert(false);
    }
    return true;
  }
  return false;
}

static inline segregated_fits_list*
__get_segregated_fits_block(segregated_fits_head* head)
{
  // if (segregated_fits_is_empty(head)) return NULL;
  segregated_fits_list* block = head->head->next;  
  head->head->next = block->next;
  mask_allocated_flag(block);
  return block;
}


/* return the address of data portion */
void*
get_segregated_fits_block(segregated_fits* sfits, uint32_t data_size)
{
  int class_index = class_ceil_index(data_size);
  if (class_index > sfits->len) return NULL;

  segregated_fits_head* head = &sfits->heads[class_index];
  if (!segregated_fits_is_empty(head))
    if(!segregated_fits_divide(sfits, class_index))
      return NULL;

  segregated_fits_list* block = __get_segregated_fits_block(head);
  assert(block != head->head);
  return (void*) block;
}

void
free_segregated_fits_block(segregated_fits* sfits, segregated_fits_list* block)
{
  uint64_t data_size = *((uint64_t*)((uint64_t)block - BOUNDARY_TAG_SIZE));
  // XXX
  // *********************
  // check the previous and next block to merge.

  int class_index = class_ceil_index((uint32_t) data_size);
  if (class_index > sfits->len) return NULL;

  segregated_fits_head* head = &sfits->heads[class_index];
  segregated_fits_insert_block_head(head, block);
}

static inline void
dump_segregated_fits_block(segregated_fits_head* head)
{
  segregated_fits_list *next;
  if(segregated_fits_is_empty(head)) return;

  uint32_t mem_size, block_size;
  mem_size = head->mem_size;
  block_size = head->mem_size + SEGREGATED_SIZE_SPACE;
  printf("MemoryBlockSize: %u, AllocatedSize: %u,"
         "HeadAddress: %lx\n"
         "\t|\n",
         mem_size, block_size, (uint64_t) &head->head);
  for(next = head->head; next != &head->head; next = next->next) {
    uint64_t next_addr = (uint64_t) next, header_size, footer_size;
    header_size = *(uint64_t*)(next_addr - 8);
    footer_size = *(uint64_t*)(next_addr + mem_size);
    printf("\t|---%lx\n", next_addr - 8);
    
    assert(block_size == header_size);
    assert(block_size == footer_size);
  }
}

void
dump_segregated_fits(segregated_fits* sfits)
{
  uint32_t len = sfits->len;
  for (uint32_t i = 0; i < len; i++) {
    segregated_fits_head* head = &sfits->heads[i];
    dump_segregated_fits_block(head);
  }
}
