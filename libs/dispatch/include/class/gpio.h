#ifndef __CLASS__GPIO__H__
#define __CLASS__GPIO__H__

#include <io.h>

#define GPIO_CLAIM      IO_CLASS_REQUEST(IO_CLASS_GPIO, 0)
#define GPIO_RELEASE    IO_CLASS_REQUEST(IO_CLASS_GPIO, 1)
#define GPIO_ASSERT     IO_CLASS_REQUEST(IO_CLASS_GPIO, 2)
#define GPIO_DEASSERT   IO_CLASS_REQUEST(IO_CLASS_GPIO, 3)
#define GPIO_READ       IO_CLASS_REQUEST(IO_CLASS_GPIO, 4)
#define GPIO_WRITE      IO_CLASS_REQUEST(IO_CLASS_GPIO, 5)
#define GPIO_SET_INPUT  IO_CLASS_REQUEST(IO_CLASS_GPIO, 6)
#define GPIO_SET_OUTPUT IO_CLASS_REQUEST(IO_CLASS_GPIO, 7)

typedef struct GPIO_SIMPLE_REQUEST {
    unsigned int Lines;
} GPIO_SIMPLE_REQUEST;

typedef struct GPIO_WRITE_REQUEST {
    unsigned int Lines;
    unsigned int Values;
} GPIO_WRITE_REQUEST;

LIBC_BEGIN_PROTOTYPES

#pragma GCC visibility push(default)

static inline IOSTATUS GpioClaim(HANDLE Handle, unsigned int Lines) {
    return IoExecute(Handle, GPIO_CLAIM, &Lines, sizeof(Lines), NULL);
}

static inline IOSTATUS GpioRelease(HANDLE Handle, unsigned int Lines) {
    return IoExecute(Handle, GPIO_RELEASE, &Lines, sizeof(Lines), NULL);
}

static inline IOSTATUS GpioAssert(HANDLE Handle, unsigned int Lines) {
    return IoExecute(Handle, GPIO_ASSERT, &Lines, sizeof(Lines), NULL);
}

static inline IOSTATUS GpioDeassert(HANDLE Handle, unsigned int Lines) {
    return IoExecute(Handle, GPIO_DEASSERT, &Lines, sizeof(Lines), NULL);
}

static inline IOSTATUS GpioRead(HANDLE Handle, unsigned int *Lines) {
    return IoExecute(Handle, GPIO_READ, Lines, sizeof(*Lines), NULL);
}

static inline IOSTATUS GpioWrite(HANDLE Handle, unsigned int Lines, unsigned int Values) {
    GPIO_WRITE_REQUEST Request = { .Lines = Lines, .Values = Values };

    return IoExecute(Handle, GPIO_WRITE, &Request, sizeof(Request), NULL);
}

static inline IOSTATUS GpioSetInput(HANDLE Handle, unsigned int Lines) {
    return IoExecute(Handle, GPIO_SET_INPUT, &Lines, sizeof(Lines), NULL);
}

static inline IOSTATUS GpioSetOutput(HANDLE Handle, unsigned int Lines) {
    return IoExecute(Handle, GPIO_SET_OUTPUT, &Lines, sizeof(Lines), NULL);
}

#pragma GCC visibility pop

LIBC_END_PROTOTYPES

#endif

