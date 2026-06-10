#include "rob_test.h"

#include "../tester.h"

#include "immintr.h"
#include "mem.h"
#include "types.h"

#include "instructions.h.out"
AS_RESULT(rob_result_t);

void func(request_dependencies_t *args) {
  void *ptr1 = alloc(4096 * 1024);
  void *ptr2 = alloc(4096 * 1024);

  RESULT->iterations = *(u64 *)args[0];

  /* run_battery_x; */
  run_battery_i;
  /* run_battery; */
}

#include "../tester.c"
