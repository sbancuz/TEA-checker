#ifndef _rob_TEST
#define _rob_TEST

#include "test_name.h.out"

#include "../tester.h"
#include "types.h"

typedef struct {
  usize iterations;
  usize raw_readings_nop[512];
  double readings_nop[512] TO_PLOT("line", "plot1") AXIS(Y, "latency", 512)
      AXIS(X, "instruction_count", 512) VALUES("rob_size");

  usize raw_readings_xor[512];
  double readings_xor[512];
  /* TO_PLOT("line", "plot1") AXIS(Y, "latency", 512) */
  /*       AXIS(X, "instruction_count", 512) VALUES("register_file_size"); */

  usize rob_size;
  usize register_file_size;
} rob_result_t;

#endif
