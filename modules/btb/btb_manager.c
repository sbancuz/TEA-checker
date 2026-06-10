#include "../commands.h"
#include "btb_test.h"

#define IMPLEMENTATIONS
#include "../../libs/lbstd.h"
#include <stdlib.h>

EXPORT_RESULT_SETUP(request_dependencies_t *dependencies) { return OK; }

EXPORT_RESULT_STRUCT_SIZE() { return sizeof(btb_result_t); }

EXPORT_RESULT_STRUCT_DIAGNOSTICS(btb_result_t *result) {
  double aa = (double)result->A_after_A_tot / result->tries - result->overhead;
  double ab = (double)result->A_after_B_tot / result->tries - result->overhead;
  double ba = (double)result->B_after_A_tot / result->tries - result->overhead;
  plog(INFO, "A_after_B: %f",
       (double)result->A_after_B_tot / result->tries - result->overhead);
  plog(INFO, "B_after_A: %f",
       (double)result->B_after_A_tot / result->tries - result->overhead);
  plog(INFO, "A_after_A: %f",
       (double)result->A_after_A_tot / result->tries - result->overhead);

  result->aa = aa;
  result->ba = ba;
  result->ab = ab;

  if (!are_close(aa, ab, 20.f) && !are_close(aa, ba, 20.f)) {
    return OK;
  }

  plog(INFO, "No btb");
  return KO;
}
