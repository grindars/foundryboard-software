#ifndef __STDLIB__H__
#define __STDLIB__H__

#include "stddef.h"
#include "stdint.h"

typedef void (*heap_lock_cb_t)(void *arg);
typedef void (*heap_unlock_cb_t)(void *arg);

typedef struct heap_locking_callbacks {
    heap_lock_cb_t lock;
    heap_unlock_cb_t unlock;
} heap_locking_callbacks_t;

LIBC_BEGIN_PROTOTYPES

#pragma GCC visibility push(default)

void heap_seed(void *block, size_t size);

void heap_initialize_locking(const heap_locking_callbacks_t *callbacks, void *arg);

void *malloc(size_t size);
void *calloc(size_t count, size_t size);
void free(void *ptr);
void *realloc(void *block, size_t bytes);

long strtol(const char *restrict str, char **restrict endptr, int base);

#pragma GCC visibility pop

LIBC_END_PROTOTYPES

#endif
