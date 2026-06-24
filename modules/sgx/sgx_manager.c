#include "../commands.h"
#include "sgx_test.h"

#define IMPLEMENTATIONS
#include "../../libs/lbstd.h"
#include <stdlib.h>

EXPORT_RESULT_SETUP(request_dependencies_t *dependencies) { return OK; }

EXPORT_RESULT_STRUCT_SIZE() { return sizeof(sgx_result_t); }

EXPORT_RESULT_STRUCT_DIAGNOSTICS(sgx_result_t *result) {
  if (result->support == SGX_SUPPORTED) {
    plog(INFO, "SGX supported");
    return OK;
  }

  plog(INFO, "SGX not supported");
  return KO;
}
