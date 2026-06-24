#include "tester.h"
#ifdef _GNU_SOURCE
#undef _GNU_SOURCE
#endif

#define _GNU_SOURCE
#include <errno.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "commands.h"

#define _MEM_IMPLEMENTATION
#define _THREAD_IMPLEMENTATION
#include "mem.h"
#include "thread.h"
#include "types.h"

void func(request_dependencies_t *);

extern result_t *RESULT;

bool run_test(request_dependencies_t *args, cpuid_t cpu) {
  long num_cpus = sysconf(_SC_NPROCESSORS_ONLN);
  if (cpu >= num_cpus) {
    fprintf(stderr, "Invalid CPU: %d (system has %ld CPUs)\n", cpu, num_cpus);
    return false;
  }

  cpu_set_t set;

  CPU_ZERO(&set);
  CPU_SET(cpu, &set);

  if (sched_setaffinity(0, sizeof(set), &set) == -1) {
    perror("sched_setaffinity");
    return false;
  }

  func(args);

  return true;
}

long tester_run(u32 cmd, struct run_function_request *request) {
  __init_alloc();
  RESULT = request->ret;

  switch ((enum command)cmd) {
  case RUN_FUNCTION: {
    run_test(request->args, request->cpu);
  }

  default:
    break;
  }

  __deinit_alloc();

  return 0;
}
