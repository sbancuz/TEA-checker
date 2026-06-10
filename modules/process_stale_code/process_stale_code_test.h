#ifndef _process_stale_code_TEST
#define _process_stale_code_TEST

#include "test_name.h.out"

#include "../tester.h"
#include "types.h"

typedef struct {
  usize iters;

  double uncached_access_time;
  double overhead;

  usize measured_access_time_tot;
  double measured_access_time;

  usize normal_call_time_tot;
  double normal_call_time;

  usize machine_clear_call_time_tot;
  double machine_clear_call_time;
} process_stale_code_result_t;

#endif
