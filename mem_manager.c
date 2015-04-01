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

#define FREE_BIT(x) (x) & (~0U - 1)
#define USED_BIT(x) (x) | 1

static inline uint32_t
BLOCK_SIZE(segregated_fits_list* block)
{
  return FREE_BIT(*(uint64_t*)((char*) block - BOUNDARY_TAG_SIZE));
}

static inline bool
BLOCK_USED(segregated_fits_list* block)
{
  return (*(uint64_t*)((char*) block - BOUNDARY_TAG_SIZE)) & 1UL;
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

static void
get_segregated_fits_block_with_addr(segregated_fits_head* head, void* addr)
{
  D("Addr: 0x%lx block_size: %u, data size: %u", addr,
    head->mem_size+SEGREGATED_SIZE_SPACE, head->mem_size);
  if (segregated_fits_is_empty(head)) {
    assert("head must have the block with the addr" && false);
  }

  segregated_fits_list* next, *pre;
  for(pre = &head->head, next = head->head.next;
      next != &head->head;
      pre = next, next = next->next) {
    if (addr != (void*) next) continue;

    pre->next = next->next;
    return ;
  }

  assert("this imply that not all empty spaces are managed by head" && false);
}

/**
 *                        +----------------------+
 *                        |    size        |flag |
 *                        +----------------------+
 *                        | segregated_fits_list |
 *                        +----------------------+
 *                        |                      |
 *                        |                      |
 *  second argument is -> +----------------------+
 *                        |    size        |flag |
 *                        +----------------------+
 */
static inline bool
merge_block_forward(segregated_fits* sfits, void** block_end_ptr)
{
  uint64_t head_addr = sfits->addr;
  uint64_t block_size = FREE_BIT(*(uint_64*)(*block_end_ptr));
  void* previous_block_end = (void*)((char*)(*block_end_ptr) - block_size);
  if(previous_block_end < head_addr) {
    D("check next blcok is over the end of partition");
    return false;
  }
    
  if(*(uint64_t*)previous_block_end & 1UL) {
    D("previous block used");
    return false;
  }

  void* previous_head;
  previous_head = (char*)(*block_end_ptr) - *(uint64_t*)previous_block_end;
  if(previoud_head < head_addr) {
    assert("check next blcok is over the end of partition" && false);
    return false;
  }
  
  // remove previous block from segregated fits list.
  int index = (int) ((*(uint64_t*)previous_head) >> DIFF_MAGIC) - 3;
  if (index >= 0) {
    segregated_fits_head* head = &sfits->heads[i];
    segregated_fits* list_ptr = (char*) previous_head + BOUNDARY_TAG_SIZE;
    get_segregated_fits_block_with_addr(head, (void*) list_ptr);
  }

  uint64_t new_block_size = block_size + *(uint64_t*)previous_block_end;
  *(uint64_t*)previous_head = *(uint64_t*)(*block_end_ptr) = new_block_size;
  *block_end_ptr = (char*) previou_head + new_block_size - BOUNDARY_TAG_SIZE;
  
  return true;
}
/*
static inline bool
merge_block_forward(segregated_fits* sfits, segregated_fits_list** _block)
{
  uint64_t head_addr = sfits->addr;
  segregated_fits_list* block = *_block;
  segregated_fits_list* previous;
  uint32_t block_size = BLOCK_SIZE(block);
  uint32_t max_size = segregated_fits_class(sfits->len) + SEGREGATED_SIZE_SPACE;
  uint64_t* previous_block_size_ptr;
  previous_block_size_ptr = (uint64_t*)((char*)block - SEGREGATED_SIZE_SPACE);

  // check next blcok is over the end of partition
  if (head_addr > (uint64_t) previous_block_size_ptr) {
    //D("check next blcok is over the end of partition");
    return false;
  }

  uint32_t previous_block_size = FREE_BIT(*previous_block_size_ptr);
  if (previous_block_size == max_size) {
    D("previous max_size\n");
    return false;
  }

  // in this case, somthing is wrong. previous block is over partition boundary.
  previous = (segregated_fits_list*)((char*)block - previous_block_size);
  if (head_addr > (uint64_t) (char*)previous - BOUNDARY_TAG_SIZE) {
    assert("block is over the partition boundary" && false);
    return false;
  }

  // if set the flag, the block is being used.
  if (BLOCK_USED(previous)) {
    D("the block is used");
    return false;
  }
  
  D("current  block addr: 0x%lx, size: %u", (char*)block - 8, BLOCK_SIZE(block));
  D("previous block addr: 0x%lx, size: %u", (char*)previous - 8, BLOCK_SIZE(previous));
  // remove previous block from old head
  int previous_block_index = (int)(previous_block_size >> DIFF_MAGIC) -3;
  if (previous_block_index >= 0) {
    //D("remove previous block from old head");
    segregated_fits_head* head = &sfits->heads[previous_block_index];
    get_segregated_fits_block_with_addr(head, (void*) previous);
  }

  // merge current block and next block.
  uint64_t new_block_size = block_size + previous_block_size;
  *(uint64_t*)((char*)block - SEGREGATED_SIZE_SPACE + block_size)
      = *(uint64_t*)((char*)previous - 8) 
      = FREE_BIT(new_block_size);

  *_block = previous;
  D("addr: 0x%lx, block size: %u", *_block, BLOCK_SIZE(*_block));

  return true;
}
*/
/**
 *  second argument is -> +----------------------+
 *                        |    size        |flag |
 *                        +----------------------+
 *                        | segregated_fits_list |
 *                        +----------------------+
 *                        |                      |
 *                        |                      |
 *                        +----------------------+
 *                        |    size        |flag |
 *                        +----------------------+
 */

static inline bool
merge_block_backward(segregated_fits* sfits, segregated_fits_list** _block)
{
  uint32_t max_size = segregated_fits_class(sfits->len) + SEGREGATED_SIZE_SPACE;
  uint64_t end_addr = sfits->addr + sfits->addr_size;
  segregated_fits_list* block = *_block;
  segregated_fits_list* next;
  uint64_t block_size = BLOCK_SIZE(block);
  next = (segregated_fits_list*)((char*)block + block_size);

  // check next blcok is over the end of partition
  if ((uint64_t) next >= end_addr) {
    D("next(%u) < partition end(%u)", next, end_addr);
    return false;
  }

  // if set the flag, the block is being used.
  if (BLOCK_USED(next)){
    D("the block is used");
    return false;
  }

  uint32_t next_block_size = BLOCK_SIZE(next);
  uint64_t next_block_end = (uint64_t)(
      (char*) next + next_block_size - BOUNDARY_TAG_SIZE);
  if (next_block_size == max_size) {
    D("next max_size\n");
    return false;
  }
  // in this case, somthing is wrong. next block is over partition boundary.
  if (next_block_end > end_addr) {
    D("bad bad bad. next block is over partition boundary");
    assert("block is over the partition boundary" && false);
  }
  
  D("current block addr: 0x%lx, size: %u", (char*)block - 8, BLOCK_SIZE(block));
  D("   next block addr: 0x%lx, size: %u", (char*)next  - 8, BLOCK_SIZE(next));
  // remove next block from old head  
  int next_block_index = (int)(next_block_size >> DIFF_MAGIC) - 3;
  if (next_block_index >= 0) {
    D("remove next block from old head");
    segregated_fits_head* head = &sfits->heads[next_block_index];
    get_segregated_fits_block_with_addr(head, (void*)next);
  }

  // merge current block and next block.
  uint64_t new_block_size = block_size + next_block_size;
  *(uint64_t*)((char*)next - SEGREGATED_SIZE_SPACE + next_block_size)
      = *(uint64_t*)((char*)block - 8) 
      = FREE_BIT(new_block_size);

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
    //D("index: %u, mem_size: %u, addr: %lx",
    // i, head->mem_size, head);
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
  
  assert((*((uint64_t*)(data_addr - BOUNDARY_TAG_SIZE))) == 
         *((uint64_t*)(data_addr + mem_size - SEGREGATED_SIZE_SPACE)));
  assert((*((uint64_t*)(data_addr - BOUNDARY_TAG_SIZE))) == mem_size);
  D("class index:%d, ", class_index);
  *addr_ptr = (void*) (addr + mem_size);
  *size = *size - mem_size;
  return *size == 0 ? ENOMEM : 0;
}

static bool
__segregated_fits_reclassing(segregated_fits* sfits, 
                             segregated_fits** block_ptr, uint32_t* size)
{
  void* addr = (void*)((char*)*block_ptr - BOUNDARY_TAG_SIZE);
  int res = segregated_fits_reclassing(sfits, &addr, &size);
  if( res < 0) return false;

  *block_ptr = (segregated_fits*)((char*) addr + BOUNDARY_TAG_SIZE);
  *(uint64_t*)((char*)addr + *size - BOUNDARY_TAG_SIZE)
      = *(uint64_t*)((char*)addr)
      = *size;
  return true;
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
    
    // divide block into block_A and block_B
    segregated_fits_list* block, *block_A, *block_B;
    uint64_t block_size, block_A_size, block_B_size;
    block = __get_segregated_fits_block(head);
    block_size   = BLOCK_SIZE(block);
    block_A = block;
    block_A_size = (uint64_t)segregated_fits_class((uint32_t)class_index)
                   + SEGREGATED_SIZE_SPACE;
    block_B_size = (uint64_t)(i - class_index) << DIFF_MAGIC;
    block_B = (segregated_fits_list *)(
        (char*) block + SEGREGATED_SIZE_SPACE +
        (segregated_fits_class((uint32_t) class_index)));

    D("%lu => %lu + %lu", block_size, block_A_size, block_B_size);
    segregated_fits_head* head_A = &sfits->heads[class_index];
    segregated_fits_insert_block_head(head_A, block_A);
    
    // before call merge_block_backward, MUST set the size to the block.
    if (buddy_class_index >= 0) {
      *(uint64_t*)((char*) block_B + block_B_size - SEGREGATED_SIZE_SPACE)
                   = *(uint64_t*)((char*) block_B - BOUNDARY_TAG_SIZE)
                   = block_B_size;
    } else if(buddy_class_index == -2) { // 0 <= size < 8
      //D("8 block");
      mask_free_8_block((char*)block_B - 8);
    } else if(buddy_class_index == -1) { // 8 <= size < 16 // not necessary?
      //D("16 block");
      mask_free_16_block((char*)block_B - 8);
    } else {
      D("buddy_class_index : %d", buddy_class_index);
      assert(false);
    }

    while(merge_block_backward(sfits, &block_B));

    D("divide %u-size block into %u-size and %u-size",
      block_size, block_A_size, BLOCK_SIZE(block_B));
        
    buddy_class_index = (BLOCK_SIZE(block_B) >> DIFF_MAGIC) - 3;
    if (buddy_class_index >= 0) {
      segregated_fits_head* head_B = &sfits->heads[buddy_class_index];
      segregated_fits_insert_block_head(head_B, block_B);
    }    
       
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
  D("blocksize: %u", BLOCK_SIZE(block));

  return (void*) block;
}

void
free_segregated_fits_block(segregated_fits* sfits, segregated_fits_list* block)
{
  printf("\n");
  // merge previous and next blocks
  uint32_t max_size = segregated_fits_class(sfits->len) + SEGREGATED_SIZE_SPACE;
  uint32_t size;
  while(merge_block_forward(sfits, &block)) {
    if(max_size <= BLOCK_SIZE(block)) {
      __segregated_fits_reclassing(sfits, &block, &size);
      break;
    }
  }
  D("block addr: 0x%lx, size: %u", block, BLOCK_SIZE(block));
  while(merge_block_backward(sfits, &block)) {
    D("block addr: 0x%lx, size: %u", block, BLOCK_SIZE(block));
    if(max_size <= BLOCK_SIZE(block)) {
      if(!__segregated_fits_reclassing(sfits, &block, &size))
        break;
    }
  }
  D("block addr: 0x%lx, size: %u", block, BLOCK_SIZE(block));

  uint64_t data_size = BLOCK_SIZE(block);
  int class_index = (data_size >> DIFF_MAGIC) - 3;
  if (class_index < 0) return ;
  if (class_index > sfits->len) {
    D("THIS IS THE FACTOR OF BUGS?!");
    return ;
  }

  D("FREE block: %lu, index: %d", data_size, class_index);

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
         "HeadAddress: 0x%lx\n"
         "\t|\n",
         mem_size, block_size, (uint64_t) &head->head);
  
  for(next = head->head.next; next != &head->head; next = next->next) {
    uint64_t header_size, footer_size;
    char* next_addr = (char*) next;
    header_size = *(uint64_t*)(next_addr - 8);
    footer_size = *(uint64_t*)(next_addr + mem_size);
    printf("\t|---0x%lx\n", (uint64_t)next_addr - 8);
    
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
