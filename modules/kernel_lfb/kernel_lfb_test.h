#ifndef _kernel_lfb_TEST
#define _kernel_lfb_TEST

#include "test_name.h.out"

#include "../tester.h"
#include "types.h"

typedef struct {
  double uncached_access_time;
  double overhead;

  u64 cache_line_time_access_tot;
  u64 cache_line_access_count;
} kernel_lfb_result_t;

#endif
