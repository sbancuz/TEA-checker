#include "../commands.h"
#include "stl_forward_test.h"

#define IMPLEMENTATIONS
#include "../../libs/lbstd.h"

EXPORT_RESULT_SETUP(request_dependencies_t *dependencies) { return OK; }

EXPORT_RESULT_STRUCT_SIZE() { return sizeof(stl_forward_result_t); }

EXPORT_RESULT_STRUCT_DIAGNOSTICS(stl_forward_result_t *result) {
  double eps = 0.01f;
  int count = 0;

  for (int o1 = 0; o1 < OFFSETS; o1++) {
    for (int o2 = 0; o2 < OFFSETS; o2++) {
      result->timings[o1 * OFFSETS + o2] =
          ((double)result->readings[o1 * OFFSETS + o2] / 10 /
           result->iterations);

      // Skip first iteration to have a running average
      if (o1 == 0 && o2 == 0)
        continue;

      // If they are all close then this would mean that if stl is present, we
      // can't create a window to exploit it
      if (!are_close(result->timings[o1 * OFFSETS + o2],
                     result->timings[(o1 * OFFSETS + o2) - 1], 20.)) {
        /* plog(INFO, "%f %f", result->timings[o1 * OFFSETS + o2], */
        /*      result->timings[(o1 * OFFSETS + o2) - 1]); */
        count += 1;
      }
    }
  }

  // Even with mitigations a couple of outliers can still exist, a passing test
  // has around 300 `count` so 7%
  if ((double)count > OFFSETS * OFFSETS * 0.01f) {
    plog(INFO, "HAS STL");
    return OK;
  }

  plog(INFO, "NO STL");
  return KO;
}
