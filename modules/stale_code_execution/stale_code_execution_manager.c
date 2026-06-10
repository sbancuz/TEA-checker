#include "../commands.h"
#include "stale_code_execution_test.h"

#define IMPLEMENTATIONS
#include "../../libs/lbstd.h"
#include <stdlib.h>

EXPORT_RESULT_SETUP(request_dependencies_t *dependencies) { return OK; }

EXPORT_RESULT_STRUCT_SIZE() { return sizeof(stale_code_execution_result_t); }

EXPORT_RESULT_STRUCT_DIAGNOSTICS(stale_code_execution_result_t *result) {
  plog(INFO, "stale_code_execution module called!");
  const double epsilon = 0.5;
  result->measured_access_time =
      (double)result->measured_access_time_tot / result->iters -
      result->overhead;

  result->normal_call_time =
      (double)result->normal_call_time_tot / result->iters - result->overhead;

  result->machine_clear_call_time =
      (double)result->machine_clear_call_time_tot / result->iters -
      result->overhead;

  plog(INFO, "%f", result->normal_call_time);
  plog(INFO, "%f", result->machine_clear_call_time);
  plog(INFO, "%f %f", result->measured_access_time,
       result->uncached_access_time);

  if (result->measured_access_time <
      result->uncached_access_time * (1 - epsilon)) {
    plog(INFO, "OK");
    return OK;
  } else {
    plog(INFO, "KO");
    return KO;
  }
}
