#ifndef __STRING__H__
#define __STRING__H__

#include "stddef.h"
#include "stdint.h"

LIBC_BEGIN_PROTOTYPES

#pragma GCC visibility push(default)

void *memcpy(void *restrict s1, const void *restrict s2, size_t n);
void *memset(void *b, int c, size_t len);
int memcmp(const void *s1, const void *s2, size_t n);

size_t strlen(const char *s);
char *strchr(const char *s, int c);
const char *strrchr(const char *s, int c);
int strcmp(const char *s1, const char *s2);

#pragma GCC visibility pop

LIBC_END_PROTOTYPES

#endif
