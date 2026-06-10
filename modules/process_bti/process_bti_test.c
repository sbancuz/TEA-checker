#include "process_bti_test.h"

#include "../tester.h"

#include "../cache/cache_test.h"

#include "immintr.h"
#include "mem.h"
#include "types.h"

AS_RESULT(process_bti_result_t);

volatile u8 CACHE_LINE_ALIGNED ptr[CACHE_LINE_SZ] = {1};

no_inline CACHE_LINE_ALIGNED void target_A(void) { load(ptr); }
no_inline CACHE_LINE_ALIGNED void target_B(void) { nop(); }

no_inline CACHE_LINE_ALIGNED void target_1(void) { nop(); }
no_inline CACHE_LINE_ALIGNED void target_2(void) { nop(); }
no_inline CACHE_LINE_ALIGNED void target_3(void) { nop(); }
no_inline CACHE_LINE_ALIGNED void target_4(void) { nop(); }
no_inline CACHE_LINE_ALIGNED void target_5(void) { nop(); }
no_inline CACHE_LINE_ALIGNED void target_6(void) { nop(); }
no_inline CACHE_LINE_ALIGNED void target_7(void) { nop(); }
no_inline CACHE_LINE_ALIGNED void target_8(void) { nop(); }

void (*targets[8])(void) = {
    target_1, target_2, target_3, target_4,
    target_5, target_6, target_7, target_8,
};

void indirect_call(volatile void (*fp)()) { fp(); }

#ifdef MITIGATE
#define mcall indirect_call(targets[get_rand() % 8]);
#else
#define mcall
#endif

void func(request_dependencies_t *args) {
  cache_result_t *cache_r = args[1];
  /* RESULT->iters = cache_r->tries / 100; */
  /* RESULT->uncached_access_time = cache_r->uncached_access_time; */
  /* RESULT->overhead = cache_r->overhead; */
  RESULT->iters = 10;
  RESULT->uncached_access_time = 322.f;
  RESULT->overhead = 78.f;
  const u64 train_iters = 1000;

  for (int j = 0; j < RESULT->iters; j++) {
    cache_line_flush(ptr);

    serialise();
    memory_barrier();
    for (int i = 0; i < train_iters; i++) {
      mcall;
      indirect_call(target_A);
    }

    serialise();
    memory_barrier();

    mcall;
    indirect_call(target_B);

    memory_barrier();
    serialise();

    volatile u64 start = get_cycle();

    load(ptr);

    read_memory_barrier();
    volatile u64 end = get_cycle();
    RESULT->measured_access_time_tot += end - start;
  }
}

#include "../tester.c"
