#ifndef _COMMANDS
#define _COMMANDS
#include "../include/types.h" // TODO: Import it better

enum command {
  RUN_FUNCTION,
};

typedef void *request_dependencies_t;
typedef void request_return_t;

typedef void (*testing_func_t)(request_dependencies_t *);
typedef int cpuid_t;

struct run_function_request {
  unsigned long args_count;
  request_dependencies_t *args;
  usize *args_sizes;
  cpuid_t cpu;
  request_return_t *ret;
};

#endif
