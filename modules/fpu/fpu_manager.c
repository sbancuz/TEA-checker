#include "../commands.h"
#include "fpu_test.h"

#define IMPLEMENTATIONS
#include "../../libs/lbstd.h"
#include <stdlib.h>

EXPORT_RESULT_SETUP(request_dependencies_t *dependencies) { return OK; }

EXPORT_RESULT_STRUCT_SIZE() { return sizeof(fpu_result_t); }

EXPORT_RESULT_STRUCT_DIAGNOSTICS(fpu_result_t *result) {
  bool x86 = result->x86.fpu;
  bool arm = result->arm.fpu;
  bool riscv = result->riscv.floats | result->riscv.doubles;
  result->supported = x86 | arm | riscv;

  if (result->supported) {
    plog(INFO, "Supported");
    return OK;
  }
  return KO;
}
