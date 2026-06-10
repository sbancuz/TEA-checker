#ifndef _MEM
#define _MEM

#include "../probe/commands.h"
#include "types.h"

#define MPROT_NONE 0
#define MPROT_READ 1
#define MPROT_WRITE 2
#define MPROT_EXEC 4

#define CACHE_LINE_SZ 4096
#define CACHE_LINE_ALIGNED __attribute__((aligned(CACHE_LINE_SZ)))
#define CACHE_LINE_ALIGNED_PTR __attribute__((aligned(CACHE_LINE_SZ)))

void *alloc(usize);
bool mem_protect(void *, usize, int);
usize *get_kernel_ptr(void);
usize get_kernel_time(void);
void kernel_ptr_cache_flush(void);
void kernel_ptr_cache(void);
void ker_open(void);
void ker_close(void);
void pte_clear_noflush(volatile char *page);
void pte_restore_noflush(volatile char *page);
void tlb_flush(void);
void tlb_flush_page(volatile char *page);

#define no_inline __attribute__((__noinline__))
#define no_tailcall __attribute__((__noinline__))

#endif // _MEM

#ifdef _MEM_IMPLEMENTATION
#ifdef RUNNER_KERNEL

#include <asm-generic/barrier.h>
#include <asm/tlbflush.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/pgtable.h>
#include <linux/slab.h>

struct alloc_entry {
  void *ptr;
  struct list_head list;
};

static LIST_HEAD(alloc_list);

void __init_alloc(void) { INIT_LIST_HEAD(&alloc_list); }

void *alloc(usize size) {
  void *buf = kmalloc(size, GFP_KERNEL);

  if (!buf)
    return NULL;

  struct alloc_entry *entry;
  entry = kmalloc(sizeof(*entry), GFP_KERNEL);
  if (!entry) {
    kfree(buf);
    return NULL;
  }

  entry->ptr = buf;
  list_add(&entry->list, &alloc_list);

  return buf;
}

void __deinit_alloc(void) {
  struct alloc_entry *entry, *tmp;

  list_for_each_entry_safe(entry, tmp, &alloc_list, list) {
    kfree(entry->ptr);
    list_del(&entry->list);
    kfree(entry);
  }
}

static pte_t saved_pte;
static pte_t *saved_ptep;

static pte_t *walk_to_pte(unsigned long addr) {
  unsigned long cr3_val;
  pgd_t *pgdp;
  p4d_t *p4dp;
  pud_t *pudp;
  pmd_t *pmdp;
  pte_t *ptep;

  /* Read the physical address of the current PGD from CR3.
     Mask off PCID bits (lower 12) and any flags in upper bits. */
  asm volatile("mov %%cr3, %0" : "=r"(cr3_val));
  pgdp = (pgd_t *)__va(cr3_val & PAGE_MASK) + pgd_index(addr);

  if (pgd_none(*pgdp) || pgd_bad(*pgdp)) {
    pr_err("tlb: bad pgd for addr %lx\n", addr);
    return (void *)0;
  }

  p4dp = p4d_offset(pgdp, addr);
  if (p4d_none(*p4dp) || p4d_bad(*p4dp)) {
    pr_err("tlb: bad p4d for addr %lx\n", addr);
    return (void *)0;
  }

  pudp = pud_offset(p4dp, addr);
  if (pud_none(*pudp) || pud_bad(*pudp)) {
    pr_err("tlb: bad pud for addr %lx\n", addr);
    return (void *)0;
  }

  pmdp = pmd_offset(pudp, addr);
  if (pmd_none(*pmdp) || pmd_bad(*pmdp)) {
    pr_err("tlb: bad pmd for addr %lx\n", addr);
    return (void *)0;
  }

  ptep = pte_offset_kernel(pmdp, addr);
  if (pte_none(*ptep)) {
    pr_err("tlb: bad pte for addr %lx\n", addr);
    return (void *)0;
  }
  return ptep;
}

void pte_clear_noflush(volatile char *page) {
  unsigned long addr = (unsigned long)page & PAGE_MASK;
  pte_t *ptep = walk_to_pte(addr);

  if (!ptep) {
    pr_err("pte_clear_noflush_kernel: walk failed for %lx\n", addr);
    return;
  }
  if (!(pte_val(*ptep) & _PAGE_PRESENT)) {
    pr_err("pte_clear_noflush_kernel: already not-present for %lx\n", addr);
    return;
  }

  saved_pte = *ptep;
  saved_ptep = ptep;

  native_set_pte(ptep, __pte(pte_val(saved_pte) & ~(pteval_t)_PAGE_PRESENT));

  asm volatile("clflush (%0)" ::"r"(ptep) : "memory");
  asm volatile("mfence" ::: "memory");
}

void pte_restore_noflush(volatile char *page) {
  if (!saved_ptep) {
    pr_err("pte_restore_noflush_kernel: no saved PTE\n");
    return;
  }

  native_set_pte(saved_ptep, saved_pte);

  asm volatile("clflush (%0)" ::"r"(saved_ptep) : "memory");
  asm volatile("mfence" ::: "memory");

  saved_ptep = NULL;
}

void tlb_flush() { __flush_tlb_all(); }

void tlb_flush_page(volatile char *page) {

  unsigned long addr = (unsigned long)page;
  unsigned long cr3_val;
  pgd_t *pgdp;
  p4d_t *p4dp;
  pud_t *pudp;
  pmd_t *pmdp;
  pte_t *ptep;

  /* Read the physical address of the current PGD from CR3.
     Mask off PCID bits (lower 12) and any flags in upper bits. */
  asm volatile("mov %%cr3, %0" : "=r"(cr3_val));
  pgdp = (pgd_t *)__va(cr3_val & PAGE_MASK) + pgd_index(addr);

  if (pgd_none(*pgdp) || pgd_bad(*pgdp)) {
    pr_err("tlb: bad pgd for addr %lx\n", addr);
    return;
  }

  p4dp = p4d_offset(pgdp, addr);
  if (p4d_none(*p4dp) || p4d_bad(*p4dp)) {
    pr_err("tlb: bad p4d for addr %lx\n", addr);
    return;
  }

  pudp = pud_offset(p4dp, addr);
  if (pud_none(*pudp) || pud_bad(*pudp)) {
    pr_err("tlb: bad pud for addr %lx\n", addr);
    return;
  }

  pmdp = pmd_offset(pudp, addr);
  if (pmd_none(*pmdp) || pmd_bad(*pmdp)) {
    pr_err("tlb: bad pmd for addr %lx\n", addr);
    return;
  }

  ptep = pte_offset_kernel(pmdp, addr);
  if (pte_none(*ptep)) {
    pr_err("tlb: bad pte for addr %lx\n", addr);
    return;
  }

  asm volatile("clflush (%0)" ::"r"(pgdp) : "memory");
  asm volatile("clflush (%0)" ::"r"(p4dp) : "memory");
  asm volatile("clflush (%0)" ::"r"(pudp) : "memory");
  asm volatile("clflush (%0)" ::"r"(pmdp) : "memory");
  asm volatile("clflush (%0)" ::"r"(ptep) : "memory");
  asm volatile("mfence" ::: "memory");
  asm volatile("invlpg (%0)" ::"r"(page) : "memory");
}

bool mem_protect(void *ptr, usize len, int prot) { return true; }

volatile usize *____ptr = NULL;

void ker_open() {};
void ker_close() {};

usize *get_kernel_ptr(void) { return ____ptr; }
usize get_kernel_time(void) {
  ktime_t start, end;
  start = __builtin_ia32_rdtsc();
  load(____ptr);

  rmb();
  end = __builtin_ia32_rdtsc();
  return end - start;
}

void kernel_ptr_cache_flush(void) { cache_line_flush(____ptr); }

void kernel_ptr_cache(void) {
  load(____ptr);
  load(____ptr);
  load(____ptr);
  load(____ptr);
  load(____ptr);
  load(____ptr);
}

#else
#ifdef RUNNER_USER

#include <assert.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#define da(T)                                                                  \
  struct {                                                                     \
    T *items;                                                                  \
    size_t count;                                                              \
    size_t capacity;                                                           \
  }

// --- from nob.h
#ifndef DA_INIT_CAP
#define DA_INIT_CAP 256
#endif

#define da_reserve(da, expected_capacity)                                      \
  do {                                                                         \
    if ((expected_capacity) > (da)->capacity) {                                \
      if ((da)->capacity == 0) {                                               \
        (da)->capacity = DA_INIT_CAP;                                          \
      }                                                                        \
      while ((expected_capacity) > (da)->capacity) {                           \
        (da)->capacity *= 2;                                                   \
      }                                                                        \
      (da)->items =                                                            \
          realloc((da)->items, (da)->capacity * sizeof(*(da)->items));         \
      assert((da)->items != NULL && "ERROR: Out of memory");                   \
    }                                                                          \
  } while (0)

// Append an item to a dynamic array
#define da_append(da, item)                                                    \
  do {                                                                         \
    da_reserve((da), (da)->count + 1);                                         \
    (da)->items[(da)->count++] = (item);                                       \
  } while (0)

#define da_foreach(Type, it, da)                                               \
  for (Type *it = (da)->items; it < (da)->items + (da)->count; ++it)

#define da_free(da) free((da)->items)

typedef da(void *) list_t;
static list_t alloc_list = {0};

void __init_alloc(void) { return; }

#define CACHE_LINE_SZ 4096
bool mem_protect(void *ptr, usize len, int prot) {
  return mprotect(ptr, len, prot);
}

void *alloc(usize size) {
  usize true_size =
      ((size + CACHE_LINE_SZ - 1) / CACHE_LINE_SZ) * CACHE_LINE_SZ;
  void *buf;

  if (posix_memalign(&buf, CACHE_LINE_SZ, true_size) != 0)
    return NULL;

  da_append(&alloc_list, buf);
  return buf;
}

void __deinit_alloc(void) {
  da_foreach(void *, buf, &alloc_list) { free(*buf); }
  da_free(&alloc_list);
}

int fd_kernel;
#include <stdio.h>
void ker_open() {
  fd_kernel = open("/dev/probe_device", O_RDWR);
  printf("%d\n", fd_kernel);
  if (fd_kernel < 0) {
    printf("Failed to open device");
    return;
  }
}

void ker_close() { close(fd_kernel); }

usize *get_kernel_ptr(void) {
  struct probe_request req = {0};
  usize kptr = 0;
  usize time = 0;
  req.ret = &kptr;
  req.access_time = &time;

  int ret = ioctl(fd_kernel, PROBE_GET, &req);
  if (ret < 0) {
    perror("Failed to open ioclt");
    return NULL;
  }

  return (usize *)kptr;
}

usize get_kernel_time(void) {
  struct probe_request req = {0};
  usize kptr = 0;
  usize time = 0;
  req.ret = &kptr;
  req.access_time = &time;

  int ret = ioctl(fd_kernel, PROBE_GET, &req);
  if (ret < 0) {
    perror("Failed to open ioclt");
    return 0;
  }

  return time;
}

void kernel_ptr_cache(void) {
  struct probe_request req = {0};
  int ret = ioctl(fd_kernel, PROBE_CACHE, &req);
  if (ret < 0) {
    perror("Failed to open ioclt");
    return;
  }
}

void kernel_ptr_cache_flush(void) {
  int ret = ioctl(fd_kernel, PROBE_UNCACHE, NULL);
  if (ret < 0) {
    perror("Failed to open ioclt");
    return;
  }
}

/* static jmp_buf longjmp_buf; */
/*  */
/* void unblock_signal(int signum __attribute__((__unused__))) { */
/*   sigset_t sigs; */
/*   sigemptyset(&sigs); */
/*   sigaddset(&sigs, signum); */
/*   sigprocmask(SIG_UNBLOCK, &sigs, NULL); */
/* } */
/*  */
/* void segfault_handler_callback(int signum) { */
/*   (void)signum; */
/*   unblock_signal(SIGSEGV); */
/*   longjmp(longjmp_buf, 1); */
/* } */
/*  */
/* void setup_signal_handler() { signal(SIGSEGV, segfault_handler_callback); }
 */

void pte_clear_noflush(volatile char *page) {
  struct probe_request req = {0};
  req.ret = (usize *)page;

  int ret = ioctl(fd_kernel, PROBE_TLB_REMOVE_PTE, &req);
  if (ret < 0) {
    perror("Failed to open ioclt");
  }
}

void pte_restore_noflush(volatile char *page) {
  struct probe_request req = {0};
  req.ret = (usize *)page;

  int ret = ioctl(fd_kernel, PROBE_TLB_RESTORE_PTE, &req);
  if (ret < 0) {
    perror("Failed to open ioclt");
  }
}

void tlb_flush_page(volatile char *page) {
  struct probe_request req = {0};
  req.ret = (usize *)page;

  int ret = ioctl(fd_kernel, PROBE_TLB_FLUSH_PAGE, &req);
  if (ret < 0) {
    perror("Failed to open ioclt");
  }
}

void tlb_flush() {
  int ret = ioctl(fd_kernel, PROBE_TLB_FLUSH, NULL);
  if (ret < 0) {
    perror("Failed to open ioclt");
  }
}

#else
#ifdef RUNNER_SIMULATION

#define CACHE_LINE_SZ 4096
#define HEAP_SIZE (CACHE_LINE_SZ * 128)
/* #define HEAP_SIZE (CACHE_LINE_SZ * 1) */
#define HEAP_ALIGN CACHE_LINE_SZ

// Simple static heap
static u8 heap[HEAP_SIZE];
static usize heap_offset = 0;

// Align a value up to nearest multiple of HEAP_ALIGN
static inline usize align_up(usize size) {
  return (size + HEAP_ALIGN - 1) & ~(HEAP_ALIGN - 1);
}

void *alloc(usize size) {
  size = align_up(size);
  if (heap_offset + size > HEAP_SIZE) {
    // Out of memory
    return NULL;
  }
  void *ptr = &heap[heap_offset];
  heap_offset += size;
  return ptr;
}

void *calloc(usize nmemb, usize size) {
  usize total = nmemb * size;
  void *ptr = alloc(total);
  if (!ptr)
    return NULL;
  for (usize i = 0; i < total; i++)
    ((u8 *)ptr)[i] = 0;
  return ptr;
}

void __init_alloc(void) {}
void __deinit_alloc(void) {}

#else
#error Unsupported target
#endif
#endif
#endif

#endif
