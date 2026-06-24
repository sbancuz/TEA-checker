
#include "smt_test.h"

#include "../tester.h"

#include "immintr.h"
#include "mem.h"
#include "thread.h"
#include "types.h"

AS_RESULT(smt_result_t);

typedef struct {
  usize result;
  usize iterations;
} thread_arg;

volatile int start_flag;

void *busy_loop(void *arg) {
  thread_arg *t = (thread_arg *)arg;

  volatile usize sum = 0;

  while (start_flag == 0)
    ;

  volatile usize start = get_cycle();
  for (unsigned long i = 0; i < t->iterations; i++) {
    sum += i;
  }

  t->result = get_cycle() - start;
  memory_barrier();
  serialise();

  return t;
}

void func(request_dependencies_t *args) {
  /* RESULT->iterations = 1000000000; */
  RESULT->iterations = 10000;

  thread_arg a1 = {.iterations = RESULT->iterations};
  thread_arg a2 = {.iterations = RESULT->iterations};

  thread_t t1 = 0;
  thread_t t2 = 0;

  thread_create(&t1, busy_loop, &a1, 0);
  serialise();
  start_flag = 1;
  thread_join(t1, 0);
  start_flag = 0;
  memory_barrier();

  RESULT->alone_thread_time_tot = a1.result;

  thread_create(&t1, busy_loop, &a1, 0);
  thread_create(&t2, busy_loop, &a2, 0);

  serialise();
  start_flag = 1;
  thread_join(t1, 0);
  thread_join(t2, 0);
  start_flag = 0;
  memory_barrier();

  RESULT->same_thread_time_tot =
      (a1.result + a2.result) >> 1; // This is just the mean

  thread_create(&t1, busy_loop, &a1, 0);
#ifdef MITIGATE
  thread_create(&t2, busy_loop, &a2, 0);
#else
  thread_create(&t2, busy_loop, &a2, 1);
#endif

  serialise();
  start_flag = 1;
  thread_join(t1, 0);
  thread_join(t2, 0);
  start_flag = 0;
  memory_barrier();

  RESULT->same_core_time_tot =
      (a1.result + a2.result) >> 1; // This is just the mean

  thread_create(&t1, busy_loop, &a1, 0);
  thread_create(&t2, busy_loop, &a2, 2);

  serialise();
  start_flag = 1;
  thread_join(t1, 0);
  thread_join(t2, 0);
  start_flag = 0;
  memory_barrier();

  RESULT->different_core_time_tot =
      (a1.result + a2.result) >> 1; // This is just the mean
}

#include "../tester.c"
