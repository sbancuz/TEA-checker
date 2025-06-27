#include "measure_cache.h"
#include "../tester.h"

#include "immintr.h"
#include "mem.h"
#include "types.h"

#define TRIES 1000000

static DECLARE_RESULT;

void func(void *args) {
  unsigned char arr[1] = {};

  RESULT->tries = TRIES;

  u64 sum = 0;
  for (int i = 0; i <= TRIES; i++) {
    volatile u64 start = get_cycle();
    sum += (get_cycle() - start);
    serialise();
  }

  RESULT->overhead_tot = sum;

  for (int i = 0; i < 1000; i++)
    load(arr);

  sum = 0;
  for (int i = 0; i <= TRIES; i++) {
    volatile u64 start = get_cycle();

    load(arr);

    serialise();
    memory_barrier();
    sum += (get_cycle() - start);
  }

  RESULT->cached_access_time_tot = sum;

  sum = 0;
  for (int i = 0; i <= TRIES; i++) {
    cache_line_flush(arr);
    volatile u64 start = get_cycle();

    load(arr);

    serialise();
    memory_barrier();
    sum += (get_cycle() - start);
  }

  RESULT->uncached_access_time_tot = sum;
}

#include "../tester.c"
