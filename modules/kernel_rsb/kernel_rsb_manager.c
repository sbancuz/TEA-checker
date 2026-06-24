#include "../commands.h"
#include "kernel_rsb_test.h"

#define IMPLEMENTATIONS
#include "../../libs/lbstd.h"
#include <stdlib.h>

EXPORT_RESULT_SETUP(request_dependencies_t *dependencies) { return OK; }

EXPORT_RESULT_STRUCT_SIZE() { return sizeof(kernel_rsb_result_t); }

EXPORT_RESULT_STRUCT_DIAGNOSTICS(kernel_rsb_result_t *result) {
  plog(INFO, "kernel_rsb module called!");
  double cache_line_access_time = (double)result->cache_line_time_access_tot /
                                      result->cache_line_access_count -
                                  result->overhead;
  plog(INFO, "THIS cache_line_access_time %f", cache_line_access_time);
  plog(INFO, "uncached_access_time %f", result->uncached_access_time);
  bool present =
      !are_close(cache_line_access_time, result->uncached_access_time, 20.f) &&
      cache_line_access_time < result->uncached_access_time;

  if (!present) {
    plog(INFO, "Speculative memory access not present. Spectre V1 like "
               "vulnerabilities are impossible");

    return KO;
  }
  plog(INFO, "Transient memory access present");

  return OK;
}
