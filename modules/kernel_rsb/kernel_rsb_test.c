#include "kernel_rsb_test.h"

#include "../tester.h"

#include "../cache/cache_test.h"
#include "../rsb/rsb_test.h"

#include "immintr.h"
#include "mem.h"
#include "rand.h"
#include "types.h"

AS_RESULT(kernel_rsb_result_t);

volatile u8 *ptr;

s32 *take_branch CACHE_LINE_ALIGNED;
s32 idx;
#define TRAINING_LOOPS 1000

no_inline void victim() { return; }
no_inline void load_ptr() {
  if (take_branch[idx]) {
    load(ptr);
  }
}

no_inline void rsb_safe_target() { read_memory_barrier(); }

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
  ker_open();
  volatile u8 CACHE_LINE_ALIGNED *kernel_cache_line =
      (volatile u8 *)get_kernel_ptr();
  volatile u8 *user_cache_line = (volatile u8 *)alloc(CACHE_LINE_SZ);

  for (idx = 0; idx < cache_r->tries; idx++) {
    ptr = user_cache_line;

    for (int j = 0; j < TRAINING_LOOPS; j++) {
      load_ptr();
    }

    ptr = take_branch[idx] ? user_cache_line : kernel_cache_line;
    kernel_ptr_cache_flush();
    cache_line_flush(user_cache_line);
    cache_line_flush(&ptr);

    cache_line_flush(&take_branch[idx]);
    serialise();
    memory_barrier();

    rsb_poison(rsb_depth);
#ifdef MITIGATE
    // Keep the RSB always full
    rsb_stuff(READINGS);
    serialise();
#endif

    victim();

    serialise();
    sum += get_kernel_time() * !take_branch[idx];
  }

  RESULT->cache_line_time_access_tot = sum;
}

#include "../tester.c"
