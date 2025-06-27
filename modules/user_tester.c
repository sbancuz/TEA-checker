#include "tester.h"
#define _GNU_SOURCE
#include <errno.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "../src/commands.h"

#define _MEM_IMPLEMENTATION
#include "mem.h"
#include "types.h"

void func(void *);

extern DECLARE_RESULT;

bool run_test(void *args, cpuid_t cpu) {
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
