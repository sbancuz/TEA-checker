#ifndef _sgx_TEST
#define _sgx_TEST

#include "test_name.h.out"

#include "../tester.h"
#include "types.h"

typedef enum {
  SGX_SUPPORTED = 0,
  SGX_NOT_SUPPORTED,
  SGX_SUPPORTED_NO_AVALIABLE,
} sgx_support_t;

typedef struct {
  sgx_support_t support;
  bool sgx1_present;
  bool sgx2_present;
} sgx_result_t;

#endif
