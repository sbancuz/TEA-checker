#ifndef _fpu_TEST
#define _fpu_TEST

#include "test_name.h.out"

#include "../tester.h"
#include "types.h"

typedef struct {
  bool fpu;
} fpu_x86_t;

typedef struct {
  bool fpu;
} fpu_arm_t;

typedef struct {
  bool floats;
  bool doubles;
} fpu_riscv_t;

typedef struct {
  bool supported;
  fpu_arm_t arm;
  fpu_x86_t x86;
  fpu_riscv_t riscv;

} fpu_result_t;

#endif
