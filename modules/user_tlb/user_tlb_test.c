/*
 * user_tlb_test.c — TLB stale-translation via mprotect.
 */

#include "user_tlb_test.h"

#include "../cache/cache_test.h"
#include "../tester.h"
#include "../tlb/tlb_test.h"

#include "immintr.h"
#include "mem.h"
#include "rand.h"
#include "types.h"

AS_RESULT(user_tlb_result_t);

s32 *take_branch CACHE_LINE_ALIGNED;
s32 idx;

#define TRAINING_LOOPS 1000

void func(request_dependencies_t *args) {
  cache_result_t *cache_r = args[1];
  tlb_result_t *tlb_r = args[2];
  ker_open();

  RESULT->overhead = cache_r->overhead;
  RESULT->uncached_access_time = cache_r->uncached_access_time;
  RESULT->tlb_capacity = tlb_r->size > 0 ? (1ULL << (tlb_r->size)) : 0;

  u64 tlb_entries = tlb_r->size > 0 ? (1ULL << (tlb_r->size)) : 64;
  u64 num_pages = tlb_entries;
  volatile u64 start, end, total = 0;

  volatile u8 **pages = alloc(num_pages * sizeof(u8 *));
  for (u64 i = 0; i < num_pages; i++) {
    pages[i] = alloc(CACHE_LINE_SZ);
  }

  take_branch = alloc(cache_r->tries * sizeof(s32));

  for (int i = 0; i < cache_r->tries; i++) {
    take_branch[i] = 1;
    if (i % 32 == 0) {
      u32 bit;
      do {
        bit = get_rand() % 2;
      } while (bit == 1);
      take_branch[i] = 0;
    }
    RESULT->cache_line_access_count += !take_branch[i];
  }

  u64 sum = 0;
  volatile u8 *cache_line = (volatile u8 *)alloc(CACHE_LINE_SZ);

  for (idx = 0; idx < cache_r->tries; idx++) {
    mem_protect(cache_line, CACHE_LINE_SZ, MPROT_READ | MPROT_WRITE);
    tlb_flush_page((void *)cache_line);
    serialise();
    memory_barrier();

    /* TLB fill: fillers first, victim last (MRU). */
    for (u64 p = 1; p < num_pages; p++) {
      pages[p][0]++;
    }
    cache_line[0]++;

    /* Branch-predictor training with taken override. */
    s32 real_branch = take_branch[idx];
    take_branch[idx] = 1;
    for (int j = 0; j < TRAINING_LOOPS; j++) {
      if (take_branch[idx]) {
        load(cache_line);
      }
    }
    take_branch[idx] = real_branch;

    /* Flush victim from data cache; TLB entry survives. */
    cache_line_flush((void *)cache_line);

    /* Flush branch condition to open the speculation window. */
    cache_line_flush(&take_branch[idx]);

    if (!real_branch) {
      mem_protect(cache_line, CACHE_LINE_SZ, MPROT_NONE);
    }
#ifdef MITIGATE
    tlb_flush_page(cache_line);
    tlb_flush();
#endif
    if (!real_branch) {
      pte_clear_noflush((void *)cache_line);
    }
    serialise();
    memory_barrier();

    /* Speculative load through the stale TLB entry. */
    if (take_branch[idx]) {
      load(cache_line);
    }

    if (!real_branch) {
      pte_restore_noflush((void *)cache_line);
    }

    mem_protect(cache_line, CACHE_LINE_SZ, MPROT_READ | MPROT_WRITE);
    serialise();
    memory_barrier();

    start = get_cycle();
    load((volatile void *)cache_line);
    serialise();
    read_memory_barrier();
    end = get_cycle();

    sum += (end - start) * !real_branch;
  }

  RESULT->cache_line_time_access_tot = sum;
  ker_close();
}

#include "../tester.c"
