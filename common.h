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
  fprintf(stderr, "%03d.%06d %s [%d] " _fmt "\n", \
    (int)(_t0.tv_sec % 1000), (int)_t0.tv_usec, \
          __FUNCTION__, __LINE__, ##__VA_ARGS__); \
  } while (0)

#endif

#endif
