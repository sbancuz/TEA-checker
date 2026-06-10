#define _GNU_SOURCE
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define ITER 1000000000UL

typedef struct {
  int cpu;
  double result;
} thread_arg;

void *busy_loop(void *arg) {
  thread_arg *t = (thread_arg *)arg;

  // Set affinity
  cpu_set_t set;
  CPU_ZERO(&set);
  CPU_SET(t->cpu, &set);
  sched_setaffinity(0, sizeof(set), &set);

  struct timespec start, end;
  clock_gettime(CLOCK_MONOTONIC, &start);

  double sum = 0;
  for (unsigned long i = 0; i < ITER; i++) {
    sum += i * 0.0000001;
  }

  clock_gettime(CLOCK_MONOTONIC, &end);
  double elapsed =
      (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
  t->result = elapsed;

  return NULL;
}

int main() {
  pthread_t t1, t2;
  thread_arg a1 = {0, 0};
  thread_arg a2 = {2, 0};

  printf("Running two threads WITHOUT affinity (OS chooses CPUs)...\n");
  pthread_create(&t1, NULL, busy_loop, &a1);
  /* pthread_create(&t2, NULL, busy_loop, &a2); */
  pthread_join(t1, NULL);
  /* pthread_join(t2, NULL); */
  printf("Elapsed times: Thread1 = %.3f s, Thread2 = %.3f s\n", a1.result,
         a2.result);

  printf("\nRunning two threads WITH fixed affinity (CPU0 and CPU1)...\n");
  pthread_create(&t1, NULL, busy_loop, &a1);
  pthread_create(&t2, NULL, busy_loop, &a2);
  pthread_join(t1, NULL);
  pthread_join(t2, NULL);
  printf("Elapsed times: Thread1 = %.3f s, Thread2 = %.3f s\n", a1.result,
         a2.result);

  printf("\nObservation:\n");
  printf("- If SMT is active, running both threads on the same core (OS "
         "choice) may be slower.\n");
  printf("- Setting affinity spreads them out and may reduce contention.\n");

  return 0;
}
