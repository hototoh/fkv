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

#define FREE_BIT(x) (x) & (~0U - 2)
#define USED_BIT(x) (x) | 1

static inline uint64_t
BLOCK_SIZE(segregated_fits_list* block)
{
  return FREE_BIT(*(uint64_t*)((char*) block - BOUNDARY_TAG_SIZE));
}

static inline void
segregated_fits_insert_block_head(segregated_fits_head* head,
                                  segregated_fits_list* block)
{
  uint32_t data_size = head->mem_size;
  uint32_t mem_size  = data_size + SEGREGATED_SIZE_SPACE;
  char* block_addr = (char*) block;
  *((uint64_t*)((char*)block_addr - 8)) = FREE_BIT(mem_size);
  *((uint64_t*)((char*)block_addr + data_size)) = FREE_BIT(mem_size);
  block->next = head->head.next;
  head->head.next = block;
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
  uint64_t mem_size;
  mem_size = BLOCK_SIZE(block);
  *((uint64_t*)((char*) block - BOUNDARY_TAG_SIZE)) 
      = *((uint64_t*)((char*) block - SEGREGATED_SIZE_SPACE + mem_size)) 
      = USED_BIT(mem_size);
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

static inline bool
merge_block_forward(segregated_fits_list** _block, uint64_t head_addr)
{
  segregated_fits_list* block = *_block;
  segregated_fits_list* previous;
  uint64_t block_size = BLOCK_SIZE(block);
  previous = (segregated_fits_list*)((char*)block + block_size);
  // check next blcok is over the end of partition
  if ((uint64_t) previous >= head_addr) return false;

  uint64_t previous_block_size = BLOCK_SIZE(previous);
  // if set the flag, the block is being used.
  if (previous_block_size & 1UL) return false;
  
  uint64_t previous_block_head = (uint64_t) (
      (char*) previous + previous_block_size - BOUNDARY_TAG_SIZE);
  // in this case, somthing is wrong. previous block is over partition boundary.
  if ((uint64_t)((char*)previous_block_head - 8) < head_addr) 
    assert("block is over the partition boundary" && false);
  
  // merge current block and next block.
  uint64_t new_block_size = block_size + previous_block_size;
  *(uint64_t*)((char*)block - SEGREGATED_SIZE_SPACE + block_size)
      = *(uint64_t*)((char*)previous - 8) 
      = FREE_BIT(new_block_size);

  *_block = previous;
  return true;

}

static inline bool
merge_block_backward(segregated_fits_list** _block, uint64_t end_addr)
{
  segregated_fits_list* block = *_block;
  segregated_fits_list* next;
  uint64_t block_size = BLOCK_SIZE(block);
  next = (segregated_fits_list*)((char*)block + block_size);
  // check next blcok is over the end of partition
  if ((uint64_t) next >= end_addr) return false;

  uint64_t next_block_size = BLOCK_SIZE(next);
  // if set the flag, the block is being used.
  if (next_block_size & 1UL) return false;
  

  uint64_t next_block_end = (uint64_t)(
      (char*) next + next_block_size - BOUNDARY_TAG_SIZE);
  // in this case, somthing is wrong. next block is over partition boundary.
  if (next_block_end > end_addr)
    assert("block is over the partition boundary" && false);
  
  // merge current block and next block.
  uint64_t new_block_size = block_size + next_block_size;
  *(uint64_t*)((char*)next - SEGREGATED_SIZE_SPACE + next_block_size)
      = *(uint64_t*)((char*)block - 8) 
      = FREE_BIT(new_block_size);
  //*_block = block;
  return true;
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
    head->head.next = &head->head;
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
  //D("index: %d, addr_ptr: %lx, size: %u, block_size: %u", 
  //class_index, addr, *size, mem_size);

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

static inline segregated_fits_list*
__get_segregated_fits_block(segregated_fits_head* head)
{
  // if (segregated_fits_is_empty(head)) return NULL;
  segregated_fits_list* block = head->head.next;  
  head->head.next = block->next;
  
  mask_allocated_flag(block);
  return block;
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
    // divide block into block_A and block_B
    block = __get_segregated_fits_block(head);
    block_A = block;
    block_B = (segregated_fits_list *)(
        (char*) block + SEGREGATED_SIZE_SPACE +
        (segregated_fits_class(class_index)));

    segregated_fits_head* head_A = &sfits->heads[class_index];
    segregated_fits_insert_block_head(head_A, block_A);
    
    while(merge_block_backward(&block_B, sfits->addr + sfits->addr_size));
    buddy_class_index = (BLOCK_SIZE(block_B) >> DIFF_MAGIC)
                        - SEGREGATED_SIZE_SPACE_BITS;
    if (buddy_class_index >= 0) {
      segregated_fits_head* head_B = &sfits->heads[buddy_class_index];
      segregated_fits_insert_block_head(head_B, block_B);
    } else if(buddy_class_index == -2) { // 0 <= size < 8
      mask_free_8_block((void*) block_B);
    } else if(buddy_class_index == -1) { // 8 <= size < 16
      mask_free_16_block((void*) block_B);
    } else {
      D("buddy_class_index : %d", buddy_class_index);
      assert(false);
    }
    
    // join the next block with block_B
    return true;
  }
  return false;
}

/* return the address of data portion */
void*
get_segregated_fits_block(segregated_fits* sfits, uint32_t data_size)
{
  int class_index = class_ceil_index(data_size);
  if (class_index > sfits->len) return NULL;

  segregated_fits_head* head = &sfits->heads[class_index];
  if (segregated_fits_is_empty(head)) {
    if(!segregated_fits_divide(sfits, class_index)) {
      return NULL;
    }
  }

  segregated_fits_list* block = __get_segregated_fits_block(head);
  assert(block != head->head.next);
  return (void*) block;
}

void
free_segregated_fits_block(segregated_fits* sfits, segregated_fits_list* block)
{
  uint64_t data_size;
  data_size = FREE_BIT(*((uint64_t*)((char*)block - BOUNDARY_TAG_SIZE)));
  // XXX
  // check the previous and next block to merge.
  while(merge_block_backward(&block, sfits->addr + sfits->addr_size));
  while(merge_block_forward(&block, sfits->addr));
  

  int class_index = class_ceil_index((uint32_t) data_size);
  if (class_index > sfits->len) return ;

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
  
  for(next = head->head.next; next != &head->head; next = next->next) {
    //uint64_t next_addr = (uint64_t) next, header_size, footer_size;
    uint64_t header_size, footer_size;
    char* next_addr = (char*) next;
    header_size = *(uint64_t*)(next_addr - 8);
    footer_size = *(uint64_t*)(next_addr + mem_size);
    printf("\t|---0x%lx\n", (uint64_t)next_addr - 8);
    
    assert(block_size == header_size);
    assert(block_size == footer_size);
  }
  D("fi.");
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
