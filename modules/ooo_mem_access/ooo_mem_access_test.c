#include "ooo_mem_access_test.h"

#include "../cache/cache_test.h"
#include "../tester.h"

#include "immintr.h"
#include "mem.h"
#include "rand.h"
#include "types.h"

#include "instructions.h.out"

AS_RESULT(ooo_mem_access_result_t);

// TODO: Find the correct value before doing this
#define CACHE_LINE_SZ 4096
#define CACHE_LINE_ALIGNED __attribute__((aligned(CACHE_LINE_SZ)))
#define CACHE_LINE_ALIGNED_PTR __attribute__((aligned(CACHE_LINE_SZ)))

void func(request_dependencies_t *args) {
  cache_result_t *cache_r = args[1];

  RESULT->cache_line_access_count = 0;
  RESULT->overhead = cache_r->overhead;
  RESULT->uncached_access_time = cache_r->uncached_access_time;

  int *take_branch CACHE_LINE_ALIGNED = alloc(cache_r->tries * sizeof(u64));

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
  volatile u8 *cache_line = (volatile u8 *)alloc(CACHE_LINE_SZ);
  volatile u8 *always_out_of_cache = (volatile u8 *)alloc(CACHE_LINE_SZ);

  volatile u64 no_opt = 0;
  for (int i = 0; i <= cache_r->tries; i++) {
    /* mem_protect(cache_line, CACHE_LINE_SZ, PROT_READ | PROT_WRITE); */
    cache_line_flush(cache_line);
    /* if (!take_branch[i]) { */
    /* mem_protect(cache_line, CACHE_LINE_SZ, PROT_NONE); */
    /* } */

    cache_line_flush(always_out_of_cache);
    cache_line_flush(&take_branch[i]);
    serialise();
    memory_barrier();

    load(always_out_of_cache);
    if (take_branch[i]) { // Enter this speculatively
#ifdef MITIGATE
      memory_barrier();
#endif
      measure_window(no_opt);
      load(cache_line);
    }

    /* mem_protect(cache_line, CACHE_LINE_SZ, PROT_READ | PROT_WRITE); */
    serialise();
    volatile u64 start = get_cycle();

    load((volatile void *)cache_line);

    serialise();
    memory_barrier();
    volatile u64 end = get_cycle();
    sum += (end - start) * !take_branch[i];
  }

  RESULT->cache_line_time_access_tot = sum;
}

#include "../tester.c"
