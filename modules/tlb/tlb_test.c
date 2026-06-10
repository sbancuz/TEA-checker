#include "tlb_test.h"

#include "../tester.h"

#include "immintr.h"
#include "mem.h"
#include "types.h"

AS_RESULT(tlb_result_t);

#define PAGE_SIZE 4096

void func(request_dependencies_t *args) {
  // TODO: Optimize this, uses too much RAM
  volatile char **pages = alloc(MAX_PAGES * sizeof(char *));

  for (usize num_pages = 0; num_pages < MAX_PAGES; num_pages++) {
    pages[num_pages] = alloc(PAGE_SIZE);

    // Ensure allocation
    pages[num_pages][0] = 1;
  }
  ker_open();
  for (usize working_set = 1, wk = 0; working_set < MAX_PAGES;
       working_set *= 2, wk++) {
    volatile u64 start, end, total = 0;

    for (usize i = 0; i < working_set; i++)
      (void)pages[i][0];

    for (int iter = 0; iter < 100; iter++) {
      for (usize i = 0; i < working_set; i++) {
#ifdef MITIGATE
        tlb_flush_page(pages[i]);
        tlb_flush();
#endif
        serialise();
        memory_barrier();

        start = get_cycle();
        pages[i][0]++;

        read_memory_barrier();
        end = get_cycle();
        total += (end - start);
      }
    }

    RESULT->raw_readings[wk] += total;
  }

  ker_close();
}

#include "../tester.c"
