#include "locks_test.h"

#include "../tester.h"

#include "immintr.h"
#include "mem.h"
#include "types.h"

AS_RESULT(locks_result_t);

void func(request_dependencies_t *args) {

#ifdef __GCC_HAVE_SYNC_COMPARE_AND_SWAP_4
  RESULT->support = true;
#else
  RESULT->support = false;
#endif
}

#include "../tester.c"
