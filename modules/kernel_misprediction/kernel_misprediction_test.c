#include "kernel_misprediction_test.h"

#include "../tester.h"

#include "../cache/cache_test.h"

#include "immintr.h"
#include "mem.h"
#include "rand.h"
#include "types.h"

AS_RESULT(kernel_misprediction_result_t);

#define CACHE_LINE_SZ 4096
#define CACHE_LINE_ALIGNED __attribute__((aligned(CACHE_LINE_SZ)))
#define CACHE_LINE_ALIGNED_PTR __attribute__((aligned(CACHE_LINE_SZ)))

void func(request_dependencies_t *args) {
  cache_result_t *cache_r = args[1];

  usize tries = cache_r->tries / 10;

  RESULT->overhead = cache_r->overhead;
  RESULT->uncached_access_time = cache_r->uncached_access_time;

  int *take_branch CACHE_LINE_ALIGNED = alloc(tries * sizeof(u64));

  for (int i = 0; i < tries; i++) {
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
  ker_open();
  volatile usize CACHE_LINE_ALIGNED *kernel_cache_line = get_kernel_ptr();
  volatile usize *user_cache_line =
      (volatile usize *)alloc(CACHE_LINE_SZ * sizeof(usize));

  volatile usize *ptr = user_cache_line;

  volatile u8 *always_out_of_cache = (volatile u8 *)alloc(CACHE_LINE_SZ);
  for (int i = 0; i <= tries; i++) {
    ptr = take_branch[i] ? user_cache_line : kernel_cache_line;
    kernel_ptr_cache_flush();
    cache_line_flush(user_cache_line);
    cache_line_flush(&ptr);

    cache_line_flush(always_out_of_cache);
    cache_line_flush(&take_branch[i]);
    serialise();
    memory_barrier();

    if (take_branch[i]) {
#ifdef MITIGATE
      serialise();
#endif
      load(ptr);
    }

    serialise();

    sum += get_kernel_time() * !take_branch[i];
  }

  RESULT->cache_line_time_access_tot = sum;

  ker_close();
}

#include "../tester.c"
