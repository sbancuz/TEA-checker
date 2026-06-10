#ifndef _smt_TEST
#define _smt_TEST

#include "test_name.h.out"

#include "../tester.h"
#include "types.h"

typedef struct {
  usize iterations;

  u64 alone_thread_time_tot;
  u64 same_thread_time_tot;
  u64 same_core_time_tot;
  u64 different_core_time_tot;

  double alone_thread_time;
  double same_thread_time;
  double same_core_time;
  double different_core_time;
} smt_result_t;

#endif
