#include "user_misprediction_test.h"

#include "../tester.h"

#include "../cache/cache_test.h"

#include "immintr.h"
#include "mem.h"
#include "rand.h"
#include "types.h"

AS_RESULT(user_misprediction_result_t);

// TODO: Find the correct value before doing this
#define CACHE_LINE_SZ 4096
#define CACHE_LINE_ALIGNED __attribute__((aligned(CACHE_LINE_SZ)))
#define CACHE_LINE_ALIGNED_PTR __attribute__((aligned(CACHE_LINE_SZ)))

void func(request_dependencies_t *args) {
  cache_result_t *cache_r = args[1];

  RESULT->overhead = cache_r->overhead;
  RESULT->uncached_access_time = cache_r->uncached_access_time;

  int *take_branch CACHE_LINE_ALIGNED = alloc(cache_r->tries * sizeof(u64));

  for (int i = 0; i < cache_r->tries; i++) {
    take_branch[i] = 1;
    if (i % 32 == 0) {
      u32 bit;
      u32 count = 0;
      do {
        bit = get_rand() % 2;
        count += 1;
      } while (bit == 1);

      take_branch[i] = 0;
    }

    RESULT->cache_line_access_count += !take_branch[i];
  }

  u64 sum = 0;
  volatile u8 *cache_line = (volatile u8 *)alloc(CACHE_LINE_SZ);

  for (int i = 0; i < cache_r->tries; i++) {
    mem_protect(cache_line, CACHE_LINE_SZ, MPROT_READ | MPROT_WRITE);
    cache_line_flush(cache_line);
    if (!take_branch[i]) {
      mem_protect(cache_line, CACHE_LINE_SZ, MPROT_NONE);
    }

    cache_line_flush(&take_branch[i]);
    serialise();
    memory_barrier();

    // Bad speculation
    if (take_branch[i]) {
#ifdef MITIGATE
      serialise();
#endif
      load(cache_line);
    }

    mem_protect(cache_line, CACHE_LINE_SZ, MPROT_READ | MPROT_WRITE);
    serialise();
    volatile u64 start = get_cycle();

    load((volatile void *)cache_line);

    serialise();
    memory_barrier();
    volatile u64 end = get_cycle();
    sum += (end - start) * !take_branch[i];
  }

  RESULT->cache_line_time_access_tot = sum;
}

#include "../tester.c"
