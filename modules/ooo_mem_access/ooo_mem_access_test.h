#ifndef _ooo_mem_access_TEST
#define _ooo_mem_access_TEST

#include "test_name.h.out"

#include "../tester.h"
#include "types.h"

typedef struct {
  double uncached_access_time;
  double overhead;

  u64 cache_line_time_access_tot;
  u64 cache_line_access_count;

  double window_full;
  usize window_size;
} ooo_mem_access_result_t;

#endif
