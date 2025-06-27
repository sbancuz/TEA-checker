#ifndef _MEASURE_CACHE
#define _MEASURE_CACHE

#include "test_name.h.out"

#include "../tester.h"
#include "types.h"

DEFINE_RESULT_STRUCT {
  u64 tries;
  u64 overhead_tot;
  u64 cached_access_time_tot;
  u64 uncached_access_time_tot;

  double overhead;
  double cached_access_time;
  double uncached_access_time;
};

#endif
