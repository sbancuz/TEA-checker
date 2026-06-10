#include "kernel_stl_test.h"

#include "../tester.h"

#include "../cache/cache_test.h"

#include "immintr.h"
#include "mem.h"
#include "rand.h"
#include "types.h"

AS_RESULT(kernel_stl_result_t);

// TODO: Find the correct value before doing this
#define CACHE_LINE_SZ 4096
#define CACHE_LINE_ALIGNED __attribute__((aligned(CACHE_LINE_SZ)))
#define CACHE_LINE_ALIGNED_PTR __attribute__((aligned(CACHE_LINE_SZ)))

#ifdef TARGET_X86_64
#ifdef MITIGATE
#define stl_test                                                               \
  "mov (%1), %%edx;\n"                                                         \
  "serialize;\n"                                                               \
  "mov %%rdx, (%0);\n"                                                         \
  "serialize;\n"
#else
#define stl_test                                                               \
  "mov (%1), %%edx;\n"                                                         \
  "mov %%rdx, (%0);\n"
#endif

#define clobbers "rdx", "memory"
#elif TARGET_RISCV
#ifdef MITIGATE
#define stl_test                                                               \
  "lw t0, 0(%1)\n"                                                             \
  "fence iorw, iorw\n"                                                         \
  "fence.i\n"                                                                  \
  "sd t0, 0(%0)\n"                                                             \
  "fence iorw, iorw\n"                                                         \
  "fence.i\n"
#else
#define stl_test                                                               \
  "lw t0, 0(%1)\n"                                                             \
  "sd t0, 0(%0)\n"
#endif

#define clobbers "t0", "memory"
#endif

void func(request_dependencies_t *args) {
  cache_result_t *cache_r = args[1];

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
  ker_open();
  volatile usize CACHE_LINE_ALIGNED *kernel_cache_line = get_kernel_ptr();
  volatile usize *user_cache_line = (volatile usize *)alloc(CACHE_LINE_SZ * sizeof(usize));
  volatile u8 *shadow_page = (volatile u8 *)alloc(CACHE_LINE_SZ);

  volatile usize *ptr = user_cache_line;

  volatile u8 *always_out_of_cache = (volatile u8 *)alloc(CACHE_LINE_SZ);
  for (int i = 0; i < cache_r->tries; i++) {
    ptr = take_branch[i] ? user_cache_line : kernel_cache_line;
    kernel_ptr_cache_flush();
    cache_line_flush(user_cache_line);

    cache_line_flush(always_out_of_cache);
    serialise();
    memory_barrier();

    load(always_out_of_cache);
    if (take_branch[i]) {
      __asm__ __volatile__(stl_test
                           :
                           : "r"(shadow_page), // %0 = destination pointer
                             "r"(ptr)          // %1 = source pointer
                           : clobbers);
    }

    serialise();

    sum += get_kernel_time() * !take_branch[i];
  }

  RESULT->cache_line_time_access_tot = sum;
}

#include "../tester.c"
