#ifndef _kernel_rsb_TEST
#define _kernel_rsb_TEST

#include "test_name.h.out"

#include "../tester.h"
#include "types.h"

typedef struct {
  double uncached_access_time;
  double overhead;

  u64 cache_line_time_access_tot;
  u64 cache_line_access_count;
} kernel_rsb_result_t;

#endif
