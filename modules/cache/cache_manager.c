#include "../commands.h"
#include "cache_test.h"

#define IMPLEMENTATIONS
#include "../../libs/lbstd.h"

EXPORT_RESULT_SETUP(request_dependencies_t *dep) {
  usize *tries = dep[0];
  *tries /= 100000;
  printf("%d\n", *tries);

  return OK;
}

EXPORT_RESULT_STRUCT_SIZE() { return sizeof(cache_result_t); }

EXPORT_RESULT_STRUCT_DIAGNOSTICS(cache_result_t *result) {
  const double epsilon = 0.2;

  // We give  alittle bit of leeway since this result can be very noisy
  result->overhead = ((double)result->overhead_tot / result->tries) * 0.97f;
  result->cached_access_time =
      ((double)result->cached_access_time_tot / result->tries -
       result->overhead);
  result->uncached_access_time =
      (double)result->uncached_access_time_tot / result->tries -
      result->overhead;

  plog(INFO, "tries: %zu", result->tries);
  plog(INFO, "overhead: %f", result->overhead);
  plog(INFO, "cached_access_time: %f", result->cached_access_time);
  plog(INFO, "uncached_access_time: %f", result->uncached_access_time);

  if (result->cached_access_time <
      result->uncached_access_time * (1 - epsilon)) {
    return OK;
  } else {
    return KO;
  }
}
