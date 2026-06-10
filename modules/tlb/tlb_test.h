#ifndef _tlb_TEST
#define _tlb_TEST

#include "test_name.h.out"

#include "../tester.h"
#include "types.h"

#define TLB_TEST_COUNT 16
#define MAX_PAGES (1ULL << TLB_TEST_COUNT)

typedef struct {
  usize raw_readings[TLB_TEST_COUNT];
  ssize size;
  double readings[TLB_TEST_COUNT] TO_PLOT("line", "tlb timings")
      AXIS(Y, "latency", TLB_TEST_COUNT)
          AXIS(X, "tlb_size_(log_scale)", TLB_TEST_COUNT) VALUES("run");
} tlb_result_t;

#endif
