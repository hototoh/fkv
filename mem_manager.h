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
  uint32_t bit = max_size >> DIFF_MAGIC;
  return (max_size & 0b111)  ? bit + 1 : bit;
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

int
segregated_fits_reclassing(segregated_fits* sfits, void** addr_ptr,
                           uint32_t* size);

void*
get_segregated_fits_block(segregated_fits* sfits, uint32_t data_len);

void
free_segregated_fits_block(segregated_fits* sfits, segregated_fits_list* block);

void
dump_segregated_fits(segregated_fits* sfits);

#endif
