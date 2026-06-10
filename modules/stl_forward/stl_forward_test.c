#include "stl_forward_test.h"

#include "../cache/cache_test.h"
#include "../tester.h"

#include "immintr.h"
#include "mem.h"
#include "types.h"

AS_RESULT(stl_forward_result_t);

#ifdef TARGET_X86_64
#ifdef MITIGATE
#define test                                                                   \
  "mov (%1), %%edx;\n"                                                         \
  "serialize;\n"                                                               \
  "mov %%rdx, (%0);\n"                                                         \
  "serialize;\n"
#else
#define test                                                                   \
  "mov (%1), %%edx;\n"                                                         \
  "mov %%rdx, (%0);\n"
#endif

#define clobbers "rdx", "memory"
#elif TARGET_RISCV
#ifdef MITIGATE
#define test                                                                   \
  "lw t0, 0(%1)\n"                                                             \
  "fence iorw, iorw\n"                                                         \
  "fence.i\n"                                                                  \
  "sd t0, 0(%0)\n"                                                             \
  "fence iorw, iorw\n"                                                         \
  "fence.i\n"
#else
#define test                                                                   \
  "lw t0, 0(%1)\n"                                                             \
  "sd t0, 0(%0)\n"
#endif

#define clobbers "t0", "memory"
#endif

#define test10 test test test test test test test test test test

void func(request_dependencies_t *args) {
  char *p = (char *)alloc(4096); // TODO: Swap with page size
  char *p_align = (char *)((unsigned long long)(p + 63) & ~0x3fULL);

  cache_result_t *cache_r = args[1];
  RESULT->overhead = cache_r->overhead;
  RESULT->iterations = cache_r->tries / 10;

  for (int o2 = 0; o2 < OFFSETS; o2++) {
    for (int o1 = 0; o1 < OFFSETS; o1++) {
      u64 sum = 0;
      for (int i = 0; i < RESULT->iterations; i++) {
        serialise();
        memory_barrier();

        volatile u64 start = get_cycle();
        __asm__ __volatile__(
            test10
            :
            : "r"((unsigned long long *)(p_align + o2)), // %0 (dest, 8-byte)
              "r"((unsigned *)(p_align + o1))            // %1 (src, 4-byte)
            : clobbers);

        serialise();
        memory_barrier();
        sum += get_cycle() - start;
      }

      RESULT->readings[o1 * OFFSETS + o2] = sum;
    }
  }
}

#include "../tester.c"
