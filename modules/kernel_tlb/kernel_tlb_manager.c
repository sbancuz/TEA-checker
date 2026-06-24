/*
 * kernel_tlb_manager.c — diagnostics for the kernel TLB test.
 */

#include "../commands.h"
#include "kernel_tlb_test.h"

#define IMPLEMENTATIONS
#include "../../libs/lbstd.h"

EXPORT_RESULT_SETUP(request_dependencies_t *dependencies) { return OK; }

EXPORT_RESULT_STRUCT_SIZE() { return sizeof(kernel_tlb_result_t); }

EXPORT_RESULT_STRUCT_DIAGNOSTICS(kernel_tlb_result_t *result) {
  plog(INFO, "kernel_tlb module called!");
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
    plog(INFO, "Kernel TLB stale-translation not detected");

    return KO;
  }
  plog(INFO, "Kernel TLB stale-translation detected");

  return KO;
}
