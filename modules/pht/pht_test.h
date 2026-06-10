#ifndef _pht_TEST
#define _pht_TEST

#include "test_name.h.out"

#include "../tester.h"
#include "types.h"

typedef struct {
  u64 tries;
  u64 instruction_time_tot;

  u64 branch_taken_time_tot;
  u64 taken_counted;

  u64 branch_not_taken_time_tot;
  u64 not_taken_counted;

  u64 instruction_time_tot_long;

  u64 branch_taken_time_tot_long;
  u64 taken_counted_long;

  u64 branch_not_taken_time_tot_long;
  u64 not_taken_counted_long;

  u64 number_of_instructions;

  double overhead;
  double penalty;
} pht_result_t;

#endif
