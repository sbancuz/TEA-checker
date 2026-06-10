#include "../commands.h"
#include "user_space_break_test.h"

#define IMPLEMENTATIONS
#include "../../libs/lbstd.h"
#include <stdlib.h>

EXPORT_RESULT_SETUP(request_dependencies_t *dependencies) {
  return KO;
}

EXPORT_RESULT_STRUCT_SIZE() { return sizeof(user_space_break_result_t); }

EXPORT_RESULT_STRUCT_DIAGNOSTICS(user_space_break_result_t *result) {
  plog(INFO, "user_space_break module called!");
  return KO;
}
