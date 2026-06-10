#include "../commands.h"
#include "kernel_space_break_test.h"

#define IMPLEMENTATIONS
#include "../../libs/lbstd.h"
#include <stdlib.h>

EXPORT_RESULT_SETUP(request_dependencies_t *dependencies) {
  return KO;
}

EXPORT_RESULT_STRUCT_SIZE() { return sizeof(kernel_space_break_result_t); }

EXPORT_RESULT_STRUCT_DIAGNOSTICS(kernel_space_break_result_t *result) {
  plog(INFO, "kernel_space_break module called!");
  return KO;
}
