#include "../commands.h"
#include "pipeline_test.h"

#define IMPLEMENTATIONS
#include "../../libs/lbstd.h"

EXPORT_RESULT_SETUP(request_dependencies_t *dependencies) { return OK; }

EXPORT_RESULT_STRUCT_SIZE() { return sizeof(pipeline_result_t); }

EXPORT_RESULT_STRUCT_DIAGNOSTICS(pipeline_result_t *result) {
  result->no_interleaved =
      (double)result->no_interleaved_tot / result->tries - result->overhead;
  result->interleaved =
      (double)result->interleaved_tot / (result->tries) - result->overhead;

  plog(INFO, "Multiple independent %f", result->no_interleaved);
  plog(INFO, "Single instruction %f", result->interleaved);

  if (!are_close(result->no_interleaved, result->interleaved, 20.f)) {
    plog(INFO, "Pipelining not present");
    return KO;
  }
  plog(INFO, "Pipelining present");
  return OK;
}
