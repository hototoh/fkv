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

//#define DEBUG

extern bool DEBUG;
static uint64_t len = 0;

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
mask_free_flag(segregated_fits_list* block)
{
  uint64_t mem_size;
  mem_size = BLOCK_SIZE(block);
  *((uint64_t*)((char*) block - BOUNDARY_TAG_SIZE)) 
      = *((uint64_t*)((char*) block - SEGREGATED_SIZE_SPACE + mem_size)) 
      = mem_size;
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
  //D("addr: 0x%lx block_size: %u, data size: %u", addr,
  //  head->mem_size+SEGREGATED_SIZE_SPACE, head->mem_size);
  if (segregated_fits_is_empty(head)) {
    assert("head must have the block with the addr" && false);
  }
  
  segregated_fits_list* next, *pre;
  for(pre = &head->head, next = head->head.next;
      next != &head->head;
      pre = next, next = next->next) {
    if (addr != (void*) next) continue;

    if(DEBUG) 
      printf("[%lu]HOge!?\n", len++);
    pre->next = next->next;
    if(DEBUG)
      printf("[%lu]HOge!!\n", len++);
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
merge_block_forward(segregated_fits* sfits, void** block_tail_ptr)
{
  uint32_t max_block_size = segregated_fits_class(sfits->len);
  uint64_t head_addr = sfits->addr;
  uint64_t block_size = FREE_BIT(*(uint64_t*)(*block_tail_ptr));
  void* previous_block_tail = (void*)((char*)(*block_tail_ptr) - block_size);

  if((uint64_t)previous_block_tail < head_addr) {
    //D("check next blcok is over the tail of partition");
    return false;
  }

  if((*(uint64_t*)previous_block_tail) & 1UL) {
    return false;
  }

  if((*(uint64_t*)previous_block_tail) == (max_block_size +
                                           SEGREGATED_SIZE_SPACE)) {    
    //D("previous block is max size empty");
    return false;
  }

  void* previous_head;
  previous_head = (void*)((char*)previous_block_tail 
                          - *(uint64_t*)previous_block_tail
                          + BOUNDARY_TAG_SIZE);
  if((uint64_t)previous_head < head_addr) {
    assert("check next blcok is over the tail of partition" && false);
    return false;
  }
  
  // remove the previous block from segregated fits list.
  int data_size = (int) (*(uint64_t*)previous_block_tail
                                   - SEGREGATED_SIZE_SPACE);
  int index;
  index = data_size > 0 ? class_ceil_index(data_size) : -1;
  index = index >= (int) sfits->len ? sfits->len - 1 : index;
  if (index >= 0) {
    segregated_fits_head* head = &sfits->heads[index];
    segregated_fits* list_ptr = (segregated_fits*)((char*)previous_head + BOUNDARY_TAG_SIZE);
    
    get_segregated_fits_block_with_addr(head, (void*) list_ptr);
  }

  uint64_t new_block_size = block_size + *(uint64_t*)previous_block_tail;
  *(uint64_t*)previous_head = *(uint64_t*)(*block_tail_ptr) = new_block_size;
  return true;
}

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
merge_block_backward(segregated_fits* sfits, void** block_head_ptr)
{
  uint32_t max_block_size = segregated_fits_class(sfits->len);
  uint64_t tail_addr = sfits->addr + (uint64_t) sfits->addr_size;
  uint64_t block_size = FREE_BIT(*(uint64_t*)(*block_head_ptr));
  void* next_block_head = (void*)((char*)(*block_head_ptr) + block_size);
  
  if(tail_addr < ((uint64_t)next_block_head + BOUNDARY_TAG_SIZE)) {
    if(DEBUG)
      D("check next blcok is over the end of partition");    
    return false;
  }
  
  if((*(uint64_t*)next_block_head) & 1UL) {
    if(DEBUG)
     D("next block is used");
    return false;
  }

  if((*(uint64_t*)next_block_head) == (max_block_size +
                                       SEGREGATED_SIZE_SPACE)) {
    if(DEBUG)
       D("next block is full size empty block");
    return false;    
  }

  void* next_tail = (void*) ((char*)next_block_head - BOUNDARY_TAG_SIZE
                             + *(uint64_t*)next_block_head);
  if(tail_addr < (uint64_t)next_tail+BOUNDARY_TAG_SIZE) {
    assert("check next blcok is over the end of partition" && false);
    return false;
  }

  // remove the next block from segregated fits list.
  int data_size = (int)(*(uint64_t*)next_block_head
                        - SEGREGATED_SIZE_SPACE);
  int index;
  index = data_size > 0 ? class_ceil_index(data_size) : -1;
  index = index >= (int) sfits->len ? sfits->len - 1 : index;
  if (index >= 0) {
    segregated_fits_head* head = &sfits->heads[index];
    void* list_ptr= (void*)((char*) next_block_head + BOUNDARY_TAG_SIZE);
    get_segregated_fits_block_with_addr(head, list_ptr);
  }

  uint64_t new_block_size = block_size + *(uint64_t*)next_block_head;
  *(uint64_t*)(*block_head_ptr) = *(uint64_t*) next_tail = new_block_size;
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
  class_index = class_index >= (int)(sfits->len) ?
                (int) sfits->len - 1 : class_index;  
  mem_size = ((uint32_t)class_index +SEGREGATED_SIZE_SPACE_BITS+ 1)
             << DIFF_MAGIC;

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

  assert((*((uint64_t*)(addr))) == 
         *((uint64_t*)(addr + mem_size - BOUNDARY_TAG_SIZE)));
  assert((*((uint64_t*)(addr))) == mem_size);

  *addr_ptr = (void*) (addr + mem_size);
  *size = *size - mem_size;
  return *size == 0 ? ENOMEM : 0;
}

static bool
__segregated_fits_reclassing(segregated_fits* sfits, 
                             void** block_head_ptr, uint32_t* size)
{
  int res = segregated_fits_reclassing(sfits, (void**) block_head_ptr, size);
  if( res < 0) return false;
  
  *(uint64_t*)*block_head_ptr
      = *(uint64_t*)((char*)*block_head_ptr + *size - BOUNDARY_TAG_SIZE)
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
    //D("index: %d", i);

    // divide block into block_A and block_B
    segregated_fits_list* block, *block_A;
    void* block_B_head;
    uint64_t block_size, block_A_size, block_B_size;
    block = __get_segregated_fits_block(head);
    block_size   = BLOCK_SIZE(block);
    block_A = block;
    block_A_size = (uint64_t)segregated_fits_class((uint32_t)class_index)
                   + SEGREGATED_SIZE_SPACE;
    segregated_fits_head* head_A = &sfits->heads[class_index];
    segregated_fits_insert_block_head(head_A, block_A);

    block_B_size = block_size - block_A_size;
    block_B_head = (void *)(
        (char*) block - BOUNDARY_TAG_SIZE + block_A_size);    
    // D(" %lu ==> %lu + %lu", block_size, block_A_size, block_B_size);

    // before call merge_block_backward, MUST set the size to the block.
    if (buddy_class_index >= 0) {
      *(uint64_t*)((char*) block_B_head + block_B_size - BOUNDARY_TAG_SIZE)
                   = *(uint64_t*) block_B_head
                   = block_B_size;
    } else if(buddy_class_index == -2) { // 0 <= size < 8
      mask_free_8_block((char*)block_B_head);
    } else if(buddy_class_index == -1) { // 8 <= size < 16 // not necessary?
      mask_free_16_block((char*)block_B_head);
    } else {
      D("buddy_class_index : %d", buddy_class_index);
      assert(false);
    }

    while(merge_block_backward(sfits, &block_B_head)) {
      uint32_t size = (uint32_t)(*(uint64_t*) block_B_head);
      uint32_t max_size = segregated_fits_class(sfits->len) + SEGREGATED_SIZE_SPACE;
      if(size < max_size) continue;

      if(!__segregated_fits_reclassing(sfits, (void**) &block_B_head, &size)) ;
      break;
    }
        
    buddy_class_index = (int)((*(uint64_t*)block_B_head) >> DIFF_MAGIC) - 3;
    if (buddy_class_index >= 0) {
      segregated_fits_head* head_B = &sfits->heads[buddy_class_index];
      segregated_fits_insert_block_head(head_B, (segregated_fits_list*)(
          (char*)block_B_head + BOUNDARY_TAG_SIZE));
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
  if(DEBUG)
    printf("[START]get_segregated_fits_block\n");
  segregated_fits_list* block = __get_segregated_fits_block(head);
  if(DEBUG)
    printf("[END]get_segregated_fits_block\n");
  assert(block != head->head.next);
  return (void*) block;
}

void
free_segregated_fits_block(segregated_fits* sfits, segregated_fits_list* block)
{
  // merge previous and next blocks
  mask_free_flag(block);
  uint32_t max_size = segregated_fits_class(sfits->len) + SEGREGATED_SIZE_SPACE;
  uint32_t size;
  void* block_head,* block_tail = (void*) ((char*) block + BLOCK_SIZE(block)
                                            - SEGREGATED_SIZE_SPACE);
  while(merge_block_forward(sfits, &block_tail)) {
    size = (uint32_t)(*(uint64_t*) block_tail);
    if(max_size <= size) {
      block_head = (void*)((char*) block_tail - size + BOUNDARY_TAG_SIZE); 
      __segregated_fits_reclassing(sfits, (void**)&block_head, &size);
      break;
    }
  }
  
  size = (uint32_t)(*(uint64_t*) block_tail);
  block_head = (void*)((char*) block_tail - size + BOUNDARY_TAG_SIZE); 

  while(merge_block_backward(sfits, &block_head)) {
    size = (uint32_t)(*(uint64_t*) block_head);
    if(max_size <= size) {
      if(!__segregated_fits_reclassing(sfits, (void**) &block_head, &size))
	;
      break;
    }
  }

  uint64_t data_size = *(uint64_t*) block_head;
  int class_index = (data_size >> DIFF_MAGIC) - 3;
  if (class_index < 0) return ;
  if (class_index > sfits->len) return ;

  segregated_fits_head* head = &sfits->heads[class_index];
  block = (segregated_fits_list*) ((char*) block_head + BOUNDARY_TAG_SIZE);
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
