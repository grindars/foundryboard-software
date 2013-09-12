#ifndef __STDARG__H__
#define __STDARG__H__

#define va_list __builtin_va_list
#define va_start(l, a) __builtin_va_start(l, a)
#define va_end(l) __builtin_va_end(l)
#define va_arg(l, t) __builtin_va_arg(l, t)
#define va_copy(d, s) __builtin_va_copy(d, s)

#endif
