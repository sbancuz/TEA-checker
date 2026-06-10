#include "kernel_space_break_test.h"

#include "../tester.h"

#include "immintr.h"
#include "mem.h"
#include "types.h"

AS_RESULT(kernel_space_break_result_t);

void func(request_dependencies_t *args) {}

#include "../tester.c"
