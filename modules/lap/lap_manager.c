#include "../commands.h"
#include "lap_test.h"

#define IMPLEMENTATIONS
#include "../../libs/lbstd.h"

EXPORT_RESULT_SETUP(request_dependencies_t *dependencies) { return OK; }

EXPORT_RESULT_STRUCT_SIZE() { return sizeof(lap_result_t); }

EXPORT_RESULT_STRUCT_DIAGNOSTICS(lap_result_t *result) {
  plog(INFO, "random: %zu -- (unrolled) %zu", result->random_time,
       result->random_time_unroll);
  plog(INFO, "fixed: %zu -- (unrolled) %zu", result->fixed_time,
       result->fixed_time_unroll);

  if (!are_close(result->random_time, result->fixed_time, 50.0f) ||
      !are_close(result->random_time_unroll, result->fixed_time_unroll,
                 50.0f)) {
    return OK;
  }

  return KO;
}
