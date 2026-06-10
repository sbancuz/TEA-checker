#ifndef _lfb_TEST
#define _lfb_TEST

#if __has_include("count.h.out")
#include "count.h.out"
#define RECOMPILE 0
#else
#define LFB_TEST_VARIANTS_COUNT 99999
#define RECOMPILE 1
#endif

#include "test_name.h.out"

#include "../tester.h"
#include "types.h"

typedef struct {
  usize iterations;
  usize variants;
  usize raw_readings[LFB_TEST_VARIANTS_COUNT];
  double readings[LFB_TEST_VARIANTS_COUNT] TO_PLOT("line",
                                                   "line fill buffer size")
      AXIS(Y, "latency", LFB_TEST_VARIANTS_COUNT)
          AXIS(X, "test", LFB_TEST_VARIANTS_COUNT) VALUES("run");

  usize lfb_size;
  double cache_hit_time;
} lfb_result_t;

#endif
