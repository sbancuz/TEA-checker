#include "process_lfb_test.h"

#include "../tester.h"

#include "../cache/cache_test.h"

#include "immintr.h"
#include "mem.h"
#include "rand.h"
#include "thread.h"
#include "types.h"

AS_RESULT(process_lfb_result_t);

#define TRAINING_LOOPS 30
#define ARRAY_SIZE (64 * 1024 * 1024)

usize bounds = 16;
volatile usize small_array[16];
volatile usize *big_array;

void speculative_access(usize idx) {
  if (idx < bounds) {
    load(&small_array[idx]);
  }
}

volatile int keep_running = 1;

void *producer_thread(void *arg) {
  while (keep_running) {
    for (usize i = 0; i < ARRAY_SIZE; i += 64) {
      load(&big_array[i]);
#ifdef MITIGATE
      serialise();
#endif
    }
  }

  return (void *)0;
}

volatile u8 CACHE_LINE_ALIGNED cache_line[CACHE_LINE_SZ] = {0};

void func(request_dependencies_t *args) {
  cache_result_t *cache_r = args[1];

  RESULT->overhead = cache_r->overhead;
  RESULT->uncached_access_time = cache_r->uncached_access_time;
  RESULT->cache_line_access_count = cache_r->tries;
  small_array[0] = (usize)&small_array;
  big_array = (volatile usize *)alloc(ARRAY_SIZE * sizeof(usize));

  for (usize i = 0; i < ARRAY_SIZE; i++)
    big_array[i] = (usize)cache_line;

  for (int i = 0; i < 16; i++)
    small_array[i] = get_rand();

  thread_t prod;
  thread_create(&prod, producer_thread, (void *)0, 0);

  int total_hits = 0;

  u64 sum = 0;
  for (int iter = 0; iter < cache_r->tries; iter++) {
    cache_line_flush(cache_line);
    memory_barrier();

    for (int i = 0; i < TRAINING_LOOPS; i++)
      speculative_access(i % bounds);

    serialise();
    memory_barrier();

    speculative_access(0xdeadbeef);

    serialise();
    volatile u64 start = get_cycle();

    load((volatile void *)cache_line);

    serialise();
    memory_barrier();
    volatile u64 end = get_cycle();
    sum += (end - start);
  }

  keep_running = 0;
  thread_join(prod, 0);

  RESULT->cache_line_time_access_tot = sum;
}

#include "../tester.c"
