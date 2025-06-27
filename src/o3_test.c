#define TEST_NAME "O3"
#include "tester.h"

#include "../immintr.h"
#include "../proofs/instructions.h"
#include <linux/types.h>
#define TRIES 1000000

// TODO: Find the correct value before doing this
#define CACHE_LINE_SZ 4096

#define FP_SHIFT 16
#define FP_SCALE (1 << FP_SHIFT)

typedef u64 fix64;

static inline fix64 fix(u64 x) { return x << FP_SHIFT; }
static inline u64 fxint(fix64 x) { return x >> FP_SHIFT; }
static inline u64 fxintr(fix64 x) { return (x + (FP_SCALE >> 1)) >> FP_SHIFT; }

static inline fix64 fxadd(fix64 a, fix64 b) { return a + b; }
static inline fix64 fxsub(fix64 a, fix64 b) { return a - b; }

static inline fix64 fxmul(fix64 a, fix64 b) {
  return (fix64)(((u64)a * b) >> FP_SHIFT);
}

static inline fix64 fxdiv(fix64 a, fix64 b) {
  return (fix64)(((u64)a << FP_SHIFT) / b);
}

static inline void fxprint(const char *label, fix64 x) {
  int i = fxint(x);
  int f = (x & (FP_SCALE - 1)) * 1000 / FP_SCALE;
  printk("%s = %d.%03d\n", label, i, f);
}

unsigned long long func(void *args) {
  unsigned char arr[CACHE_LINE_SZ] = {};

  const fix64 epsilon_ratio = (1 / 0.2f);
  fix64 overhead = 0;
  fix64 nodep_instruction_time = 0;
  fix64 uncached_access_time = 0;
  fix64 uncached_access_time_with_inst = 0;

  u64 sum = 0;
  for (int i = 0; i <= TRIES; i++) {
    volatile unsigned long long start = rdtsc();
    sum += (rdtsc() - start);
    serialize();
  }

  overhead = fxdiv(fix(sum), fix(TRIES));

  sum = 0;
  for (int i = 0; i <= TRIES; i++) {
    volatile unsigned long long start = rdtsc();
    nodep_xor;
    serialize();
    sum += (rdtsc() - start);
  }

  nodep_instruction_time =
      fxdiv(fxsub(fix(sum), overhead), fix((TRIES * reps)));

  sum = 0;
  for (int i = 0; i <= TRIES; i++) {
    clflush(arr);
    volatile unsigned long long start = rdtsc();
    __asm__ __volatile__("movl (%0), %%eax" ::"r"(arr) : "eax");
    serialize();
    mb();
    sum += (rdtsc() - start);
  }

  uncached_access_time = fxdiv(fxsub(fix(sum), overhead), fix(TRIES));

  sum = 0;
  for (int i = 0; i <= TRIES; i++) {
    clflush(arr);
    volatile unsigned long long start = rdtsc();
    __asm__ __volatile__("movl (%0), %%eax" ::"r"(arr) : "eax");
    nodep_xor;
    serialize();
    mb();
    sum += (rdtsc() - start);
  }

  uncached_access_time_with_inst = fxdiv(fxsub(fix(sum), overhead), fix(TRIES));

  s64 margin =
      fxint(uncached_access_time_with_inst) - fxint(uncached_access_time);

  fxprint("avg_overhead: ", overhead);

  fxprint("avg_nodep_instruction_time: ", nodep_instruction_time);
  fxprint("avg_nodep_instruction_time * ops: ", nodep_instruction_time * reps);
  fxprint("avg_uncached_access_time: ", uncached_access_time);
  fxprint("avg_uncached_access_time_with_inst: ",
          uncached_access_time_with_inst);

  if (margin < (s64)(fxint(uncached_access_time) / epsilon_ratio)) {
    return 0;
  }

  return 1;
}

#include "tester.c"
