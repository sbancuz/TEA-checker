#include "../commands.h"
#include "smt_test.h"

#define IMPLEMENTATIONS
#include "../../libs/lbstd.h"

EXPORT_RESULT_SETUP(request_dependencies_t *dependencies) { return OK; }

EXPORT_RESULT_STRUCT_SIZE() { return sizeof(smt_result_t); }

EXPORT_RESULT_STRUCT_DIAGNOSTICS(smt_result_t *result) {
  result->alone_thread_time =
      (double)result->alone_thread_time_tot / result->iterations;
  result->same_thread_time =
      (double)result->same_thread_time_tot / result->iterations;
  result->same_core_time =
      (double)result->same_core_time_tot / result->iterations;
  result->different_core_time =
      (double)result->different_core_time_tot / result->iterations;

  plog(INFO, "alone %f", result->alone_thread_time);
  plog(INFO, "same t %f", result->same_thread_time);
  plog(INFO, "same c %f", result->same_core_time);
  plog(INFO, "diff c %f", result->different_core_time);

  if (!(are_close(result->alone_thread_time, result->different_core_time,
                  10.f))) {

    plog(WARN, "There is only one core?");
  }

  if ((are_close(result->same_thread_time, result->same_core_time, 20.f))) {

    plog(INFO, "SMT not found");
    return KO;
  }

  plog(INFO, "SMT found");
  return OK;
}
