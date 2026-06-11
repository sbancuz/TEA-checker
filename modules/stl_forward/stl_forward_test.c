#include "stl_forward_test.h"

#include "../cache/cache_test.h"
#include "../tester.h"

#include "immintr.h"
#include "mem.h"
#include "types.h"

AS_RESULT(stl_forward_result_t);

#ifdef TARGET_X86_64

#ifdef MITIGATE

#ifdef __SERIALIZE__
#define SER "serialize;\n"
#define ARGS
#define clobbers "rdx", "memory"

#else

#define SER "cpuid;\n"
#define ARGS "+a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
#define clobbers "memory"
#endif

#define stl_test                                                               \
  "mov (%[src]), %%edx;\n"                                                     \
  "serialize;\n"                                                               \
  "mov %%rdx, (%[dst]);\n" SER
#else

#define ARGS
#define clobbers "rdx", "memory"

#define stl_test                                                               \
  "mov (%[src]), %%edx;\n"                                                     \
  "mov %%rdx, (%[dst]);\n"
#endif

#elif TARGET_RISCV

#define ARGS

#ifdef MITIGATE
#define stl_test                                                               \
  "lw t0, 0(%[src])\n"                                                         \
  "fence iorw, iorw\n"                                                         \
  "fence.i\n"                                                                  \
  "sd t0, 0(%[dst])\n"                                                         \
  "fence iorw, iorw\n"                                                         \
  "fence.i\n"
#else
#define stl_test                                                               \
  "lw t0, 0(%[src])\n"                                                         \
  "sd t0, 0(%[dst])\n"
#endif

#define clobbers "t0", "memory"
#endif

#define stl_test10                                                             \
  stl_test stl_test stl_test stl_test stl_test stl_test stl_test stl_test      \
      stl_test stl_test

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
        register unsigned int eax = 0, ebx, ecx, edx;
        __asm__ __volatile__(
            stl_test10
            : ARGS
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
