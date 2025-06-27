#ifndef _COMMANDS
#define _COMMANDS

enum command {
  RUN_FUNCTION,
};

typedef void (*testing_func_t)(void *);
typedef int cpuid_t;

struct run_function_request {
  void *args;
  cpuid_t cpu;
  void *ret;
};

#endif
