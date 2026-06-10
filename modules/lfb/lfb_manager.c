#include "../commands.h"
#include "lfb_test.h"

#define IMPLEMENTATIONS
#include "../../libs/lbstd.h"

EXPORT_RESULT_SETUP(request_dependencies_t *dependencies) {
  usize variants = 32;
  str instructions = {0};

  str_append_cstr(&instructions,
                  tsprintf("#define LFB_TEST_VARIANTS_COUNT %d\n", variants));
  da_append(&instructions, '\0');

  const char *inst_file = "count.h.out";
  if (!write_to_file(inst_file, instructions.items)) {
    plog(ERR, "Faild to write %s: %s", inst_file, strerror(errno));
    return KO;
  }

  return OK;
}

EXPORT_RESULT_STRUCT_SIZE() { return sizeof(lfb_result_t); }

EXPORT_RESULT_STRUCT_DIAGNOSTICS(lfb_result_t *result) {
  plog(INFO, "lfb module called! %d", result->variants);
  result->raw_readings[0] = result->raw_readings[1] - 1;
  for (s32 i = 0; i < result->variants; i++) {
    result->readings[i] =
        ((double)result->raw_readings[i] / (result->iterations)) -
        result->cache_hit_time;
  }

  double *filtered = malloc(result->variants * sizeof(double));
  median_filter(result->variants, result->readings, filtered, 3);

  ssize lfb_size =
      /* detect_sudden_jumps(result->readings, result->variants, 8, 7.5f); */
      detect_jump_cusum(result->variants, result->readings, 150.f);

  plog(INFO, "LFB %d", lfb_size);

  if (lfb_size < 1) {
    return KO;
  }

  result->lfb_size = lfb_size;

  return OK;
}
