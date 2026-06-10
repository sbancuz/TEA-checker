/*
  Henry Wong <henry@stuffedcow.net>
  http://blog.stuffedcow.net/2014/01/x86-memory-disambiguation/

  2014-10-14
*/

#define N (80 * 1000 * 100U)
#include <malloc.h>
#include <stdio.h>

inline unsigned long long int get_cycle() {
  unsigned int lo, hi;

  __asm__ volatile(".byte 0x0f, 0x31" : "=a"(lo), "=d"(hi));
  return (long long)(((unsigned long long)hi) << 32LL) | (unsigned long long)lo;
}

// Don't inline the benchmarking code into main
void __attribute__((noinline)) tightloop4();
void __attribute__((noinline)) tightloop2();
void __attribute__((noinline)) tightloop1();
void *__attribute__((noinline)) calibration();

// Functions defined in asm blobs.
void tightloop_st_fd(void);
void tightloop_st_fd2(void);
void tightloop_st_fa(void);

volatile char arr[16];

// CAUTION: Assumes AMD64 calling convention: First two parameters passed in RSI
// and RDI.
//  Also assumes the compiler doesn't clobber rsi/rdi before to the asm()
//  statements.
void tightloop4(volatile unsigned long long *d, volatile unsigned *s) {
  unsigned j;
  for (j = 0; j < N; ++j) {
    asm("mov (%rsi),%edx\n"
        "mov %rdx,(%rdi)\n");
    asm("mov (%rsi),%edx\n"
        "mov %rdx,(%rdi)\n");
    asm("mov (%rsi),%edx\n"
        "mov %rdx,(%rdi)\n");
    asm("mov (%rsi),%edx\n"
        "mov %rdx,(%rdi)\n");
    asm("mov (%rsi),%edx\n"
        "mov %rdx,(%rdi)\n");
#if 1
    asm("mov (%rsi),%edx\n"
        "mov %rdx,(%rdi)\n");
    asm("mov (%rsi),%edx\n"
        "mov %rdx,(%rdi)\n");
    asm("mov (%rsi),%edx\n"
        "mov %rdx,(%rdi)\n");
    asm("mov (%rsi),%edx\n"
        "mov %rdx,(%rdi)\n");
    asm("mov (%rsi),%edx\n"
        "mov %rdx,(%rdi)\n");
#endif

#if 0 // For testing whether more unrolling affects results
		asm("mov (%rsi),%edx\n"
			"mov %rdx,(%rdi)\n");
		asm("mov (%rsi),%edx\n"
			"mov %rdx,(%rdi)\n");
		asm("mov (%rsi),%edx\n"
			"mov %rdx,(%rdi)\n");
		asm("mov (%rsi),%edx\n"
			"mov %rdx,(%rdi)\n");
		asm("mov (%rsi),%edx\n"
			"mov %rdx,(%rdi)\n");
		asm("mov (%rsi),%edx\n"
			"mov %rdx,(%rdi)\n");
		asm("mov (%rsi),%edx\n"
			"mov %rdx,(%rdi)\n");
		asm("mov (%rsi),%edx\n"
			"mov %rdx,(%rdi)\n");
		asm("mov (%rsi),%edx\n"
			"mov %rdx,(%rdi)\n");
		asm("mov (%rsi),%edx\n"
			"mov %rdx,(%rdi)\n");
#endif
  }
}

void tightloop2(volatile unsigned long long *d, volatile unsigned short *s) {
  unsigned j;
  for (j = 0; j < N; ++j) {
    asm("movzxw (%rsi),%rdx\n"
        "mov %rdx,(%rdi)\n");
    asm("movzxw (%rsi),%rdx\n"
        "mov %rdx,(%rdi)\n");
    asm("movzxw (%rsi),%rdx\n"
        "mov %rdx,(%rdi)\n");
    asm("movzxw (%rsi),%rdx\n"
        "mov %rdx,(%rdi)\n");
    asm("movzxw (%rsi),%rdx\n"
        "mov %rdx,(%rdi)\n");
    asm("movzxw (%rsi),%rdx\n"
        "mov %rdx,(%rdi)\n");
    asm("movzxw (%rsi),%rdx\n"
        "mov %rdx,(%rdi)\n");
    asm("movzxw (%rsi),%rdx\n"
        "mov %rdx,(%rdi)\n");
    asm("movzxw (%rsi),%rdx\n"
        "mov %rdx,(%rdi)\n");
    asm("movzxw (%rsi),%rdx\n"
        "mov %rdx,(%rdi)\n");
#if 0
		asm("movzxw (%rsi),%rdx\n"
			"mov %rdx,(%rdi)\n");
		asm("movzxw (%rsi),%rdx\n"
			"mov %rdx,(%rdi)\n");
		asm("movzxw (%rsi),%rdx\n"
			"mov %rdx,(%rdi)\n");
		asm("movzxw (%rsi),%rdx\n"
			"mov %rdx,(%rdi)\n");
		asm("movzxw (%rsi),%rdx\n"
			"mov %rdx,(%rdi)\n");
		asm("movzxw (%rsi),%rdx\n"
			"mov %rdx,(%rdi)\n");
		asm("movzxw (%rsi),%rdx\n"
			"mov %rdx,(%rdi)\n");
		asm("movzxw (%rsi),%rdx\n"
			"mov %rdx,(%rdi)\n");
		asm("movzxw (%rsi),%rdx\n"
			"mov %rdx,(%rdi)\n");
		asm("movzxw (%rsi),%rdx\n"
			"mov %rdx,(%rdi)\n");
#endif
  }
}

void tightloop1(volatile unsigned long long *d, volatile unsigned char *s) {
  unsigned j;
  for (j = 0; j < N; ++j) {
    asm("movzxb (%rsi),%rdx\n"
        "mov %rdx,(%rdi)\n");
    asm("movzxb (%rsi),%rdx\n"
        "mov %rdx,(%rdi)\n");
    asm("movzxb (%rsi),%rdx\n"
        "mov %rdx,(%rdi)\n");
    asm("movzxb (%rsi),%rdx\n"
        "mov %rdx,(%rdi)\n");
    asm("movzxb (%rsi),%rdx\n"
        "mov %rdx,(%rdi)\n");
    asm("movzxb (%rsi),%rdx\n"
        "mov %rdx,(%rdi)\n");
    asm("movzxb (%rsi),%rdx\n"
        "mov %rdx,(%rdi)\n");
    asm("movzxb (%rsi),%rdx\n"
        "mov %rdx,(%rdi)\n");
    asm("movzxb (%rsi),%rdx\n"
        "mov %rdx,(%rdi)\n");
    asm("movzxb (%rsi),%rdx\n"
        "mov %rdx,(%rdi)\n");
#if 0
		asm("movzxb (%rsi),%rdx\n"
			"mov %rdx,(%rdi)\n");
		asm("movzxb (%rsi),%rdx\n"
			"mov %rdx,(%rdi)\n");
		asm("movzxb (%rsi),%rdx\n"
			"mov %rdx,(%rdi)\n");
		asm("movzxb (%rsi),%rdx\n"
			"mov %rdx,(%rdi)\n");
		asm("movzxb (%rsi),%rdx\n"
			"mov %rdx,(%rdi)\n");
		asm("movzxb (%rsi),%rdx\n"
			"mov %rdx,(%rdi)\n");
		asm("movzxb (%rsi),%rdx\n"
			"mov %rdx,(%rdi)\n");
		asm("movzxb (%rsi),%rdx\n"
			"mov %rdx,(%rdi)\n");
		asm("movzxb (%rsi),%rdx\n"
			"mov %rdx,(%rdi)\n");
		asm("movzxb (%rsi),%rdx\n"
			"mov %rdx,(%rdi)\n");
#endif
  }
}

//        Core2   Lynnfield  SNB  IVB
// L1 load   3        4       4     4
// fwd       5        5       5     5
// fail      12      15.4    17    16
// No overlap 1       1       1     1  <-- i.e., 1/throughput

asm(".align 16\n"
    "tightloop_st_fa: \n"
    "sub $8, %rsp\n"
    "movq %rsp, %rcx\n"
    "movl $100000000,%eax\n"
    "movq %rsp,(%rsp)\n" // Store address on stack.
    ".align 16\n"
    "1:\n"

    "movq (%rcx),%rsi\n"
    "movq %rsi,(%rcx)\n" // Store fast-address, slow-data.
    "movq (%rcx),%rsi\n"
    "movq %rsi,(%rcx)\n" // Store fast-address, slow-data.
    "movq (%rcx),%rsi\n"
    "movq %rsi,(%rcx)\n" // Store fast-address, slow-data.
    "movq (%rcx),%rsi\n"
    "movq %rsi,(%rcx)\n" // Store fast-address, slow-data.
    "movq (%rcx),%rsi\n"
    "movq %rsi,(%rcx)\n" // Store fast-address, slow-data.
    "movq (%rcx),%rsi\n"
    "movq %rsi,(%rcx)\n" // Store fast-address, slow-data.
    "movq (%rcx),%rsi\n"
    "movq %rsi,(%rcx)\n" // Store fast-address, slow-data.
    "movq (%rcx),%rsi\n"
    "movq %rsi,(%rcx)\n" // Store fast-address, slow-data.
    "movq (%rcx),%rsi\n"
    "movq %rsi,(%rcx)\n" // Store fast-address, slow-data.
    "movq (%rcx),%rsi\n"
    "movq %rsi,(%rcx)\n" // Store fast-address, slow-data.
    "sub $1, %rax\n"
    "cmp $0, %rax\n" // Haswell runs significantly faster with the CMP.
    "jne 1b\n"
    "nop\n"
    "nop\n"
    "nop\n"
    "nop\n"
    "nop\n"
    "nop\n"
    "nop\n"
    "nop\n"
    "nop\n"
    "add $8, %rsp\n"
    "retq\n"
    "nop\n"
    "nop\n"
    "nop\n"
    "nop\n"
    "nop\n"
    "nop\n"
    "nop\n"
    "nop\n"
    "nop\n"
    "nop\n"
    "nop\n"
    "nop\n"
    "nop\n"
    "nop\n"
    "nop\n"
    "nop\n"
    "nop\n"
    "nop\n"
    "nop\n"
    "nop\n"
    "nop\n"

);

asm(".align 16\n"
    "tightloop_st_fd: \n"
    "sub $24, %rsp\n"
    "movl $15625000,%eax\n"
    "movq %rsp,8(%rsp)\n" // Store address on stack.
    "xorq %rcx, %rcx\n"
    ".align 16\n"
    "1:\n"

    "movq 8(%rsp),%rsi\n"
    "movq %rsp,8(%rcx,%rsi)\n" // Store fast-data, slow-address.
    "movq 8(%rsp),%rsi\n"
    "movq %rsp,8(%rcx,%rsi)\n" // Store fast-data, slow-address.
    "movq 8(%rsp),%rsi\n"
    "movq %rsp,8(%rcx,%rsi)\n" // Store fast-data, slow-address.
    "movq 8(%rsp),%rsi\n"
    "movq %rsp,8(%rcx,%rsi)\n" // Store fast-data, slow-address.
    "movq 8(%rsp),%rsi\n"
    "movq %rsp,8(%rcx,%rsi)\n" // Store fast-data, slow-address.
    "movq 8(%rsp),%rsi\n"
    "movq %rsp,8(%rcx,%rsi)\n" // Store fast-data, slow-address.
    "movq 8(%rsp),%rsi\n"
    "movq %rsp,8(%rcx,%rsi)\n" // Store fast-data, slow-address.
    "movq 8(%rsp),%rsi\n"
    "movq %rsp,8(%rcx,%rsi)\n" // Store fast-data, slow-address.
    "movq 8(%rsp),%rsi\n"
    "movq %rsp,8(%rcx,%rsi)\n" // Store fast-data, slow-address.
    "movq 8(%rsp),%rsi\n"
    "movq %rsp,8(%rcx,%rsi)\n" // Store fast-data, slow-address.

    "movq 8(%rsp),%rsi\n"
    "movq %rsp,8(%rcx,%rsi)\n" // Store fast-data, slow-address.
    "movq 8(%rsp),%rsi\n"
    "movq %rsp,8(%rcx,%rsi)\n" // Store fast-data, slow-address.
    "movq 8(%rsp),%rsi\n"
    "movq %rsp,8(%rcx,%rsi)\n" // Store fast-data, slow-address.
    "movq 8(%rsp),%rsi\n"
    "movq %rsp,8(%rcx,%rsi)\n" // Store fast-data, slow-address.
    "movq 8(%rsp),%rsi\n"
    "movq %rsp,8(%rcx,%rsi)\n" // Store fast-data, slow-address.
    "movq 8(%rsp),%rsi\n"
    "movq %rsp,8(%rcx,%rsi)\n" // Store fast-data, slow-address.
    "movq 8(%rsp),%rsi\n"
    "movq %rsp,8(%rcx,%rsi)\n" // Store fast-data, slow-address.
    "movq 8(%rsp),%rsi\n"
    "movq %rsp,8(%rcx,%rsi)\n" // Store fast-data, slow-address.
    "movq 8(%rsp),%rsi\n"
    "movq %rsp,8(%rcx,%rsi)\n" // Store fast-data, slow-address.
    "movq 8(%rsp),%rsi\n"
    "movq %rsp,8(%rcx,%rsi)\n" // Store fast-data, slow-address.

    "movq 8(%rsp),%rsi\n"
    "movq %rsp,8(%rcx,%rsi)\n" // Store fast-data, slow-address.
    "movq 8(%rsp),%rsi\n"
    "movq %rsp,8(%rcx,%rsi)\n" // Store fast-data, slow-address.
    "movq 8(%rsp),%rsi\n"
    "movq %rsp,8(%rcx,%rsi)\n" // Store fast-data, slow-address.
    "movq 8(%rsp),%rsi\n"
    "movq %rsp,8(%rcx,%rsi)\n" // Store fast-data, slow-address.
    "movq 8(%rsp),%rsi\n"
    "movq %rsp,8(%rcx,%rsi)\n" // Store fast-data, slow-address.
    "movq 8(%rsp),%rsi\n"
    "movq %rsp,8(%rcx,%rsi)\n" // Store fast-data, slow-address.
    "movq 8(%rsp),%rsi\n"
    "movq %rsp,8(%rcx,%rsi)\n" // Store fast-data, slow-address.
    "movq 8(%rsp),%rsi\n"
    "movq %rsp,8(%rcx,%rsi)\n" // Store fast-data, slow-address.
    "movq 8(%rsp),%rsi\n"
    "movq %rsp,8(%rcx,%rsi)\n" // Store fast-data, slow-address.
    "movq 8(%rsp),%rsi\n"
    "movq %rsp,8(%rcx,%rsi)\n" // Store fast-data, slow-address.

    "movq 8(%rsp),%rsi\n"
    "movq %rsp,8(%rcx,%rsi)\n" // Store fast-data, slow-address.
    "movq 8(%rsp),%rsi\n"
    "movq %rsp,8(%rcx,%rsi)\n" // Store fast-data, slow-address.
#if 1
    "movq 8(%rsp),%rsi\n"
    "movq %rsp,8(%rcx,%rsi)\n" // Store fast-data, slow-address.
    "movq 8(%rsp),%rsi\n"
    "movq %rsp,8(%rcx,%rsi)\n" // Store fast-data, slow-address.
    "movq 8(%rsp),%rsi\n"
    "movq %rsp,8(%rcx,%rsi)\n" // Store fast-data, slow-address.
    "movq 8(%rsp),%rsi\n"
    "movq %rsp,8(%rcx,%rsi)\n" // Store fast-data, slow-address.
    "movq 8(%rsp),%rsi\n"
    "movq %rsp,8(%rcx,%rsi)\n" // Store fast-data, slow-address.
    "movq 8(%rsp),%rsi\n"
    "movq %rsp,8(%rcx,%rsi)\n" // Store fast-data, slow-address.
    "movq 8(%rsp),%rsi\n"
    "movq %rsp,8(%rcx,%rsi)\n" // Store fast-data, slow-address.
    "movq 8(%rsp),%rsi\n"
    "movq %rsp,8(%rcx,%rsi)\n" // Store fast-data, slow-address.

    "movq 8(%rsp),%rsi\n"
    "movq %rsp,8(%rcx,%rsi)\n" // Store fast-data, slow-address.
    "movq 8(%rsp),%rsi\n"
    "movq %rsp,8(%rcx,%rsi)\n" // Store fast-data, slow-address.
    "movq 8(%rsp),%rsi\n"
    "movq %rsp,8(%rcx,%rsi)\n" // Store fast-data, slow-address.
    "movq 8(%rsp),%rsi\n"
    "movq %rsp,8(%rcx,%rsi)\n" // Store fast-data, slow-address.
    "movq 8(%rsp),%rsi\n"
    "movq %rsp,8(%rcx,%rsi)\n" // Store fast-data, slow-address.
    "movq 8(%rsp),%rsi\n"
    "movq %rsp,8(%rcx,%rsi)\n" // Store fast-data, slow-address.
    "movq 8(%rsp),%rsi\n"
    "movq %rsp,8(%rcx,%rsi)\n" // Store fast-data, slow-address.
    "movq 8(%rsp),%rsi\n"
    "movq %rsp,8(%rcx,%rsi)\n" // Store fast-data, slow-address.
    "movq 8(%rsp),%rsi\n"
    "movq %rsp,8(%rcx,%rsi)\n" // Store fast-data, slow-address.
    "movq 8(%rsp),%rsi\n"
    "movq %rsp,8(%rcx,%rsi)\n" // Store fast-data, slow-address.

    "movq 8(%rsp),%rsi\n"
    "movq %rsp,8(%rcx,%rsi)\n" // Store fast-data, slow-address.
    "movq 8(%rsp),%rsi\n"
    "movq %rsp,8(%rcx,%rsi)\n" // Store fast-data, slow-address.
    "movq 8(%rsp),%rsi\n"
    "movq %rsp,8(%rcx,%rsi)\n" // Store fast-data, slow-address.
    "movq 8(%rsp),%rsi\n"
    "movq %rsp,8(%rcx,%rsi)\n" // Store fast-data, slow-address.
    "movq 8(%rsp),%rsi\n"
    "movq %rsp,8(%rcx,%rsi)\n" // Store fast-data, slow-address.
    "movq 8(%rsp),%rsi\n"
    "movq %rsp,8(%rcx,%rsi)\n" // Store fast-data, slow-address.
    "movq 8(%rsp),%rsi\n"
    "movq %rsp,8(%rcx,%rsi)\n" // Store fast-data, slow-address.
    "movq 8(%rsp),%rsi\n"
    "movq %rsp,8(%rcx,%rsi)\n" // Store fast-data, slow-address.
    "movq 8(%rsp),%rsi\n"
    "movq %rsp,8(%rcx,%rsi)\n" // Store fast-data, slow-address.
    "movq 8(%rsp),%rsi\n"
    "movq %rsp,8(%rcx,%rsi)\n" // Store fast-data, slow-address.

    "movq 8(%rsp),%rsi\n"
    "movq %rsp,8(%rcx,%rsi)\n" // Store fast-data, slow-address.
    "movq 8(%rsp),%rsi\n"
    "movq %rsp,8(%rcx,%rsi)\n" // Store fast-data, slow-address.
    "movq 8(%rsp),%rsi\n"
    "movq %rsp,8(%rcx,%rsi)\n" // Store fast-data, slow-address.
    "movq 8(%rsp),%rsi\n"
    "movq %rsp,8(%rcx,%rsi)\n" // Store fast-data, slow-address.
#endif

    "sub $1, %rax\n"
    "jne 1b\n"
    "add $24, %rsp\n"
    "retq\n"
    "nop\n"
    "nop\n"
    "nop\n"
    "nop\n"
    "nop\n"
    "nop\n"
    "nop\n"
    "nop\n"
    "nop\n"
    "nop\n"
    "nop\n"
    "nop\n"
    "nop\n"
    "nop\n"
    "nop\n"
    "nop\n"
    "nop\n"
    "nop\n"
    "nop\n"
    "nop\n"
    "nop\n"

);

// Rotate through different store addresses to see whether load/store addresses
// have any impact.
asm(".align 16\n"
    "tightloop_st_fd2: \n"
    "push %rax\n"
    "sub $1024, %rsp\n"

    "xorq %rax, %rax\n"
    "movq %rsp, %rdi\n"
    "movq $1024, %rcx\n"
    "rep stosb\n"

    "movq $25000000, %rax\n"
    "xorq %rcx, %rcx\n"
    ".align 16\n"
    "1:\n"

    "movq       0x1f8(%rsp),%rsi\n"
    "movq %rcx, 0x0(%rsp,%rsi)\n" // Store fast-data, slow-address.
    "movq       0x0(%rsp),%rsi\n"
    "movq %rcx, 0x8(%rsp,%rsi)\n" // Store fast-data, slow-address.
    "movq       0x8(%rsp),%rsi\n"
    "movq %rcx, 0x10(%rsp,%rsi)\n" // Store fast-data, slow-address.
    "movq       0x10(%rsp),%rsi\n"
    "movq %rcx, 0x18(%rsp,%rsi)\n" // Store fast-data, slow-address.
    "movq       0x18(%rsp),%rsi\n"
    "movq %rcx, 0x20(%rsp,%rsi)\n" // Store fast-data, slow-address.
    "movq       0x20(%rsp),%rsi\n"
    "movq %rcx, 0x28(%rsp,%rsi)\n" // Store fast-data, slow-address.
    "movq       0x28(%rsp),%rsi\n"
    "movq %rcx, 0x30(%rsp,%rsi)\n" // Store fast-data, slow-address.
    "movq       0x30(%rsp),%rsi\n"
    "movq %rcx, 0x38(%rsp,%rsi)\n" // Store fast-data, slow-address.
    "movq       0x38(%rsp),%rsi\n"
    "movq %rcx, 0x40(%rsp,%rsi)\n" // Store fast-data, slow-address.
    "movq       0x40(%rsp),%rsi\n"
    "movq %rcx, 0x48(%rsp,%rsi)\n" // Store fast-data, slow-address.
    "movq       0x48(%rsp),%rsi\n"
    "movq %rcx, 0x50(%rsp,%rsi)\n" // Store fast-data, slow-address.
    "movq       0x50(%rsp),%rsi\n"
    "movq %rcx, 0x58(%rsp,%rsi)\n" // Store fast-data, slow-address.
    "movq       0x58(%rsp),%rsi\n"
    "movq %rcx, 0x60(%rsp,%rsi)\n" // Store fast-data, slow-address.
    "movq       0x60(%rsp),%rsi\n"
    "movq %rcx, 0x68(%rsp,%rsi)\n" // Store fast-data, slow-address.
    "movq       0x68(%rsp),%rsi\n"
    "movq %rcx, 0x70(%rsp,%rsi)\n" // Store fast-data, slow-address.
    "movq       0x70(%rsp),%rsi\n"
    "movq %rcx, 0x78(%rsp,%rsi)\n" // Store fast-data, slow-address.
    "movq       0x78(%rsp),%rsi\n"
    "movq %rcx, 0x80(%rsp,%rsi)\n" // Store fast-data, slow-address.
    "movq       0x80(%rsp),%rsi\n"
    "movq %rcx, 0x88(%rsp,%rsi)\n" // Store fast-data, slow-address.
    "movq       0x88(%rsp),%rsi\n"
    "movq %rcx, 0x90(%rsp,%rsi)\n" // Store fast-data, slow-address.
    "movq       0x90(%rsp),%rsi\n"
    "movq %rcx, 0x98(%rsp,%rsi)\n" // Store fast-data, slow-address.
    "movq       0x98(%rsp),%rsi\n"
    "movq %rcx, 0xa0(%rsp,%rsi)\n" // Store fast-data, slow-address.
    "movq       0xa0(%rsp),%rsi\n"
    "movq %rcx, 0xa8(%rsp,%rsi)\n" // Store fast-data, slow-address.
    "movq       0xa8(%rsp),%rsi\n"
    "movq %rcx, 0xb0(%rsp,%rsi)\n" // Store fast-data, slow-address.
    "movq       0xb0(%rsp),%rsi\n"
    "movq %rcx, 0xb8(%rsp,%rsi)\n" // Store fast-data, slow-address.
    "movq       0xb8(%rsp),%rsi\n"
    "movq %rcx, 0xc0(%rsp,%rsi)\n" // Store fast-data, slow-address.
    "movq       0xc0(%rsp),%rsi\n"
    "movq %rcx, 0xc8(%rsp,%rsi)\n" // Store fast-data, slow-address.
    "movq       0xc8(%rsp),%rsi\n"
    "movq %rcx, 0xd0(%rsp,%rsi)\n" // Store fast-data, slow-address.
    "movq       0xd0(%rsp),%rsi\n"
    "movq %rcx, 0xd8(%rsp,%rsi)\n" // Store fast-data, slow-address.
    "movq       0xd8(%rsp),%rsi\n"
    "movq %rcx, 0xe0(%rsp,%rsi)\n" // Store fast-data, slow-address.
    "movq       0xe0(%rsp),%rsi\n"
    "movq %rcx, 0xe8(%rsp,%rsi)\n" // Store fast-data, slow-address.
    "movq       0xe8(%rsp),%rsi\n"
    "movq %rcx, 0xf0(%rsp,%rsi)\n" // Store fast-data, slow-address.
    "movq       0xf0(%rsp),%rsi\n"
    "movq %rcx, 0xf8(%rsp,%rsi)\n" // Store fast-data, slow-address.
    "movq       0xf8(%rsp),%rsi\n"
    "movq %rcx, 0x100(%rsp,%rsi)\n" // Store fast-data, slow-address.
    "movq       0x100(%rsp),%rsi\n"
    "movq %rcx, 0x108(%rsp,%rsi)\n" // Store fast-data, slow-address.
    "movq       0x108(%rsp),%rsi\n"
    "movq %rcx, 0x110(%rsp,%rsi)\n" // Store fast-data, slow-address.
    "movq       0x110(%rsp),%rsi\n"
    "movq %rcx, 0x118(%rsp,%rsi)\n" // Store fast-data, slow-address.
    "movq       0x118(%rsp),%rsi\n"
    "movq %rcx, 0x120(%rsp,%rsi)\n" // Store fast-data, slow-address.
    "movq       0x120(%rsp),%rsi\n"
    "movq %rcx, 0x128(%rsp,%rsi)\n" // Store fast-data, slow-address.
    "movq       0x128(%rsp),%rsi\n"
    "movq %rcx, 0x130(%rsp,%rsi)\n" // Store fast-data, slow-address.
    "movq       0x130(%rsp),%rsi\n"
    "movq %rcx, 0x138(%rsp,%rsi)\n" // Store fast-data, slow-address.
    "movq       0x138(%rsp),%rsi\n"
    "movq %rcx, 0x140(%rsp,%rsi)\n" // Store fast-data, slow-address.
    "movq       0x140(%rsp),%rsi\n"
    "movq %rcx, 0x148(%rsp,%rsi)\n" // Store fast-data, slow-address.
    "movq       0x148(%rsp),%rsi\n"
    "movq %rcx, 0x150(%rsp,%rsi)\n" // Store fast-data, slow-address.
    "movq       0x150(%rsp),%rsi\n"
    "movq %rcx, 0x158(%rsp,%rsi)\n" // Store fast-data, slow-address.
    "movq       0x158(%rsp),%rsi\n"
    "movq %rcx, 0x160(%rsp,%rsi)\n" // Store fast-data, slow-address.
    "movq       0x160(%rsp),%rsi\n"
    "movq %rcx, 0x168(%rsp,%rsi)\n" // Store fast-data, slow-address.
    "movq       0x168(%rsp),%rsi\n"
    "movq %rcx, 0x170(%rsp,%rsi)\n" // Store fast-data, slow-address.
    "movq       0x170(%rsp),%rsi\n"
    "movq %rcx, 0x178(%rsp,%rsi)\n" // Store fast-data, slow-address.
    "movq       0x178(%rsp),%rsi\n"
    "movq %rcx, 0x180(%rsp,%rsi)\n" // Store fast-data, slow-address.
    "movq       0x180(%rsp),%rsi\n"
    "movq %rcx, 0x188(%rsp,%rsi)\n" // Store fast-data, slow-address.
    "movq       0x188(%rsp),%rsi\n"
    "movq %rcx, 0x190(%rsp,%rsi)\n" // Store fast-data, slow-address.
    "movq       0x190(%rsp),%rsi\n"
    "movq %rcx, 0x198(%rsp,%rsi)\n" // Store fast-data, slow-address.
    "movq       0x198(%rsp),%rsi\n"
    "movq %rcx, 0x1a0(%rsp,%rsi)\n" // Store fast-data, slow-address.
    "movq       0x1a0(%rsp),%rsi\n"
    "movq %rcx, 0x1a8(%rsp,%rsi)\n" // Store fast-data, slow-address.
    "movq       0x1a8(%rsp),%rsi\n"
    "movq %rcx, 0x1b0(%rsp,%rsi)\n" // Store fast-data, slow-address.
    "movq       0x1b0(%rsp),%rsi\n"
    "movq %rcx, 0x1b8(%rsp,%rsi)\n" // Store fast-data, slow-address.
    "movq       0x1b8(%rsp),%rsi\n"
    "movq %rcx, 0x1c0(%rsp,%rsi)\n" // Store fast-data, slow-address.
    "movq       0x1c0(%rsp),%rsi\n"
    "movq %rcx, 0x1c8(%rsp,%rsi)\n" // Store fast-data, slow-address.
    "movq       0x1c8(%rsp),%rsi\n"
    "movq %rcx, 0x1d0(%rsp,%rsi)\n" // Store fast-data, slow-address.
    "movq       0x1d0(%rsp),%rsi\n"
    "movq %rcx, 0x1d8(%rsp,%rsi)\n" // Store fast-data, slow-address.
    "movq       0x1d8(%rsp),%rsi\n"
    "movq %rcx, 0x1e0(%rsp,%rsi)\n" // Store fast-data, slow-address.
    "movq       0x1e0(%rsp),%rsi\n"
    "movq %rcx, 0x1e8(%rsp,%rsi)\n" // Store fast-data, slow-address.
    "movq       0x1e8(%rsp),%rsi\n"
    "movq %rcx, 0x1f0(%rsp,%rsi)\n" // Store fast-data, slow-address.
    "movq       0x1f0(%rsp),%rsi\n"
    "movq %rcx, 0x1f8(%rsp,%rsi)\n" // Store fast-data, slow-address.

    "sub $1, %rax\n"
    "jne 1b\n"
    "add $1024, %rsp\n"
    "popq %rax\n"
    "retq\n"
    "nop\n"
    "nop\n"
    "nop\n"
    "nop\n"
    "nop\n"
    "nop\n"
    "nop\n"
    "nop\n"
    "nop\n"
    "nop\n"
    "nop\n"
    "nop\n"
    "nop\n"
    "nop\n"
    "nop\n"
    "nop\n"
    "nop\n"
    "nop\n"
    "nop\n"
    "nop\n"
    "nop\n"

);

void *calibration() // A sanity check on the timer. This should give the L1 load
                    // latency.
{
  register void *p;
  void *q;
  int i;

  q = (void *)&q;
  p = q;

  for (i = 0; i < N; i++) {
    p = *(void **)p;
    p = *(void **)p;
    p = *(void **)p;
    p = *(void **)p;
    p = *(void **)p;
    p = *(void **)p;
    p = *(void **)p;
    p = *(void **)p;
    p = *(void **)p;
    p = *(void **)p;
    p = *(void **)p;
    p = *(void **)p;
    p = *(void **)p;
    p = *(void **)p;
    p = *(void **)p;
    p = *(void **)p;
    p = *(void **)p;
    p = *(void **)p;
    p = *(void **)p;
    p = *(void **)p;
  }
  return p;
}

int main(int argc, char **argv) {
  unsigned long long t;
  int o1, o2;

  t = get_cycle();
  void *p = calibration();
  t = get_cycle() - t;
  // Pass 'p' to printf to prevent optimization.
  printf("Sanity check calibration: %f. Compare L1 hit latency to GET_CYCLE "
         "frequency. Should be L1 hit latency.\n",
         (double)t / N / 20.0);

  char *a = (char *)malloc(1024);
  char *p64 =
      (char *)((unsigned long long)(a + 63) & ~0x3fULL); // 64-byte aligned.

  t = get_cycle();
  tightloop_st_fd();
  t = get_cycle() - t;
  printf("fd: %f  ", (double)t / 40000000 / 25.);
  // Dependent chain of store->load, fast data. Address doesn't change.

  t = get_cycle();
  tightloop_st_fd2();
  t = get_cycle() - t;
  printf("fd2: %f  ", (double)t / 64 / 25000000.);
  // Dependent chain of store->load, fast data. Cycles through 64 different
  // adresses to test whether store-forwarding time is address-dependent.

  t = get_cycle();
  tightloop_st_fa();
  t = get_cycle() - t;
  printf("fa: %f  ", (double)t / 40000000 / 25.);
  printf("\n");
  // Dependent chain of store->load, fast address (Load dependence speculation
  // probably unnecessary)

  // o2 = write offset, o1 = read offset, in bytes.
  for (o2 = 0; o2 < 64; o2++) {
    for (o1 = 0; o1 < 64; o1++) {
      printf("%d, %d: ", o1, o2);

      t = get_cycle();
      tightloop4((unsigned long long *)(p64 + o2), (unsigned *)(p64 + o1));
      t = get_cycle() - t;
      printf("4, %f  ", (double)t / N / 10.);

      t = get_cycle();
      tightloop2((unsigned long long *)(p64 + o2),
                 (unsigned short *)(p64 + o1));
      t = get_cycle() - t;
      printf("2, %f  ", (double)t / N / 10.);

      t = get_cycle();
      tightloop1((unsigned long long *)(p64 + o2), (unsigned char *)(p64 + o1));
      t = get_cycle() - t;
      printf("1, %f  ", (double)t / N / 10.);

      printf("\n");
    }
  }

  free(a);
  return 0;
}
