#ifndef COMMON_H
#define COMMON_H

#include <sys/time.h>

#ifndef ND /* debug macros */
/* debug support */
#define ND(_fmt, ...) do {} while(0)
#define D(_fmt, ...)                        \
  do {                            \
  struct timeval _t0;             \
  gettimeofday(&_t0, NULL);           \
  fprintf(stdout, "%03d.%06d %s [%d] " _fmt "\n", \
    (int)(_t0.tv_sec % 1000), (int)_t0.tv_usec, \
          __FUNCTION__, __LINE__, ##__VA_ARGS__); \
  } while (0)

static inline void print_addr(void* addr, char* var) {
  printf("addr: 0b");
  for(int i = 0; i < 64; i++) {
    uint64_t _addr = (uint64_t) addr >> i;
    printf("%1d", (int) _addr & 1);
  }
  printf(" : %s\n", var);
}

#endif

#define FKV_ROUNDUP8(x) (((x) + 7UL) & (~7UL))
#define FKV_ROUNDUP64(x) (((x) + 63UL) & (~63UL))
#define FKV_ROUNDUP4K(x) (((x) + 4095UL) & (~4095UL))
#define FKV_ROUNDUP1M(x) (((x) + 1048575UL) & (~1048575UL))
#define FKV_ROUNDUP2M(x) (((x) + 2097151UL) & (~2097151UL))

#ifndef FLS_SUPPORT
#define fls generic_fls
#endif

bool DEBUG;

static inline int
generic_fls(uint32_t x)
{
  int r = 32;
  if (!x) return 0;
  
  if (!(x & 0xffff0000u)) {
    x <<= 16;
    r -= 16;
  }
  if (!(x & 0xff000000u)) {
    x <<= 8;
    r -= 8;
  }
  if (!(x & 0xf0000000u)) {
    x <<= 4;
    r -= 4;
  }
  if (!(x & 0xc0000000u)) {
    x <<= 2;
    r -= 2;
  }
  if (!(x & 0x80000000u)) {
    x <<= 1;
    r -= 1;
  }
  return r;
}

#endif
