#include "../commands.h"
#include "simd_test.h"

#define IMPLEMENTATIONS
#include "../../libs/lbstd.h"
#include <stdlib.h>

EXPORT_RESULT_SETUP(request_dependencies_t *dependencies) { return OK; }

EXPORT_RESULT_STRUCT_SIZE() { return sizeof(simd_result_t); }

EXPORT_RESULT_STRUCT_DIAGNOSTICS(simd_result_t *result) {
  bool x86 = result->x86.sse | result->x86.sse2 | result->x86.sse3 |
             result->x86.avx | result->x86.avx2 | result->x86.avx512;

  bool arm = result->arm.neon | result->arm.sve;
  bool riscv = result->riscv.vector;
  result->supported = x86 | arm | riscv;

  if (result->supported) {
    plog(INFO, "Supported");
    return OK;
  }

  return KO;
}
