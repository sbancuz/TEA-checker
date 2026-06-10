#include "btb_test.h"

#include "../cache/cache_test.h"
#include "../tester.h"

#include "immintr.h"
#include "mem.h"
#include "rand.h"
#include "types.h"

AS_RESULT(btb_result_t);

no_inline CACHE_LINE_ALIGNED void target_A(void) { nop(); }
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

void indirect_call(volatile void (*fp)(void)) { fp(); }

#ifdef MITIGATE
#define mcall indirect_call(targets[get_rand() % 8]);
#else
#define mcall
#endif

void func(request_dependencies_t *args) {
  cache_result_t *cache_r = args[1];
  RESULT->tries = cache_r->tries / 100;
  const u64 train_iters = 1000;

  for (int j = 0; j < RESULT->tries; j++) {
    {
      /* cache_line_flush(target_A); */
      /* cache_line_flush(target_B); */

      serialise();
      memory_barrier();
      for (int i = 0; i < train_iters; i++) {
        mcall;
        indirect_call(target_A);
      }

      serialise();
      memory_barrier();

      volatile u64 start = get_cycle();
      mcall;
      indirect_call(target_A);
      /* serialise(); */
      read_memory_barrier();
      volatile u64 end = get_cycle();

      RESULT->A_after_A_tot += end - start;
    }

    /* cache_line_flush(target_A); */
    /* cache_line_flush(target_B); */
    {
      serialise();
      memory_barrier();
      for (int i = 0; i < train_iters; i++) {
        mcall;
        indirect_call(target_B);
      }

      serialise();
      memory_barrier();

      volatile u64 start = get_cycle();
      mcall;
      indirect_call(target_A);
      /* serialise(); */
      read_memory_barrier();
      volatile u64 end = get_cycle();

      RESULT->A_after_B_tot += end - start;
    }

    /* cache_line_flush(target_A); */
    /* cache_line_flush(target_B); */
    {
      serialise();
      memory_barrier();
      for (int i = 0; i < train_iters; i++) {
        mcall;
        indirect_call(target_A);
      }

      serialise();
      memory_barrier();
      volatile u64 start = get_cycle();
      mcall;
      indirect_call(target_B);
      /* serialise(); */
      read_memory_barrier();
      volatile u64 end = get_cycle();

      RESULT->B_after_A_tot += end - start;
    }
  }
}

#include "../tester.c"
