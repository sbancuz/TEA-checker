#include "lfb_test.h"

#include "../tester.h"

#include "immintr.h"
#include "mem.h"
#include "types.h"

#include "../cache/cache_test.h"

AS_RESULT(lfb_result_t);

#define PAGE_SIZE 4096
#define MEASURE_ITERS 100000

void func(request_dependencies_t *args) {
  cache_result_t *cache_r = args[1];

  RESULT->variants = LFB_TEST_VARIANTS_COUNT;
  RESULT->iterations = MEASURE_ITERS;
  RESULT->cache_hit_time = cache_r->cached_access_time;

  u8 *buf = alloc(LFB_TEST_VARIANTS_COUNT * PAGE_SIZE);
  for (usize n = 1; n <= LFB_TEST_VARIANTS_COUNT; n++) {
    for (int iter = 0; iter < MEASURE_ITERS; iter++) {
      serialise();
      memory_barrier();

      volatile usize start = get_cycle();

      // Issue n distinct, L1‑missing loads
      for (usize i = 0; i < n; i++) {
        load(buf + i * PAGE_SIZE);
#ifdef MITIGATE
        serialise();
#endif
      }

      volatile usize end = get_cycle();
      read_memory_barrier();
      RESULT->raw_readings[n] += (end - start);
    }
  }
}

#include "../tester.c"
