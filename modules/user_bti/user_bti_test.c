#include "user_bti_test.h"

#include "../tester.h"

#include "../cache/cache_test.h"

#include "immintr.h"
#include "mem.h"
#include "rand.h"
#include "types.h"

AS_RESULT(user_bti_result_t);

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
  const u64 train_iters = 1000;
  target = alloc(sizeof(u64));

  for (int j = 0; j < RESULT->iters; j++) {
    mem_protect(ptr, CACHE_LINE_SZ, MPROT_READ | MPROT_WRITE);
    target_A();

    for (int i = 0; i < train_iters; i++) {
      *target = (u64)(&target_A);
      mcall;
      memory_barrier();
      indirect_call();
    }

    cache_line_flush(ptr);
    serialise();
    memory_barrier();

    for (int i = 0; i < train_iters; i++) {
      mcall;
      *target = (u64)(&target_A);
      memory_barrier();
      indirect_call();
    }

    cache_line_flush(ptr);
    serialise();

    *target = (u64)(&target_B);

    memory_barrier();
    serialise();

    cache_line_flush(target);

    mem_protect(ptr, CACHE_LINE_SZ, MPROT_NONE);
    memory_barrier();
    serialise();

    indirect_call();

    mem_protect(ptr, CACHE_LINE_SZ, MPROT_READ | MPROT_WRITE);
    memory_barrier();
    serialise();

    volatile u64 start = get_cycle();

    load(ptr);

    read_memory_barrier();
    volatile u64 end = get_cycle();
    RESULT->measured_access_time_tot += end - start;
    mem_protect(ptr, CACHE_LINE_SZ, MPROT_NONE);
  }
}

#include "../tester.c"
