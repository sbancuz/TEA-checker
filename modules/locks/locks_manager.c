#include "../commands.h"
#include "locks_test.h"

#define IMPLEMENTATIONS
#include "../../libs/lbstd.h"
#include <stdlib.h>

EXPORT_RESULT_SETUP(request_dependencies_t *dependencies) { return OK; }

EXPORT_RESULT_STRUCT_SIZE() { return sizeof(locks_result_t); }

EXPORT_RESULT_STRUCT_DIAGNOSTICS(locks_result_t *result) {
  if (result->support)
    return OK;

  return KO;
}
