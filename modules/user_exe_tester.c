#include "tester.h"
#ifdef _GNU_SOURCE
#undef _GNU_SOURCE
#endif

#include <errno.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "commands.h"

#define _MEM_IMPLEMENTATION
#define _THREAD_IMPLEMENTATION
#include "mem.h"
/* #include "thread.h" */
#include "delim.h"
#include "types.h"

bool run_test(request_dependencies_t *args, cpuid_t cpu) {
  long num_cpus = sysconf(_SC_NPROCESSORS_ONLN);
  if (cpu >= num_cpus) {
    fprintf(stderr, "Invalid CPU: %d (system has %ld CPUs)\n", cpu, num_cpus);
    return false;
  }

  /* cpu_set_t set; */
  /*  */
  /* CPU_ZERO(&set); */
  /* CPU_SET(cpu, &set); */
  /*  */
  /* if (sched_setaffinity(0, sizeof(set), &set) == -1) { */
  /*   perror("sched_setaffinity"); */
  /*   return false; */
  /* } */

  func(args);

  return true;
}

extern result_t *RESULT;

int main(int argc, char *argv[]) {
  FILE *f = fopen(argv[1], "rb");
  if (!f) {
    printf("Could not open data.in file\n");
  }

  struct run_function_request req = {};

  fread(&req.args_count, 1, 8, f);
  fread(&req.cpu, 1, 8, f);

  if (req.args_count > 0) {
    req.args_sizes = calloc(req.args_count, sizeof(size_t));
    req.args = calloc(req.args_count, sizeof(unsigned char *));
    for (int i = 0; i < req.args_count; i++) {
      fread(&req.args_sizes[i], 1, 8, f);
      req.args[i] = malloc(req.args_sizes[i]);
      fread(req.args[i], 1, req.args_sizes[i], f);
    }
  }

  RESULT = calloc(1, sizeof(*RESULT));
  run_test(req.args, req.cpu);

  printf(DELIM);
  fflush(stdout);
  write(STDOUT_FILENO, RESULT, sizeof(*RESULT));
  /* write(STDERR_FILENO, RESULT, sizeof(*RESULT)); */
  printf(DELIM);
  fflush(stdout);

  return 0;
}
