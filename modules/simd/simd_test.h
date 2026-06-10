#ifndef _simd_TEST
#define _simd_TEST

#include "test_name.h.out"

#include "../tester.h"
#include "types.h"

typedef struct {
  bool sse;
  bool sse2;
  bool sse3;
  bool avx;
  bool avx2;
  bool avx512;
} simd_x86_t;

typedef struct {
  bool neon;
  bool sve;
} simd_arm_t;

typedef struct {
  bool vector;
} simd_riscv_t;

typedef struct {
  bool supported;
  simd_arm_t arm;
  simd_x86_t x86;
  simd_riscv_t riscv;
} simd_result_t;

#endif
