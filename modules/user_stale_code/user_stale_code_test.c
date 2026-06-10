#include "user_stale_code_test.h"

#include "../tester.h"

#include "../cache/cache_test.h"

#include "immintr.h"
#include "mem.h"
#include "types.h"

AS_RESULT(user_stale_code_result_t);

#define CACHE_LINE_SZ 4096
#define CACHE_LINE_ALIGNED __attribute__((aligned(CACHE_LINE_SZ)))
#define CACHE_LINE_ALIGNED_PTR __attribute__((aligned(CACHE_LINE_SZ)))

static void __attribute__((__noinline__))
__attribute__((aligned(CACHE_LINE_SZ))) test_access(unsigned long addr) {
  load((void *)addr);
}

static unsigned long **juck1;
static unsigned long *juck2;
static unsigned long juck3;

void func(request_dependencies_t *args) {
#ifdef RUNNER_KERNEL
  RESULT->iters = 1;
  return;
#endif
  cache_result_t *cache_r = args[1];
  /* printf("%d\n", cache_r->tries); */

  unsigned long page = (unsigned long)&test_access & ~0xfffUL;
  mem_protect((void *)page, 4096, MPROT_EXEC | MPROT_READ | MPROT_WRITE);

  RESULT->uncached_access_time = cache_r->uncached_access_time;
  RESULT->overhead = cache_r->overhead;
  RESULT->iters = cache_r->tries;

  volatile u8 *ptr = (volatile u8 *)alloc(CACHE_LINE_SZ);

  for (usize i = 0; i < RESULT->iters; i++) {
    cache_line_flush(&juck1);
    cache_line_flush(&juck2);
    cache_line_flush(&juck3);

    mem_protect(ptr, CACHE_LINE_SZ, MPROT_READ | MPROT_WRITE);
    cache_line_flush(ptr);
    mem_protect(ptr, CACHE_LINE_SZ, MPROT_NONE);

    memory_barrier();

    char tmp = *((volatile char *)test_access);

    memory_barrier();

    /* Build stale chain */
    juck3 = __asm_ret;
    /* juck3 = 0xc3; */
    cache_line_flush(&juck3);
    juck2 = &juck3;
    cache_line_flush(&juck2);
    juck1 = &juck2;
    cache_line_flush(&juck1);
    memory_barrier();

    /* Overwrite scsb entry transiently */
    *((volatile char *)test_access) = **juck1;

#ifdef MITIGATE
    memory_barrier();
#endif

    /* Speculative access */
    test_access((usize)ptr);

    /* Restore */
    *((volatile char *)test_access) = tmp;

    mem_protect(ptr, CACHE_LINE_SZ, MPROT_READ | MPROT_WRITE);
    serialise();
    memory_barrier();
    volatile u64 start = get_cycle();

    load(ptr);

    serialise();
    read_memory_barrier();
    volatile u64 end = get_cycle();
    RESULT->measured_access_time_tot += end - start;

    mem_protect(ptr, CACHE_LINE_SZ, MPROT_NONE);
  }
}

#include "../tester.c"
