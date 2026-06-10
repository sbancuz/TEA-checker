#ifndef _pipeline_TEST
#define _pipeline_TEST

#include "test_name.h.out"

#include "../tester.h"
#include "types.h"

typedef struct {
  double overhead;
  usize tries;
  usize no_interleaved_tot;
  usize interleaved_tot;
  double no_interleaved;
  double interleaved;
} pipeline_result_t;

#endif
