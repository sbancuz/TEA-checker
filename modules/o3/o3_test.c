#include "o3_test.h"
#include "../cache/cache_test.h"

#include "../tester.h"

#include "immintr.h"
#include "mem.h"
#include "types.h"

#include "instructions.h.out"

AS_RESULT(o3_result_t);

// TODO: Find the correct value before doing this
#define CACHE_LINE_SZ 4096

void func(request_dependencies_t *args) {
  unsigned char arr[CACHE_LINE_SZ] = {};

  cache_result_t *cache_r = args[1];
  RESULT->number_of_instructions = nodep_xor_count;
  RESULT->tries = cache_r->tries;
  RESULT->overhead = cache_r->overhead;

  usize sum = 0;
  for (int i = 0; i <= cache_r->tries; i++) {
    serialise();
    memory_barrier();

    volatile u64 start = get_cycle();
    nodep_xor;

    serialise();
    memory_barrier();
    sum += (get_cycle() - start);
  }

  RESULT->nodep_instruction_time_tot = sum;

  sum = 0;
  for (int i = 0; i <= cache_r->tries; i++) {
    cache_line_flush(arr);
    serialise();
    memory_barrier();
    volatile u64 start = get_cycle();

    load(arr);
#ifdef MITIGATE
    memory_barrier();
#endif
    nodep_xor;

    serialise();
    memory_barrier();
    sum += (get_cycle() - start);
  }

  RESULT->uncached_access_time_with_instruction_tot = sum;
  RESULT->uncached_access_time = cache_r->uncached_access_time;
}

#include "../tester.c"
