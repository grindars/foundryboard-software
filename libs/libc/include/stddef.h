#ifndef __STDDEF__H__
#define __STDDEF__H__

#if defined(__cplusplus)
#define LIBC_BEGIN_PROTOTYPES extern "C" {
#define LIBC_END_PROTOTYPES }
#define NULL 0
#else
#define LIBC_BEGIN_PROTOTYPES
#define LIBC_END_PROTOTYPES
#define NULL ((void *) 0)
#endif

#define offsetof(s,f) __builtin_offsetof(s,f)

#endif
