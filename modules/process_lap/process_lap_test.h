#ifndef _process_lap_TEST
#define _process_lap_TEST

#include "test_name.h.out"

#include "../tester.h"
#include "types.h"

typedef struct {
  usize iters;
  double overhead;
  double uncached_access_time;
  usize measured_access_time_tot;
  double measured_access_time;
} process_lap_result_t;

#endif
