#include "fpu_test.h"

#include "../tester.h"

#include "immintr.h"
#include "mem.h"
#include "types.h"

AS_RESULT(fpu_result_t);

void func(request_dependencies_t *args) {

#ifdef MITIGATE
  return;
#endif

#ifdef __FPU__
  RESULT->x86.fpu = true;
#endif

#ifdef __ARM_FP
  RESULT->arm.fpu = true;
#endif

#ifdef __riscv_f
  RESULT->riscv.floats = true;
#endif

#ifdef __riscv_d
  RESULT->riscv.doubles = true;
#endif
}

#include "../tester.c"
