#include "cache_test.h"

#include "../tester.h"

#include "immintr.h"
#include "mem.h"
#include "types.h"

#define CACHE_LINE_SZ 4096
#define CACHE_LINE_ALIGNED __attribute__((aligned(CACHE_LINE_SZ)))
#define CACHE_LINE_ALIGNED_PTR __attribute__((aligned(CACHE_LINE_SZ)))
AS_RESULT(cache_result_t);

#define load5(x)                                                               \
  load(x);                                                                     \
  load(x);                                                                     \
  load(x);                                                                     \
  load(x);                                                                     \
  load(x)

#define load32(x)                                                              \
  load(x);                                                                     \
  load(x);                                                                     \
  load5(x);                                                                    \
  load5(x);                                                                    \
  load5(x);                                                                    \
  load5(x);                                                                    \
  load5(x);                                                                    \
  load5(x)

void func(request_dependencies_t *args) {
  RESULT->tries = *((usize *)args[0]);
  /* printf("%d\n", RESULT->tries); */

  volatile u8 CACHE_LINE_ALIGNED arr[CACHE_LINE_SZ] = {0};

  u64 sum = 0;
  for (int i = 0; i < RESULT->tries; i++) {
    serialise();
    memory_barrier();
    volatile u64 start = get_cycle();
    serialise();
    read_memory_barrier();
    sum += (get_cycle() - start);

    /* if (i % 10 == 0) { */
    /* printf("%d\n", i); */
    /* } */
  }

  RESULT->overhead_tot = sum;

  for (int i = 0; i < 100; i++)
    load(arr);

  sum = 0;
  for (int i = 0; i < RESULT->tries; i++) {
    serialise();
    memory_barrier();

#ifdef MITIGATE
    /* printf("HELLO\n"); */
    cache_line_flush(arr);
    serialise();
    memory_barrier();
#endif

    volatile u64 start = get_cycle();

    load(arr);

    serialise();
    read_memory_barrier();
    sum += (get_cycle() - start);
    /* if (i % 10 == 0) { */
    /* printf("%d\n", i); */
    /* } */
  }

  RESULT->cached_access_time_tot = sum;

  sum = 0;
  for (int i = 0; i < RESULT->tries; i++) {
    cache_line_flush(arr);
    serialise();
    memory_barrier();
    volatile u64 start = get_cycle();

    load(arr);

    serialise();
    read_memory_barrier();
    sum += (get_cycle() - start);
    /* if (i % 10 == 0) { */
    /* printf("%d\n", i); */
    /* } */
  }

  RESULT->uncached_access_time_tot = sum;
}

#include "../tester.c"
