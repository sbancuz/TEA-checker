#include "../commands.h"
#include "tlb_test.h"

#define IMPLEMENTATIONS
#include "../../libs/lbstd.h"
#include <stdlib.h>

bool are_close_abs(double a, double b, double threshold) {
  return fabs(a - b) <= threshold;
}

int detect_jumps_sliding_abs(const double *data, int n, int window,
                             double threshold) {
  if (window * 2 >= n) {
    printf("Window too large.\n");
    return -1;
  }

  for (int i = window; i < n - window; i++) {
    double left_mean = mean(data, i - window, window);
    double right_mean = mean(data, i, window);

    if (fabs(right_mean - left_mean) > threshold) {
      return i + window / 2;
    }
  }

  return -1;
}

EXPORT_RESULT_SETUP(request_dependencies_t *dependencies) { return OK; }

EXPORT_RESULT_STRUCT_SIZE() { return sizeof(tlb_result_t); }

int detect_sudden_jumps(double *x, int n, int WINDOW_SIZE, double THRESHOLD) {
  for (int i = 0; i <= n - WINDOW_SIZE; i++) {
    double sumX = 0, sumY = 0, sumXY = 0, sumXX = 0;

    // Compute sums for linear regression in the window
    for (int j = 0; j < WINDOW_SIZE; j++) {
      double xi = j;
      double yi = x[i + j];
      sumX += xi;
      sumY += yi;
      sumXY += xi * yi;
      sumXX += xi * xi;
    }

    double denom = WINDOW_SIZE * sumXX - sumX * sumX;
    if (denom == 0)
      continue; // avoid division by zero

    double slope = (WINDOW_SIZE * sumXY - sumX * sumY) / denom;
    double intercept = (sumY - slope * sumX) / WINDOW_SIZE;

    // Check residuals for sudden jumps
    for (int j = 0; j < WINDOW_SIZE; j++) {
      double expected = slope * j + intercept;
      double residual = fabs(x[i + j] - expected);
      if (residual > THRESHOLD) {
        return i + j; // return first jump index
      }
    }
  }
  return -1; // no jump found
}

EXPORT_RESULT_STRUCT_DIAGNOSTICS(tlb_result_t *result) {
  plog(INFO, "tlb module called!");

  for (usize working_set = 1, wk = 0; working_set <= MAX_PAGES;
       working_set *= 2, wk++) {
    result->readings[wk] =
        (double)result->raw_readings[wk] / (1000 * working_set);
  }

  result->raw_readings[0] = result->raw_readings[1];
  result->readings[0] = result->readings[1];

  double *filtered = malloc(TLB_TEST_COUNT * sizeof(double));
  median_filter(TLB_TEST_COUNT, result->readings, filtered, 3);

  result->size = detect_jump_cusum(TLB_TEST_COUNT, filtered, 200.f);

  free(filtered);

  plog(INFO, "%d", result->size);
  if (result->size <= 4) {
    plog(INFO, "TLB not present");
    return KO;
  }

  plog(INFO, "Constant time page access: TLB present size %d",
       (1 << result->size));
  return OK;
}
