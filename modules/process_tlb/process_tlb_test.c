/*
 * process_tlb_test.c — TLB stale-translation via ioctl (no mprotect).
 */

#include "process_tlb_test.h"

#include "../cache/cache_test.h"
#include "../tester.h"
#include "../tlb/tlb_test.h"

#include "immintr.h"
#include "mem.h"
#include "rand.h"
#include "types.h"

AS_RESULT(process_tlb_result_t);

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

  /*
   * pages[0] is the victim; pages[1..num_pages-1] are fillers.
   * Each occupies a distinct 4 KiB virtual page (guaranteed by mmap).
   */
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
    tlb_flush_page((void *)cache_line);
    serialise();
    memory_barrier();

    /* ── 1. TLB fill: fillers first, victim last (MRU). ──────────── */
    for (u64 p = 1; p < num_pages; p++) {
      pages[p][0]++;
    }
    cache_line[0]++;

    /* ── 2. Branch-predictor training with taken override. ───────── */
    s32 real_branch = take_branch[idx];
    take_branch[idx] = 1;
    for (int j = 0; j < TRAINING_LOOPS; j++) {
      if (take_branch[idx]) {
        load(cache_line);
      }
    }
    take_branch[idx] = real_branch;

    /* ── 3. Flush victim from data cache; TLB entry survives. ──────
     *
     * A cache-line flush (CLFLUSH on x86-64, dc civac on ARMv8,
     * cbo.flush on RISC-V Zicbom) evicts the line from L1–L3
     * without touching the TLB.  A transient load that completes
     * during step 6 will therefore be visible as a timing
     * difference at step 8.                                          */
    cache_line_flush((void *)cache_line);

    /* ── 4. Flush branch condition to open the speculation window. ─
     *
     * Evicting take_branch[idx] forces a cache miss when load_ptr()
     * tries to resolve the branch, giving the BP's "taken"
     * prediction time to drive the speculative load before the
     * actual value arrives.                                          */
    cache_line_flush(&take_branch[idx]);

    /* ── 5. Create the stale TLB entry (attack iterations only). ───
     *
     * pte_clear_noflush() writes a not-present PTE directly,
     * without INVLPG or sfence.vma.  The TLB retains the
     * translation cached in step 1: the physical address and
     * read-write permissions appear valid to the CPU's TLB-lookup
     * hardware even though the architectural PTE says otherwise.    */
#ifdef MITIGATE
    tlb_flush_page(cache_line);
    tlb_flush();
#endif
    if (!take_branch) {
      pte_clear_noflush((void *)cache_line);
    }
    serialise();
    memory_barrier();

    /* ── 6. Speculative load through the stale TLB entry. ──────────
     *
     * The BP predicts "taken" → speculatively issues a load from
     * victim_page.  The TLB lookup finds the stale entry and returns
     * the physical address immediately, bypassing the page-table
     * walker entirely.  The load completes transiently, filling
     * victim_page's cache line.
     *
     * Meanwhile the fetch of take_branch[idx] stalls on a cache
     * miss.  When the value (0) finally arrives the misprediction
     * is detected, architectural state is squashed, but the
     * cache-line fill from the transient load persists.             */
    if (take_branch[idx]) {
      load(cache_line);
    }

    if (!take_branch) {
      pte_restore_noflush((void *)cache_line);
    }
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
