#include "sgx_test.h"

#include "../tester.h"

#include "immintr.h"
#include "mem.h"
#include "types.h"

AS_RESULT(sgx_result_t);

int cpuid_c(unsigned int eax_in, unsigned int ecx_in, unsigned int *eax,
            unsigned int *ebx, unsigned int *ecx, unsigned int *edx) {
#if defined(__x86_64__) || defined(__i386__)
  unsigned int a, b, c, d;
  __asm__ volatile("cpuid"
                   : "=a"(a), "=b"(b), "=c"(c), "=d"(d)
                   : "a"(eax_in), "c"(ecx_in));
  *eax = a;
  *ebx = b;
  *ecx = c;
  *edx = d;
  /* CPUID always returns something; but callers may check max leaf first. */
  return 1;
#else
  (void)eax_in;
  (void)ecx_in;
  (void)eax;
  (void)ebx;
  (void)ecx;
  (void)edx;
  return 0;
#endif
}

void func(request_dependencies_t *args) {
#ifdef MITIGATE
  RESULT->support = SGX_NOT_SUPPORTED;
  return;
#endif
  u32 eax, ebx, ecx, edx;

  /* Check extended-feature leaf (CPUID.(EAX=7,ECX=0):EBX bit 2 = SGX) */
  if (!cpuid_c(7, 0, &eax, &ebx, &ecx, &edx)) {
    RESULT->support = SGX_NOT_SUPPORTED;
    return;
  }

  int sgx_flag = (ebx >> 2) & 1;
  if (!sgx_flag) {
    RESULT->support = SGX_NOT_SUPPORTED;
    return;
  }

  /* 2) If SGX bit set, query CPUID.(EAX=0x12,ECX=0) (SGX capabilities leaf) */
  if (!cpuid_c(0x12, 0, &eax, &ebx, &ecx, &edx)) {
    RESULT->support = SGX_SUPPORTED_NO_AVALIABLE;
    return;
  }

  RESULT->sgx1_present = eax & 0x1;
  RESULT->sgx2_present = (eax >> 1) & 0x1;

  /* CPUID.(0x12,1) reports attributes (DEBUG, MODE64BIT, provisioning, EINIT
   * token keys ...) */
  if (cpuid_c(0x12, 1, &eax, &ebx, &ecx, &edx)) {
    RESULT->support = SGX_SUPPORTED;
  } else {
    RESULT->support = SGX_SUPPORTED_NO_AVALIABLE;
  }

  /* NOTE: Even if CPUID says SGX is supported, SGX may be disabled in
   * "BIOS/UEFI or blocked by the OS. Check kernel driver (/dev/sgx* or
   * /dev/isgx) and /proc/cpuinfo for 'sgx' flag. */
}

#include "../tester.c"
