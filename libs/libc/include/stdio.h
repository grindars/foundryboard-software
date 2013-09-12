#ifndef __STDIO__H__
#define __STDIO__H__

#include "stddef.h"
#include "stdarg.h"

LIBC_BEGIN_PROTOTYPES

typedef void (*vcprintf_callback_t)(char ch, void *arg);

#pragma GCC visibility push(default)

int vcprintf(const char *fmt, va_list list, vcprintf_callback_t callback, void *pass);

#pragma GCC visibility pop

LIBC_END_PROTOTYPES

#endif
