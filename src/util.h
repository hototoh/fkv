#ifndef UTIL_H
#define UTIL_H

#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

static inline void
init_rand()
{
  srand((unsigned)time(NULL));
}

static inline uint32_t
rand_fast_integer(uint8_t max_bit)
{
  max_bit = max_bit <= 32? max_bit : 32;
  uint32_t mask = (~0U) >> (32 - max_bit);
  return (uint32_t) rand() & mask;
}

static inline uint32_t
rand_integer(uint32_t max)
{
  return (uint32_t) rand() % max;
}

static inline char
rand_char()
{
  static char material[] = 
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  static uint32_t size = (uint32_t) sizeof(material);

  return material[rand_integer(size)];
}

static inline uint32_t
rand_string(char* ptr, uint32_t len)
{
  for (int i = 0; i < len; i++) {
    ptr[i] = rand_char();
  }
  return len;
}

static inline uint32_t
rand_string_with_max(char* ptr, uint32_t max)
{
  uint32_t len = rand_integer(max);
  for (int i = 0; i < len; i++) {
    ptr[i] = rand_char();
  }
  return len;
}

#endif
