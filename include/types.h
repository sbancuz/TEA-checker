#ifndef _TYPES
#define _TYPES

#ifdef RUNNER_KERNEL
#include <linux/types.h>
#else
typedef unsigned long long u64;
typedef long long s64;

typedef unsigned long u32;
typedef unsigned long s32;

typedef unsigned short u16;
typedef short s16;

typedef unsigned char u8;
typedef char s8;

typedef _Bool bool;
#define true 1
#define false 0
#endif

#if __SIZEOF_POINTER__ == 8
typedef u64 usize;
typedef s64 isize;
#else
typedef u32 usize;
typedef s32 ssize;
#endif

#define FP_SHIFT 16
#define FP_SCALE (1 << FP_SHIFT)

typedef u64 fix64;

static inline fix64 fx64(u64 x) { return x << FP_SHIFT; }
static inline u64 fx64uint(fix64 x) { return x >> FP_SHIFT; }
static inline u64 fx64intr(fix64 x) {
  return (x + (FP_SCALE >> 1)) >> FP_SHIFT;
}

static inline fix64 fx64add(fix64 a, fix64 b) { return a + b; }
static inline fix64 fx64sub(fix64 a, fix64 b) { return a - b; }

static inline fix64 fx64mul(fix64 a, fix64 b) {
  return (fix64)(((u64)a * b) >> FP_SHIFT);
}

static inline fix64 fx64div(fix64 a, fix64 b) {
  return (fix64)(((u64)a << FP_SHIFT) / b);
}

typedef u32 fix32;
static inline fix32 fx32(u32 x) { return x << FP_SHIFT; }
static inline u32 fx32uint(fix32 x) { return x >> FP_SHIFT; }
static inline u32 fx32uintr(fix32 x) {
  return (x + (FP_SCALE >> 1)) >> FP_SHIFT;
}

static inline fix32 fx32add(fix32 a, fix32 b) { return a + b; }
static inline fix32 fx32sub(fix32 a, fix32 b) { return a - b; }

static inline fix32 fx32mul(fix32 a, fix32 b) {
  return (fix32)(((u32)a * b) >> FP_SHIFT);
}

static inline fix32 fx32div(fix32 a, fix32 b) {
  return (fix32)(((u32)a << FP_SHIFT) / b);
}
#endif // _TYPES
