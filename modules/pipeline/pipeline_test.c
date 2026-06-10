#include "pipeline_test.h"

#include "../cache/cache_test.h"
#include "../tester.h"

#include "immintr.h"
#include "mem.h"
#include "types.h"

AS_RESULT(pipeline_result_t);

void func(request_dependencies_t *args) {
  cache_result_t *cache_r = args[1];
  /* RESULT->number_of_instructions = 125; */
  RESULT->tries = cache_r->tries;

  u64 x = 0;
  usize sum = 0;
  for (int i = 0; i <= cache_r->tries; i++) {
    serialise();
    memory_barrier();

    volatile u64 start = get_cycle();
    add5(x);
#ifdef MITIGATE
    serialise();
#endif
    add5(x);
#ifdef MITIGATE
    serialise();
#endif
    add5(x);

    serialise();
    memory_barrier();
    sum += (get_cycle() - start);
  }

  RESULT->no_interleaved_tot = sum;
  sum = 0;
  for (int i = 0; i <= cache_r->tries; i++) {
    serialise();
    memory_barrier();

    volatile u64 start = get_cycle();
    add(x);

    serialise();
    memory_barrier();
    sum += (get_cycle() - start);
  }

  RESULT->interleaved_tot = sum;
}

#include "../tester.c"
