.intel_syntax noprefix

	.section .text

	// --------------------------------------------------------
	// --------------------------------------------------------

	.globl serialise 
	.type serialise, @function
serialise:
	serialize
	ret
	int3
	.size serialise, . - serialise

	// --------------------------------------------------------
	// --------------------------------------------------------

	.globl get_cycle 
	.type get_cycle, @function
get_cycle:
	rdtsc
	shl rdx, 32
	or rax, rdx
	ret
	int3
	.size get_cycle, . - get_cycle 

	// --------------------------------------------------------
	// --------------------------------------------------------

	.globl load
	.type load, @function
load:
	mov rax, [rdi]
	ret
	int3
	.size load, . - load 

	// --------------------------------------------------------
	// --------------------------------------------------------

	.globl cache_line_flush
	.type cache_line_flush, @function
cache_line_flush:
	mov rax, rdi        
	clflush [rax]
	ret
	int3
	.size cache_line_flush, . - cache_line_flush

	// --------------------------------------------------------
	// --------------------------------------------------------

	.globl memory_barrier
	.type memory_barrier, @function
memory_barrier:
	mfence
	ret
	int3
	.size memory_barrier, . - memory_barrier

	// --------------------------------------------------------
	// --------------------------------------------------------

	.globl read_memory_barrier
	.type read_memory_barrier, @function
read_memory_barrier:
	lfence
	ret
	int3
	.size read_memory_barrier, . - read_memory_barrier

	// --------------------------------------------------------
	// --------------------------------------------------------

	.globl write_memory_barrier
	.type write_memory_barrier, @function
write_memory_barrier:
	mfence
	ret
	int3
	.size write_memory_barrier, . - write_memory_barrier

.att_syntax prefix
.section .note.GNU-stack,"",@progbits
