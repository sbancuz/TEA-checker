#include "../commands.h"
#include "process_bti_test.h"

#define IMPLEMENTATIONS
#include "../../libs/lbstd.h"
#include <stdlib.h>

EXPORT_RESULT_SETUP(request_dependencies_t *dependencies) { return OK; }

EXPORT_RESULT_STRUCT_SIZE() { return sizeof(process_bti_result_t); }

EXPORT_RESULT_STRUCT_DIAGNOSTICS(process_bti_result_t *result) {
  plog(INFO, "process_bti module called!");

  const double epsilon = 0.5;
  result->measured_access_time =
      (double)result->measured_access_time_tot / result->iters;

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

  return KO;
}
