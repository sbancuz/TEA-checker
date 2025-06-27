#include "measure_cache.h"

#include <stdio.h>

EXPORT_RESULT_STRUCT_SIZE();

EXPORT_RESULT_STRUCT_DIAGNOSTICS(DECLARE_RESULT) {
  const double epsilon = 0.2;

  printf("overhead: %lld\nuncached: %lld\ncached: %lld\n", RESULT->overhead_tot,
         RESULT->uncached_access_time_tot, RESULT->cached_access_time_tot);

  RESULT->overhead = (double)RESULT->overhead_tot / RESULT->tries;
  RESULT->cached_access_time =
      (double)RESULT->cached_access_time_tot / RESULT->tries - RESULT->overhead;
  RESULT->uncached_access_time =
      (double)RESULT->uncached_access_time_tot / RESULT->tries -
      RESULT->overhead;

  if (RESULT->cached_access_time < RESULT->uncached_access_time * (1 - epsilon))
    return true;
  else
    return false;
}
