#ifndef _user_tlb_TEST
#define _user_tlb_TEST

#include "test_name.h.out"

#include "../tester.h"
#include "types.h"

typedef struct {
  double uncached_access_time;
  double overhead;

  u64 cache_line_time_access_tot;
  u64 cache_line_access_count;
  u64 tlb_capacity;
} user_tlb_result_t;

#endif
