#ifndef _ASM_IMMINTR
#define _ASM_IMMINTR

#include "macro_tricks.h"
#include "types.h"

static inline void serialise(void);
static inline u64 get_cycle(void);
static inline u64 get_cycle_ser(void);
static inline void load(volatile void *);
static inline void cache_line_flush(volatile void *);
static inline void memory_barrier(void);
static inline void read_memory_barrier(void);
static inline void write_memory_barrier(void);

#define add5(x) apply5(add, x)
#define add25(x) apply25(add, x)
#define add125(x) apply125(add, x)
#define add625(x) apply625(add, x)
#ifdef TARGET_X86_64
#define __asm_ret 0xc3

static inline void serialise(void) {
#ifdef __SERIALIZE__
  __asm__ __volatile__("serialize" : : : "memory");
#else
  register unsigned int eax = 0, ebx, ecx, edx;
  __asm__ __volatile__("cpuid"
                       : "+a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                       :
                       : "memory");
#endif
}

#define add(x) __asm__ __volatile__("add $1, %%eax" : "=a"(x))

u32 aux;
/* #define get_cycle() __builtin_ia32_rdtscp(&aux) */
#define get_cycle_ser() __builtin_ia32_rdtscp(&aux)

static inline u64 get_cycle(void) {
  /* u32 lo, hi; */
  /* __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi) : : "memory"); */
  /* return ((u64)hi << 32) | lo; */
  return __builtin_ia32_rdtsc();
}

static inline void load(volatile void *addr) {
  __asm__ __volatile__("mov (%0), %%rax" : : "r"(addr) : "rax", "memory");
}

static inline void cache_line_flush(volatile void *addr) {
  __asm__ __volatile__("clflush (%0)" : : "r"(addr) : "memory");
}

static inline void memory_barrier(void) {
  __asm__ __volatile__("mfence" : : : "memory");
}

static inline void read_memory_barrier(void) {
  __asm__ __volatile__("lfence" : : : "memory");
}

static inline void write_memory_barrier(void) {
  __asm__ __volatile__("sfence" : : : "memory");
}

#ifndef nop
#define nop()                                                                  \
  {                                                                            \
    __asm__ __volatile__("nop" ::: "memory");                                  \
  }
#endif
#define nop_nomem __asm__ __volatile__("nop")

#elif TARGET_RISCV

#define add(x) __asm__ __volatile__("addi %0, %0, 1" : "+r"(x))

#define __asm_ret 0x00008067
static inline void serialise(void) {
  __asm__ volatile("fence iorw, iorw" ::: "memory"); // serializes memory & I/O
  __asm__ volatile("fence.i" ::: "memory"); // flush instruction pipeline
}

#if defined(__riscv_zicbom)
static inline void cache_line_flush(volatile void *ptr) {
  uintptr_t p = (uintptr_t)ptr;
  __asm__ volatile("cbo.flush %0" ::"r"(p) : "memory");
}
#else
// Credits:
// https://github.com/Hardware-Forge/s4v/blob/main/Processors/BOOM/Spectrev2/inc/cache.h
#define L1_SETS 64
#define L1_SET_BITS 6 // note: this is log2Ceil(L1_SETS)
#define L1_WAYS 8     // note: this looks like there are 8 ways
#define L1_BLOCK_SZ_BYTES 64
#define L1_BLOCK_BITS 6 // note: this is log2Ceil(L1_BLOCK_SZ_BYTES)
#define L1_SZ_BYTES (L1_SETS * L1_WAYS * L1_BLOCK_SZ_BYTES)
#define FULL_MASK 0xFFFFFFFFFFFFFFFF
#define OFF_MASK (~(FULL_MASK << L1_BLOCK_BITS))
#define TAG_MASK (FULL_MASK << (L1_SET_BITS + L1_BLOCK_BITS))
#define SET_MASK (~(TAG_MASK | OFF_MASK))
u8 dummy[L1_SZ_BYTES];

static inline void cache_line_flush(volatile void *ptr) {
  volatile u8 tmp;
  usize alignedMem = ((usize)&dummy + L1_SZ_BYTES) & TAG_MASK;

  usize baseSetIndex = ((usize)ptr & SET_MASK) >> L1_BLOCK_BITS;

  for (u64 set = 0; set < L1_SETS; ++set) {
    usize setOffset = (baseSetIndex + set) << L1_BLOCK_BITS;

    for (u64 way = 0; way < L1_WAYS; ++way) {
      usize wayOffset = way << (L1_BLOCK_BITS + L1_SET_BITS);
      tmp = *((u8 *)(alignedMem + setOffset + wayOffset));
    }
  }
}
#endif

static inline u64 get_cycle(void) {
  u64 v;
  __asm__ __volatile__("rdcycle %0" : "=r"(v) : : "memory");
  return v;
}

static inline u64 get_cycle_ser(void) {
  u64 v;
  read_memory_barrier();
  __asm__ __volatile__("rdcycle %0" : "=r"(v) : : "memory");
  read_memory_barrier();
  return v;
}

static inline void load(volatile void *addr) {
  __asm__ __volatile__("ld x0, 0(%0)" : : "r"(addr) : "memory");
}

static inline void memory_barrier(void) {
  __asm__ __volatile__("fence iorw, iorw" : : : "memory");
}

static inline void read_memory_barrier(void) {
  __asm__ __volatile__("fence ir, ir" : : : "memory");
}

static inline void write_memory_barrier(void) {
  __asm__ __volatile__("fence ow, ow" : : : "memory");
}

#ifndef nop
#define nop()                                                                  \
  {                                                                            \
    __asm__ __volatile__("nop" ::: "memory");                                  \
  }
#endif
#define nop_nomem __asm__ __volatile__("nop")

#endif

#endif // _ASM_IMMINTR
