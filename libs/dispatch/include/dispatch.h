#ifndef __DISPATCH__H__
#define __DISPATCH__H__

#include <stm32l1xx.h>
#include <list.h>

typedef struct DISPATCH_DEFERRED_PROC {
    LIST_HEAD Head;

    void (*Handler)(struct DISPATCH_DEFERRED_PROC *Proc);
} DISPATCH_DEFERRED_PROC;

#define DISPATCH_PENDSV_PRIORITY    15

LIBC_BEGIN_PROTOTYPES

void DispatchInit(void);

#pragma GCC visibility push(default)

static inline unsigned int DispatchDisableInterrupts(void) {
    unsigned int Disabled = __get_PRIMASK();
    __set_PRIMASK(1);

    return Disabled;
}

static inline void DispatchEnableInterrupts(unsigned int Disabled) {
    __set_PRIMASK(Disabled);
}

static inline unsigned int DispatchEnterSystem(void) {
    unsigned int Saved = __get_BASEPRI();

    __set_BASEPRI(DISPATCH_PENDSV_PRIORITY);

    return Saved;
}

static inline void DispatchLeaveSystem(unsigned int Saved) {
    __set_BASEPRI(Saved);
}

void DispatchDefer(DISPATCH_DEFERRED_PROC *Proc);

static inline void DispatchIdle(void) {
    __WFI();
}

#pragma GCC visibility pop

LIBC_END_PROTOTYPES

#endif
