#ifndef _ASM
#define _ASM

#include "types.h"

extern void serialise(void);
extern u64 get_cycle(void);
extern void load(volatile void *);
extern void cache_line_flush(volatile void *);
extern void memory_barrier(void);
extern void read_memory_barrier(void);
extern void write_memory_barrier(void);

#endif // _ASM
