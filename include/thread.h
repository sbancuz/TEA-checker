#ifndef _THREAD
#define _THREAD

#include "types.h"
typedef struct thread_impl_t *thread_t;

bool thread_create(thread_t *restrict thread, void *(*start_routine)(void *),
                   void *restrict arg, usize cpu);

bool thread_join(thread_t thread, void **retval);

/* #define noinline __attribute__((noinline)) */

#endif // _THREAD

#ifdef _THREAD_IMPLEMENTATION
#ifdef RUNNER_KERNEL
#error TODO
#else
#ifdef RUNNER_USER

#define _GNU_SOURCE
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>

struct thread_impl_t {
  pthread_t t;
};

// Wrapper struct to pass both the user arg and CPU to the trampoline
struct thread_arg_t {
  void *(*start_routine)(void *);
  void *arg;
  usize cpu;
};

// Trampoline that sets CPU before calling the real start routine
static void *thread_trampoline(void *varg) {
  struct thread_arg_t *targ = varg;

  // Set CPU affinity for this thread
  cpu_set_t set;
  CPU_ZERO(&set);
  CPU_SET(targ->cpu, &set);
  if (sched_setaffinity(0, sizeof(set), &set) != 0) {
    perror("sched_setaffinity");
  }

  // Call the user's start routine
  void *ret = targ->start_routine(targ->arg);

  free(targ); // free the wrapper
  return ret;
}

bool thread_create(struct thread_impl_t **thread,
                   void *(*start_routine)(void *), void *arg, usize cpu) {
  if (!thread)
    return false;

  *thread = malloc(sizeof(struct thread_impl_t));
  if (!*thread)
    return false;

  // Allocate the trampoline argument
  struct thread_arg_t *targ = malloc(sizeof(struct thread_arg_t));
  if (!targ) {
    free(*thread);
    return false;
  }
  targ->start_routine = start_routine;
  targ->arg = arg;
  targ->cpu = cpu;

  return pthread_create(&(*thread)->t, NULL, thread_trampoline, targ) == 0;
}

bool thread_join(thread_t thread, void **retval) {
  if (!thread) {
    return false;
  }
  int err = pthread_join(thread->t, retval);
  free(thread);
  return err == 0;
}

#else
#ifdef RUNNER_SIMULATION
#include <stdatomic.h>

#define NUM_HARTS 4
#define MAX_THREADS 16

// ---------------------
// Thread definitions
// ---------------------
typedef enum {
  THREAD_UNUSED = 0,
  THREAD_READY,
  THREAD_RUNNING,
  THREAD_FINISHED
} thread_state_t;

typedef struct thread_impl_t {
  void *(*entry)(void *);
  void *arg;
  void *retval;
  volatile thread_state_t state;
  usize requested_hart;
  struct thread_impl_t *next;
} thread_impl_t;

typedef thread_impl_t *thread_t;

static thread_impl_t THREAD_POOL[MAX_THREADS];

typedef struct {
  thread_t head;
  thread_t tail;
  atomic_flag lock;
} thread_queue_t;

static thread_queue_t hart_queues[NUM_HARTS];

volatile bool scheduler_running = true;

// ---------------------
// Spinlock helpers
// ---------------------
static inline void lock(atomic_flag *f) {
  while (atomic_flag_test_and_set(f))
    ; // spin
}

static inline void unlock(atomic_flag *f) { atomic_flag_clear(f); }

// ---------------------
// Pool management
// ---------------------
static thread_t alloc_thread(void) {
  for (int i = 0; i < MAX_THREADS; i++) {
    thread_state_t expected = THREAD_UNUSED;
    if (__atomic_compare_exchange_n(&THREAD_POOL[i].state, &expected,
                                    THREAD_READY, 0, __ATOMIC_SEQ_CST,
                                    __ATOMIC_SEQ_CST)) {
      THREAD_POOL[i].next = NULL;
      return &THREAD_POOL[i];
    }
  }
  return NULL; // pool exhausted
}

static void free_thread(thread_t t) {
  if (t) {
    __atomic_store_n(&t->state, THREAD_UNUSED, __ATOMIC_SEQ_CST);
  }
}

static void enqueue_thread(usize hart, thread_t t) {
  t->next = NULL;
  thread_queue_t *q = &hart_queues[hart];
  lock(&q->lock);

  if (!q->tail) {
    q->head = q->tail = t;
  } else {
    q->tail->next = t;
    q->tail = t;
  }

  unlock(&q->lock);
}

static thread_t dequeue_thread(usize hart) {
  thread_queue_t *q = &hart_queues[hart];
  thread_t t = NULL;

  lock(&q->lock);

  t = q->head;
  if (t) {
    q->head = t->next;
    if (!q->head)
      q->tail = NULL;
    t->next = NULL;
  }

  unlock(&q->lock);
  return t;
}

static inline usize mhartid(void) {
  usize id;
  asm volatile("csrr %0, mhartid" : "=r"(id));
  return id;
}

// ---------------------
// Thread API
// ---------------------
bool thread_create(thread_t *thread, void *(*entry)(void *), void *arg,
                   usize hart) {
  if (!thread || !entry || hart >= NUM_HARTS)
    return false;

  thread_t t = alloc_thread();
  if (!t)
    return false;

  t->entry = entry;
  t->arg = arg;
  t->retval = NULL;
  t->requested_hart = hart;

  enqueue_thread(hart, t);
  *thread = t;
  return true;
}

void yeild(void);
bool thread_join(thread_t t, void **retval) {
  if (!t)
    return false;

  while (t->state != THREAD_FINISHED) {
    yeild();
  }

  if (retval)
    *retval = t->retval;

  free_thread(t);
  return true;
}

// ---------------------
// Fallback stealing
// ---------------------
static thread_t steal_any_thread(void) {
  usize h = mhartid();

  for (usize i = 0; i < NUM_HARTS; i++) {
    if (i == h)
      continue;

    thread_t t = dequeue_thread(i);
    if (!t)
      continue;

    thread_state_t expected = THREAD_READY;
    if (__atomic_compare_exchange_n(&t->state, &expected, THREAD_RUNNING, 0,
                                    __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
      return t;
    } else {
      enqueue_thread(i, t);
    }
  }

  return NULL;
}

// ---------------------
// Hart scheduler loop
// ---------------------
void yeild(void) {
  usize h = mhartid();
  thread_t t = dequeue_thread(h);

  if (!t) {
    t = steal_any_thread();
  }

  if (t) {
    if (t->requested_hart == h || t->state == THREAD_RUNNING) {
      t->retval = t->entry(t->arg);
      __atomic_store_n(&t->state, THREAD_FINISHED, __ATOMIC_SEQ_CST);
    } else {
      enqueue_thread(t->requested_hart, t);
    }
  }
}

void hart_scheduler_loop(void) {
  while (scheduler_running) {
    yeild();
  }
}

#else
#error Unsupported target
#endif
#endif
#endif

#endif
