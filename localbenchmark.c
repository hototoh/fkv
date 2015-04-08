#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>
#include <sched.h>

#include "common.h"
#include "city.h"
#include "zipf.h"
#include "mem_manager.h"
#include "circular_log.h"

static volatile bool running;

typedef enum _benchmark_mode_t
{
  BENCHMARK_MODE_ADD = 0,
  BENCHMARK_MODE_SET,
  BENCHMARK_MODE_GET_HIT,
  BENCHMARK_MODE_GET_MISS,
  BENCHMARK_MODE_GET_SET_95,
  BENCHMARK_MODE_GET_SET_50,
  BENCHMARK_MODE_DELETE,
  BENCHMARK_MODE_SET_1,
  BENCHMARK_MODE_GET_1,
  BENCHMARK_MODE_MAX,
} benchmark_mode_t;

struct proc_arg
{
  size_t num_threads;
  uint8_t thread_id;
  pthread_t thread;

  size_t key_length;
  size_t value_length;

  size_t op_count;
  uint8_t *op_types;
  uint8_t *op_log_entry;

  size_t num_partitions;
  kv_table* table;
  circular_log* log_table;

  benchmark_mode_t benchmark_mode;

  uint64_t junk;
  uint64_t success_count;
};

static inline uint16_t
get_partition_id(uint64_t key_hash, uint16_t num_partitions)
{
  return (uint16_t)(key_hash >> 48) & (uint16_t)(num_partitions - 1);
}

static inline circular_log_entry*
circular_log_entry_at(uint8_t* entries, size_t key_length, size_t val_length,
           uint64_t index)
{
  circular_log_entry* entry;
  uint64_t entry_size = key_length + val_length + sizeof(circular_log_entry);
  entry = (circular_log_entry*) (entries + entry_size * index);
  return entry;
}

int
benchmark_proc(void* _args)
{
  struct proc_arg* args = (struct proc_arg*) _args;
  size_t num_threads = args->num_threads;
  uint8_t thread_id = args->thread_id;

  const size_t key_length = args->key_length;
  const size_t val_length = args->value_length;
  const int64_t op_count = (int64_t)args->op_count;
  const uint8_t *op_types = args->op_types;
  const uint8_t *op_log_entry = args->op_log_entry;
  const circular_log* log_table  = args->log_table;
  benchmark_mode_t benchmark_mode = args->benchmark_mode;

  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(thread_id + 1, &cpuset);
  sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
  
  /* wait until all threads get ready */
  while(!running) ;
  
  uint64_t junk = 0;
  uint64_t success_count = 0;
  uint64_t i;
  switch (benchmark_mode)
  {
    case BENCHMARK_MODE_ADD:
      for (i = 0; i < op_count; i++)
      {
#ifdef USE_PREFETCHING
#endif
        circular_log_entry* entry = circular_log_entry_at(op_log_entry, key_length,
                                                          val_length, i);
        if (i >= 0)
        {
          if (put_circular_log_entry(log_table, entry))
            success_count++;
        }
      }
      break;
    case BENCHMARK_MODE_SET:
    case BENCHMARK_MODE_GET_HIT:
    case BENCHMARK_MODE_GET_MISS:
    case BENCHMARK_MODE_GET_SET_95:
    case BENCHMARK_MODE_GET_SET_50:
    case BENCHMARK_MODE_SET_1:
    case BENCHMARK_MODE_GET_1:
      for (i = 0; i < op_count; i++)
      {
#ifdef USE_PREFETCHING
#endif
        circular_log_entry* entry = circular_log_entry_at(op_log_entry, key_length,
                                                          val_length, i);
        if (i >= 0) {
          bool is_get = op_types[i] == 0;
          if (is_get)
          {
            uint8_t value[val_length];
            size_t value_length = val_length;
            if(get_circular_log_entry(log_table, &entry))
               success_count++;
            junk += (uint64_t)value[0];
          } else {
            if (put_circular_log_entry(log_table, &entry))
              success_count++;
          }
        }
      }
      break;
    case BENCHMARK_MODE_DELETE:
      for (i = 0; i < op_count; i++)
      {
        circular_log_entry* entry = circular_log_entry_at(op_log_entry, key_length,
                                                          val_length, i);
#ifdef USE_PREFETCHING
#endif
        if (i >= 0)
        {
          if (remove_circular_log_entry(log_table, &entry))
            success_count++;
        }
      }
      break;
    default:
      assert(false);
  }
  
  args->junk = junk;
  args->success_count = success_count;
  return 0;
}

#define KEY_SIZE 8
#define VAL_SIZE 8
#define NUM_ITEM (1 << 15)
#define BUCKET_SIZE (1 << 17)
void benchmark(int core_num, float zipf_theta, float mth_threshold)
{
  printf("## Benchmark\n");
  printf("*  zipf_theta = %lf\n", zipf_theta);

  struct timeval tv_start;
  struct timeval tv_end;
  double diff;
  
  /**
   * @num_items: 
   * @num_partitions:
   * @num_threads:
   * @num_operations
   * @max_num_operations_per_thread
   */

  const size_t num_items = core_num * NUM_ITEM;
  const size_t num_partitions = core_num;

  const size_t num_threads = core_num;
  const size_t num_operations = core_num * NUM_ITEM;
  const size_t max_num_operations_per_thread = num_operations;

  const size_t key_length = KEY_SIZE;
  const size_t val_length = VAL_SIZE;

  double add_ops = -1.;
  double set_ops = -1.;
  double get_hit_ops = -1.;
  double get_miss_ops = -1.;
  double get_set_95_ops = -1.;
  double get_set_50_ops = -1.;
  double delete_ops = -1.;
  double set_1_ops = -1.;
  double get_1_ops = -1.;

  /**
   * allocating memory for # of the running threads.
   */
  printf("# Allocating memory\n");  
  char filename[PATH_MAX];
  sprintf(filename, "test_data_entries");
  uint64_t data_size = (sizeof(circular_log_entry) + KEY_SIZE + VAL_SIZE) * num_items * 2;
  mem_allocator* log_allocator = create_mem_allocator(filename, data_size);
  uint8_t* log_entries = log_allocator->addr;
  assert(log_entries);

  uint64_t *op_count = (uint64_t *)malloc(sizeof(uint64_t) * num_threads);
  assert(op_count);

  uint8_t **op_types = (uint8_t **)malloc(sizeof(uint8_t *) * num_threads);
  assert(op_types);

  uint8_t** op_log_entries = (uint8_t**) malloc(sizeof(uint8_t*) * num_threads);
  assert(op_log_entries);

  mem_allocator* op_log_allocators[num_threads];
  for (int i = 0; i < num_threads; i++) {
    char filename[PATH_MAX];
    sprintf(filename, "ops_data_entries_%d", core_num);
    uint64_t data_size = (sizeof(circular_log_entry) + KEY_SIZE + VAL_SIZE) * num_operations;
    
    mem_allocator* op_log_allocator = create_mem_allocator(filename, data_size);
    op_log_entries[i] = op_log_allocator->addr;
    assert(op_log_entries[i]);    

    op_types[i] = (uint8_t *)malloc(sizeof(uint8_t *) * max_num_operations_per_thread);
    assert(op_types[i]);

    op_log_allocators[i] = op_log_allocator;
  }

  //perf_count_t pc = benchmark_perf_count_init();
  kv_table* table = create_kv_table("localbenchmark", num_threads, BUCKET_SIZE,
                                    BUCKET_SIZE);
  if (table == NULL) {
    D("could not create kv_table");
    return EXIT_FAILURE;
  }

  printf("generating %zu items\n", num_items);
  for (uint64_t i = 0; i < num_items * 2; i++)
  {
    circular_log_entry* entry = circular_log_entry_at(log_entries, key_length, val_length, i);
    
    entry->key_length = key_length;
    entry->val_length = val_length;
    memcpy((void*)entry->data, &i, key_length);
    memcpy((void*)((char*)entry->data + key_length), &i, val_length);
    entry->keyhash = CityHash64(entry->data, key_length);
    entry->initial_size = sizeof(circular_log_entry) + key_length + val_length;
  }
  printf("\n");

  //#######################################################################################################
  benchmark_mode_t benchmark_mode;
  for (benchmark_mode = 0; benchmark_mode < BENCHMARK_MODE_MAX; benchmark_mode++) {
    uint32_t get_threshold = 0;
    if (benchmark_mode == BENCHMARK_MODE_ADD ||
        benchmark_mode == BENCHMARK_MODE_SET ||
        benchmark_mode == BENCHMARK_MODE_DELETE ||
        benchmark_mode == BENCHMARK_MODE_SET_1)
      get_threshold = (uint32_t)(0.0 * (double)((uint32_t)-1));
    else if (benchmark_mode == BENCHMARK_MODE_GET_HIT ||
             benchmark_mode == BENCHMARK_MODE_GET_MISS ||
             benchmark_mode == BENCHMARK_MODE_GET_1)
      get_threshold = (uint32_t)(1.0 * (double)((uint32_t)-1));
    else if (benchmark_mode == BENCHMARK_MODE_GET_SET_95)
      get_threshold = (uint32_t)(0.95 * (double)((uint32_t)-1));
    else if (benchmark_mode == BENCHMARK_MODE_GET_SET_50)
      get_threshold = (uint32_t)(0.5 * (double)((uint32_t)-1));
    else
      assert(false);

    printf("generating workload\n");
    uint64_t op_type_rand_state = 3;
    struct zipf_gen_state zipf_state;
    mehcached_zipf_init(&zipf_state, num_items, zipf_theta,
                        (uint64_t)benchmark_mode);

    for (benchmark_mode = 0;
         benchmark_mode < BENCHMARK_MODE_MAX;
         benchmark_mode++) {
      switch (benchmark_mode)
      {
        case BENCHMARK_MODE_ADD:
          printf("adding %zu items\n", num_items);
          break;
        case BENCHMARK_MODE_SET:
          printf("setting %zu items\n", num_items);
          break;
        case BENCHMARK_MODE_GET_HIT:
          printf("getting %zu items (hit)\n", num_items);
          break;
        case BENCHMARK_MODE_GET_MISS:
          printf("getting %zu items (miss)\n", num_items);
          break;
        case BENCHMARK_MODE_GET_SET_95:
          printf("getting/setting %zu items (95%% get)\n", num_items);
          break;
        case BENCHMARK_MODE_GET_SET_50:
          printf("getting/setting %zu items (50%% get)\n", num_items);
          break;
        case BENCHMARK_MODE_DELETE:
          printf("deleting %zu items\n", num_items);
          break;
        case BENCHMARK_MODE_SET_1:
          printf("setting 1 item\n");
          break;
        case BENCHMARK_MODE_GET_1:
          printf("getting 1 item\n");
          break;
        default:
          assert(false);
      }
    }

    memset(op_count, 0, sizeof(uint64_t)*num_threads);
    for(size_t i = 0; i < num_operations; i++) {
      size_t j; 
      if (benchmark_mode == BENCHMARK_MODE_ADD ||
          benchmark_mode == BENCHMARK_MODE_DELETE) {
        if (i >= num_items)
          break;
        j = i;
      } else if (benchmark_mode == BENCHMARK_MODE_GET_1 ||
                 benchmark_mode == BENCHMARK_MODE_SET_1) {
        j = 0;
      } else {
        j = mehcached_zipf_next(&zipf_state);
        if (benchmark_mode == BENCHMARK_MODE_GET_MISS)
          j += num_items;
      }

      circular_log_entry* src_entry = circular_log_entry_at(log_entries, key_length, val_length, j);
      uint16_t partition_id = get_partition_id(src_entry->keyhash, num_threads);
      uint32_t op_rand = mehcached_rand(&op_type_rand_state);
      bool is_get = op_rand <= get_threshold;
    
      size_t thread_id = partition_id;
      if (op_count[thread_id] < max_num_operations_per_thread) {
        op_types[thread_id][op_count[thread_id]] = is_get ? 0 : 1;
        uint8_t* dst_entry_base = op_log_entries[thread_id];
        circular_log_entry* dst_entry = circular_log_entry_at(dst_entry_base, key_length, val_length, op_count[thread_id]);
        memcpy(dst_entry, src_entry, src_entry->initial_size);
        op_count[thread_id]++;
      } else break;
    }
    printf("\n");


    struct proc_arg args[core_num];
    for (size_t thread_id = 0; thread_id < num_threads; thread_id++)
    {
      args[thread_id].num_threads    = num_threads;
      args[thread_id].thread_id      = thread_id;

      args[thread_id].key_length     = key_length;
      args[thread_id].value_length   = val_length;
      args[thread_id].op_log_entry   = op_log_entries[thread_id];

      args[thread_id].table          = table;
      args[thread_id].log_table      = table->log[thread_id];
      args[thread_id].benchmark_mode = benchmark_mode;
    }

    D("executing workload");     /* Start measuring */
    running = false;
    for(int thread_id = 0; thread_id < num_threads; thread_id++) {    
      pthread_create(&args[thread_id].thread, NULL, (void*) benchmark_proc, (void*) &args[thread_id]);    
    }

    gettimeofday(&tv_start, NULL);
    running = true;

    int finished_thread = 0;
    while (finished_thread < core_num) {
      for(int thread_id = 0; thread_id < core_num; thread_id++) {
        const struct timespec spec = {
          .tv_sec = 0,
          .tv_nsec = 5000000,
        };
      
        int res = pthread_timedjoin_np(args[thread_id].thread, NULL, &spec);
        if (res == 0) finished_thread++;
      }
    }

    gettimeofday(&tv_end, NULL);
    diff = (double)(tv_end.tv_sec - tv_start.tv_sec) * 1. 
           + (double)(tv_end.tv_usec - tv_start.tv_usec) * 0.000001;

    /* sum the results */
    size_t success_count = 0;
    size_t operations = 0;
    for (size_t thread_id = 0; thread_id < num_threads; thread_id++)
    {
      success_count += args[thread_id].success_count;
      operations += args[thread_id].op_count;
    }

    printf("operations: %zu\n", operations);
    printf("success_count: %zu\n", success_count);

    switch (benchmark_mode)
    {
      case BENCHMARK_MODE_ADD:
        add_ops = (double)operations / diff;
        //mem_diff = mehcached_get_memuse() - mem_start;
        break;
      case BENCHMARK_MODE_SET:
        set_ops = (double)operations / diff;
        break;
      case BENCHMARK_MODE_GET_HIT:
        get_hit_ops = (double)operations / diff;
        break;
      case BENCHMARK_MODE_GET_MISS:
        get_miss_ops = (double)operations / diff;
        break;
      case BENCHMARK_MODE_GET_SET_95:
        get_set_95_ops = (double)operations / diff;
        break;
      case BENCHMARK_MODE_GET_SET_50:
        get_set_50_ops = (double)operations / diff;
        break;
      case BENCHMARK_MODE_DELETE:
        delete_ops = (double)operations / diff;
        break;
      case BENCHMARK_MODE_SET_1:
        set_1_ops = (double)operations / diff;
        break;
      case BENCHMARK_MODE_GET_1:
        get_1_ops = (double)operations / diff;
        break;
      default:
        assert(false);
    }
    printf("\n");
  }

  //if (args[0].junk == 1)
  //  printf("junk: %zu (ignore this line)\n", args[0].junk);

  //benchmark_perf_count_free(pc);
  //printf("memory:     %10.2lf MB\n", (double)mem_diff * 0.000001);
  printf("add:        %10.2lf Mops\n", add_ops * 0.000001);
  printf("set:        %10.2lf Mops\n", set_ops * 0.000001);
  printf("get_hit:    %10.2lf Mops\n", get_hit_ops * 0.000001);
  printf("get_miss:   %10.2lf Mops\n", get_miss_ops * 0.000001);
  printf("get_set_95: %10.2lf Mops\n", get_set_95_ops * 0.000001);
  printf("get_set_50: %10.2lf Mops\n", get_set_50_ops * 0.000001);
  printf("delete:     %10.2lf Mops\n", delete_ops * 0.000001);
  printf("set_1:      %10.2lf Mops\n", set_1_ops * 0.000001);
  printf("get_1:      %10.2lf Mops\n", get_1_ops * 0.000001);

  destroy_kv_table(table);
}

int main(int argc, char* argv[])
{
  if (argc < 4)
  {
    printf("%s CORE_NUM ZIPF-THETA MTH-THRESHOLD\n", argv[0]);
    return EXIT_FAILURE;
  }

  benchmark(atoi(argv[1]), atof(argv[2]), atof(argv[3]));
  return EXIT_SUCCESS;
}
