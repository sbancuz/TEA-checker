#include "rsb_test.h"

#include "../tester.h"

#include "immintr.h"
#include "mem.h"
#include "types.h"

AS_RESULT(rsb_result_t);

volatile int sink = 0;
volatile int sink2 = 0;
#define ITERATIONS 5000

no_inline void target_function(void) {
  volatile int sum = 0;
  for (int i = 0; i < 100; i++)
    sum += i;
  sink += sum;
}

no_inline void rsb_poison(int depth) {
  if (depth <= 0)
    return;
  rsb_poison(depth - 1);
}

no_inline void rsb_safe_target(void) { read_memory_barrier(); }

no_inline void rsb_stuff(int depth) {
  if (depth <= 0) {
    rsb_safe_target();
    return;
  }

  rsb_stuff(depth - 1);
}

usize measure_call(void (*func)(void)) {
  unsigned int aux;

#ifdef MITIGATE
  // Keep the RSB always full
  rsb_stuff(READINGS);
  serialise();
#endif
  read_memory_barrier();
  volatile usize start = get_cycle_ser();
  func();

  serialise();
  volatile usize end = get_cycle_ser();
  read_memory_barrier();
  return end - start;
}

void func(request_dependencies_t *args) {
  RESULT->iterations = ITERATIONS;

  for (int i = 0; i < READINGS; i++) {
    for (int j = 0; j < ITERATIONS; j++) {
      RESULT->raw_normal_readings[i] += measure_call(target_function);
    }

    for (int j = 0; j < ITERATIONS; j++) {
      rsb_poison(i);
      RESULT->raw_poison_readings[i] += measure_call(target_function);
    }
  }
}

#include "../tester.c"
