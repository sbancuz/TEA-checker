#include "tester.h"

#include <stdio.h>

#include "commands.h"
#include "unistd.h"

#define _MEM_IMPLEMENTATION
#define _THREAD_IMPLEMENTATION
#include "delim.h"
#include "mem.h"
#include "thread.h"
#include "types.h"

extern result_t *RESULT;
extern void *args[];

// Wrapper for func to match thread signature
static void *func_thread(void *arg) {
  (void)arg;
  func(args);

  scheduler_running = false;
  return NULL;
}

// Single hart bootstrap
void boot_hart0_main(void) {
  __init_alloc();
  RESULT = calloc(1, sizeof(*RESULT));

  thread_t main_thread;
  thread_create(&main_thread, func_thread, NULL, 0);

  // Start hart scheduler loop
  hart_scheduler_loop();

  // Wait for main thread to finish
  thread_join(main_thread, NULL);

  // Print result
  printf(DELIM);
  write(STDOUT_FILENO, RESULT, sizeof(*RESULT));
  printf(DELIM);

  __deinit_alloc();
}

int main(void) {
  // On single-core, NUM_HARTS == 1
  // On multi-core, hart 0 will call boot_hart0_main(), others boot_other_hart()
  usize h = mhartid();

  if (h == 0) {
    boot_hart0_main();
  } else {
    hart_scheduler_loop();
  }

  return 0;
}
