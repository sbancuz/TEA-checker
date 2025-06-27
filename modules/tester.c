#include "tester.h"

#ifdef _MEM_IMPLEMENTATION
#error Cant define _MEM_IMPLEMENTATION in the tester module
#endif

#ifdef RUNNER_KERNEL
#include "kernel_tester.c"
#elif RUNNER_USER
#include "user_tester.c"
#elif RUNNER_SIMULATION
#include "simulation_tester.c"
#endif
