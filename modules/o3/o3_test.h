#ifndef _O3_TEST
#define _O3_TEST

#include "test_name.h.out"

#include "../tester.h"
#include "types.h"

typedef struct {
  // Measured
  u64 number_of_instructions;
  u64 nodep_instruction_time_tot;
  u64 uncached_access_time_with_instruction_tot;

  // Analyzed
  double nodep_instruction_time;
  double uncached_access_time_with_instruction;

  // From dependencies
  u64 tries;
  double overhead;
  double uncached_access_time;
} o3_result_t;

#endif
