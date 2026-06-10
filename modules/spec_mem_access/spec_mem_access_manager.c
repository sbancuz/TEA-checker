#include "../commands.h"
#include "spec_mem_access_test.h"

#define IMPLEMENTATIONS
#include "../../libs/lbstd.h"

static ssize window = 0;
#define RANGE 10
static ssize range = RANGE;
static ssize step = 1 << RANGE;
static bool first_run = true;

bool make_window_tester(ssize w) {
  str instructions = {0};
  str_append_cstr(&instructions, "volatile int __x_ = 0; \n");
  str_append_cstr(&instructions, "#define measure_window(x) \\\n");
  for (int i = 0; i < window; i++) {
    str_append_cstr(&instructions, "add(__x_);\\\n");
    /* str_append_cstr(&instructions, */
    /* "__asm__ __volatile__(\"add $1, %%eax\" : \"=a\"(x));\\\n"); */
  }
  str_append_cstr(&instructions, "\n");
  da_append(&instructions, '\0');

  const char *inst_file = "instructions.h.out";
  if (!write_to_file(inst_file, instructions.items)) {
    plog(ERR, "Faild to write %s: %s", inst_file, strerror(errno));
    return false;
  }
  da_free(&instructions);

  return true;
}

EXPORT_RESULT_SETUP(request_dependencies_t *dependencies) {
  make_window_tester(window);
  return OK;
}

EXPORT_RESULT_STRUCT_SIZE() { return sizeof(spec_mem_access_result_t); }

EXPORT_RESULT_STRUCT_DIAGNOSTICS(spec_mem_access_result_t *result) {
  /* plog(INFO, "%d", result->cache_line_time_access_tot); */
  /* plog(INFO, "%d", result->cache_line_access_count); */
  /* plog(INFO, "%f", result->overhead); */
  double cache_line_access_time = (double)result->cache_line_time_access_tot /
                                      result->cache_line_access_count -
                                  result->overhead;
  plog(INFO, "cache line access time: %f", cache_line_access_time);
  plog(INFO, "uncached access time: %f", result->uncached_access_time);

  bool present =
      !are_close(cache_line_access_time, result->uncached_access_time, 50.f) &&
      cache_line_access_time < result->uncached_access_time;

  if (first_run) {
    if (present) {
      first_run = false;

      window = step;
      step /= 2;

      result->window_full =
          result->uncached_access_time - cache_line_access_time;
      make_window_tester(window);
      return RETRY;
    }
  } else {
    if (!present) {
      window -= step;
    } else {
      window += step;
    }

    plog(INFO, "window %d step %d present %d", window, step, present);
    step /= 2;
    if (range > 0) {
      range -= 1;
      make_window_tester(window);
      return RETRY;
    }
  }

  if (window == 0) {
    plog(INFO, "Speculative memory access not present. Spectre like "
               "vulnerabilities are impossible");

    return KO;
  }

  plog(INFO, "Speculative memory access present, window = %d", window);
  result->window_size = window;

  return OK;
}
