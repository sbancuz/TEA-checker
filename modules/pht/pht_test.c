#include "pht_test.h"

#include "../cache/cache_test.h"
#include "../tester.h"

#include "immintr.h"
#include "mem.h"
#include "rand.h"
#include "types.h"

AS_RESULT(pht_result_t);

// TODO: Find the correct value before doing this
#define CACHE_LINE_SZ 4096

int fill_random(int *data, int *mask, int tries) {
  int not_taken = 0;
  for (int i = 0; i < tries; i++) {
    data[i] = 1;
    mask[i] = 0;
    if (i % 32 == 0) {
      u32 bit;
      u32 count = 0;
      do {
        bit = get_rand() % 2;
        count += 1;
      } while (bit == 1);

      data[i] = 0;
      mask[i] = 1;
    }

    not_taken += mask[i];
  }

  return not_taken;
}

int fill_predictable(int *data, int *mask, int tries) {
  for (int i = 0; i < tries; i++) {
    data[i] = (i < tries / 2) ? 1 : 0;
    mask[i] = (i < 32 || (i < tries / 2 + 32 && tries / 2)) ? 0 : 1;
  }

  return tries - 64;
}

void func(request_dependencies_t *args) {
  unsigned char arr[CACHE_LINE_SZ] = {};

  cache_result_t *cache_r = args[1];
  /* RESULT->number_of_instructions = 125; */
  RESULT->tries = cache_r->tries;

  u64 x = 0;
  usize sum = 0;
  for (int i = 0; i <= cache_r->tries; i++) {
    volatile u64 start = get_cycle();
    add25(x);
    serialise();
    memory_barrier();
    sum += (get_cycle() - start);
  }
  RESULT->instruction_time_tot = sum;

  RESULT->overhead = cache_r->overhead;

  int *data = alloc(cache_r->tries * sizeof(u64));
  int *mask = alloc(cache_r->tries * sizeof(u64));

#ifdef MITIGATE
  RESULT->taken_counted = fill_random(data, mask, cache_r->tries);
#else
  RESULT->taken_counted = fill_predictable(data, mask, cache_r->tries);
#endif

  sum = 0;
  x = 0;
  for (int i = 0; i < cache_r->tries; i++) {
    volatile u64 start = get_cycle();

    if (data[i]) {
      add25(x);
    } else {
      add25(x);
    }

    serialise();
    memory_barrier();
    sum += (get_cycle() - start) * mask[i];
  }

  RESULT->branch_taken_time_tot = sum;

  sum = 0;
  RESULT->not_taken_counted = fill_random(data, mask, cache_r->tries);

  sum = 0;
  x = 0;
  for (int i = 0; i < cache_r->tries; i++) {
    volatile u64 start = get_cycle();

    if (data[i]) {
      add25(x);
    } else {
      add25(x);
    }

    serialise();
    memory_barrier();
    sum += (get_cycle() - start) * mask[i];
  }

  RESULT->branch_not_taken_time_tot = sum;

  //////////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////

  sum = 0;
  x = 0;
  for (int i = 0; i < cache_r->tries; i++) {
    volatile u64 start = get_cycle();
    add625(x);
    serialise();
    memory_barrier();
    sum += (get_cycle() - start);
  }

  RESULT->instruction_time_tot_long = sum;
#ifdef MITIGATE
  RESULT->taken_counted_long = fill_random(data, mask, cache_r->tries);
#else
  RESULT->taken_counted_long = fill_predictable(data, mask, cache_r->tries);
#endif

  sum = 0;
  x = 0;
  for (int i = 0; i < cache_r->tries; i++) {
    volatile u64 start = get_cycle();

    if (data[i]) {
      add625(x);
    } else {
      add625(x);
    }

    serialise();
    memory_barrier();
    sum += (get_cycle() - start) * mask[i];
  }

  RESULT->branch_taken_time_tot_long = sum;

  sum = 0;
  RESULT->not_taken_counted_long = fill_random(data, mask, cache_r->tries);

  sum = 0;
  x = 0;
  for (int i = 0; i < cache_r->tries; i++) {
    volatile u64 start = get_cycle();

    if (data[i]) {
      add625(x);
    } else {
      add625(x);
    }

    serialise();
    memory_barrier();
    sum += (get_cycle() - start) * mask[i];
  }

  RESULT->branch_not_taken_time_tot_long = sum;
}

#include "../tester.c"
