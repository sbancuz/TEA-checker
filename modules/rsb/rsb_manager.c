#include "../commands.h"
#include "rsb_test.h"

#define IMPLEMENTATIONS
#include "../../libs/lbstd.h"

EXPORT_RESULT_SETUP(request_dependencies_t *dependencies) { return OK; }

EXPORT_RESULT_STRUCT_SIZE() { return sizeof(rsb_result_t); }

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

EXPORT_RESULT_STRUCT_DIAGNOSTICS(rsb_result_t *result) {
  plog(INFO, "RSB called");
  usize max_size = READINGS;
  for (s32 i = 0; i < max_size; i++) {
    result->normal_readings[i] =
        (double)result->raw_normal_readings[i] / (double)result->iterations;

    result->poison_readings[i] =
        (double)result->raw_poison_readings[i] / result->iterations;
  }
  result->poison_readings[0] =
      (double)result->raw_poison_readings[1] / result->iterations;

  median_filter(max_size, result->poison_readings, result->filtered_readings,
                5);

  ssize rsb_size =
      detect_jump_welch(max_size, result->filtered_readings, 7, 20.f);

  plog(INFO, "%f", result->poison_readings[25]);

  if (rsb_size == -1) {
    plog(INFO, "Not found substatial jump");
    return KO;
  }

  plog(INFO, "Found rsb %d", rsb_size);
  result->return_stack_buffer_size = rsb_size;
  return OK;
}
