#ifndef _rsb_TEST
#define _rsb_TEST

#include "test_name.h.out"

#include "../tester.h"
#include "types.h"

#define READINGS 512

typedef struct {
  usize iterations;

  usize raw_normal_readings[READINGS];
  double normal_readings[READINGS] TO_PLOT("line", "plot1")
      AXIS(Y, "latency", 512) AXIS(X, "cycles", 512) VALUES("normal_calls");

  usize raw_poison_readings[READINGS];
  double poison_readings[READINGS] TO_PLOT("line", "plot1")
      AXIS(Y, "latency", 512) AXIS(X, "cycles", 512) VALUES("poisoned_calls");

  double filtered_readings[READINGS] TO_PLOT("line", "plot1")
      AXIS(Y, "latency", 512) AXIS(X, "cycles", 512) VALUES("filtered_calls");

  usize return_stack_buffer_size;
} rsb_result_t;

#endif
