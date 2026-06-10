#include "tester.h"

#ifdef _MEM_IMPLEMENTATION
#error Cant define _MEM_IMPLEMENTATION in the tester module
#endif

#ifdef RUNNER_KERNEL
#include "kernel_tester.c"
#elif RUNNER_USER
#ifdef EXE
#include "user_exe_tester.c"
#else
#include "user_tester.c"
#endif
#elif RUNNER_SIMULATION
#include "simulation_tester.c"
#endif
