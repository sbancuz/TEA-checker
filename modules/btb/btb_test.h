#ifndef _btb_TEST
#define _btb_TEST

#include "test_name.h.out"

#include "../tester.h"
#include "types.h"

typedef struct {
  u64 tries;

  u64 A_after_A_tot;
  u64 A_after_B_tot;
  u64 B_after_A_tot;

  double overhead;

  double aa;
  double ab;
  double ba;

} btb_result_t;

#endif
