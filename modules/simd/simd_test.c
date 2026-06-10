#include "simd_test.h"

#include "../tester.h"

#include "immintr.h"
#include "mem.h"
#include "types.h"

AS_RESULT(simd_result_t);

void func(request_dependencies_t *args) {
#ifdef MITIGATE
  return;
#endif

#ifdef __SSE__
  RESULT->x86.sse = true;
#endif

#ifdef __SSE2__
  RESULT->x86.sse2 = true;
#endif

#ifdef __SSE3__
  RESULT->x86.sse3 = true;
#endif

#ifdef __AVX__
  RESULT->x86.avx = true;
#endif

#ifdef __AVX2__
  RESULT->x86.avx2 = true;
#endif

#ifdef __AVX512F__
  RESULT->x86.avx512 = true;
#endif

#ifdef __ARM_NEON
  RESULT->arm.neon = true;
#endif

#ifdef __ARM_FEATURE_SVE
  RESULT->arm.sve = true;
#endif

#ifdef __riscv_vector
  RESULT->riscv.vector = true;
#endif
}

#include "../tester.c"
