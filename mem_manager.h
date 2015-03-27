#ifndef MEM_MANAGER_H
#define MEM_MANAGER_H

#define BOUNDARY_TAG_SIZE 8
#define SEGREGATED_SIZE_SPACE 16
#define SEGREGATED_SIZE_SPACE_BITS 2

typedef struct segregated_fits_list segregated_fits_list;
struct segregated_fits_list {
  segregated_fits_list* next;
};

/* if empty node, head point self */
typedef struct segregated_fits_head { 
  segregated_fits_list* head;
  uint32_t mem_size;
  uint32_t version;
} segregated_fits_head;


static inline void
segregated_fits_insert_block_head(segregated_fits_head* head,
                                  segregated_fits_list* block)
{
  uint32_t data_size = head->mem_size;
  uint32_t mem_size  = data_size + SEGREGATED_SIZE_SPACE;
  *((uint64_t*)(block - 8)) = mem_size & (~0UL - 1);
  *((uint64_t*)(block + data_size)) = mem_size & (~0UL - 1);

  block->next = head->head->next;
  head->head->next = block;
}

static inline bool
segregated_fits_is_empty(segregated_fits_head* head)
{
  return head->head->next == head->head;
}
                                 
typedef struct segregated_fits {
  uint32_t len;
  segregated_fits_head heads[0];
} segregated_fits;

/* max_size must be multiples of 8 */
// 8 = 1 << 3
#define DIFF_MAGIC 3

static inline uint32_t
segregated_fits_class_size(uint32_t max_size)
{
  // multiple size classes incrementing by 8 bytes  
  // min class size is 8
  return max_size >> DIFF_MAGIC;
}

static inline uint32_t
segregated_fits_class(uint32_t _index)
{
  return (_index + 1) << DIFF_MAGIC;
}

static inline void
destroy_segregated_fits(segregated_fits* sfits)
{
  free(sfits);
}

segregated_fits*
create_segregated_fits(uint32_t max_data_size);

void*
get_segregated_fits_block(segregated_fits* sfits, uint32_t data_len);

void
free_segregated_fits_block(segregated_fits* sfits, segregated_fits_list* block);

#endif
