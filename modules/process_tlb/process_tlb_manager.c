/*
 * process_tlb_manager.c — diagnostics for the TLB stale-translation test.
 */

#include "../commands.h"
#include "process_tlb_test.h"

#define IMPLEMENTATIONS
#include "../../libs/lbstd.h"

EXPORT_RESULT_SETUP(request_dependencies_t *dependencies) { return OK; }

EXPORT_RESULT_STRUCT_SIZE() { return sizeof(process_tlb_result_t); }

EXPORT_RESULT_STRUCT_DIAGNOSTICS(process_tlb_result_t *result) {
  plog(INFO, "process_tlb module called!");
  double cache_line_access_time = (double)result->cache_line_time_access_tot /
                                      result->cache_line_access_count -
                                  result->overhead;
  plog(INFO, "THIS cache_line_access_time %f", cache_line_access_time);
  plog(INFO, "uncached_access_time %f", result->uncached_access_time);
  plog(INFO, "TLB capacity: %llu pages", result->tlb_capacity);
  bool present =
      !are_close(cache_line_access_time, result->uncached_access_time, 70.f) &&
      cache_line_access_time < result->uncached_access_time;

  if (!present) {
    plog(INFO, "TLB stale-translation transient execution not detected");

    return KO;
  }
  plog(INFO, "TLB stale-translation transient execution detected");

  return OK;
}
