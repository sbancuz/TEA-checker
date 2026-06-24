#ifndef _user_bti_TEST
#define _user_bti_TEST

#include "test_name.h.out"

#include "../tester.h"
#include "types.h"

typedef struct {
  usize iters;
  double overhead;
  double uncached_access_time;
  usize measured_access_time_tot;
  double measured_access_time;
} user_bti_result_t;

#endif
