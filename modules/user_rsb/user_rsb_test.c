#include "user_rsb_test.h"

#include "../tester.h"

#include "../cache/cache_test.h"
#include "../rsb/rsb_test.h"

#include "immintr.h"
#include "mem.h"
#include "rand.h"
#include "types.h"

AS_RESULT(user_rsb_result_t);

volatile u8 CACHE_LINE_ALIGNED cache_line[CACHE_LINE_SZ] = {1};
s32 *take_branch CACHE_LINE_ALIGNED;
s32 idx;
#define TRAINING_LOOPS 1000

no_inline void victim(void) { return; }
no_inline void load_ptr(void) {
  if (take_branch[idx]) {
    load(cache_line);
  }
}

no_inline void rsb_safe_target(void) { read_memory_barrier(); }

no_inline void rsb_stuff(int depth) {
  if (depth <= 0) {
    rsb_safe_target();
    return;
  }

  rsb_stuff(depth - 1);
}

no_inline void rsb_poison(int depth) {
  if (depth <= 0)
    return;
  rsb_poison(depth - 1);
}

void func(request_dependencies_t *args) {
  cache_result_t *cache_r = args[1];
  rsb_result_t *rsb_r = args[2];

  RESULT->overhead = cache_r->overhead;
  RESULT->uncached_access_time = cache_r->uncached_access_time;
  u64 rsb_depth = rsb_r->return_stack_buffer_size + 16; // To be extra safe

  take_branch = alloc(cache_r->tries * sizeof(u64));

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

  for (idx = 0; idx < cache_r->tries; idx++) {
    mem_protect(cache_line, CACHE_LINE_SZ, MPROT_READ | MPROT_WRITE);
    cache_line_flush(cache_line);

    for (int j = 0; j < TRAINING_LOOPS; j++) {
      load_ptr();
    }

    cache_line_flush(cache_line);
    cache_line_flush(&take_branch[idx]);
    if (!take_branch[idx]) {
      mem_protect(cache_line, CACHE_LINE_SZ, MPROT_NONE);
    }
    serialise();
    memory_barrier();

    rsb_poison(rsb_depth);

    victim();

    mem_protect(cache_line, CACHE_LINE_SZ, MPROT_READ | MPROT_WRITE);
    serialise();
    volatile u64 start = get_cycle();

    load((volatile void *)cache_line);
#ifdef MITIGATE
    // Keep the RSB always full
    rsb_stuff(READINGS);
    serialise();
#endif

    serialise();
    memory_barrier();
    volatile u64 end = get_cycle();
    sum += (end - start) * !take_branch[idx];
  }

  RESULT->cache_line_time_access_tot = sum;
}

#include "../tester.c"
