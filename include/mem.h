#ifndef _MEM
#define _MEM

#include "types.h"

void *alloc(usize);

#endif // _MEM

#ifdef _MEM_IMPLEMENTATION
#ifdef RUNNER_KERNEL

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
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

#elif RUNNER_USER

#include <assert.h>

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

void *alloc(usize size) {
  void *buf = malloc(size);

  if (!buf)
    return NULL;

  da_append(&alloc_list, buf);

  return buf;
}

void __deinit_alloc(void) {
  da_foreach(void *, buf, &alloc_list) { free(buf); }
  da_free(&alloc_list);
}

#elif RUNNER_SIMULATION
#else
#error Unsupported target
#endif

#endif
