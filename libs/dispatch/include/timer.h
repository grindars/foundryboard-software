#ifndef __TIMER__H__
#define __TIMER__H__

#include <list.h>

typedef struct TIMER TIMER;

typedef unsigned long long TIME;
typedef void (*TIMER_TIMEOUT_CALLBACK)(TIMER *Timer, TIME TimeElapsed, void *Argument);

struct TIMER {
    LIST_HEAD Head;
    TIME Timeout;
    TIME Reload;
    TIME Started;

    TIMER_TIMEOUT_CALLBACK Callback;
    void *Argument;
};

LIBC_BEGIN_PROTOTYPES

void TimerInit(void);
void TimerRTCReady(void); // Called by ClockMgmt

#pragma GCC visibility push(default)

void TimerCreateSingleShot(TIMER *Timer, TIME Timeout, TIMER_TIMEOUT_CALLBACK Callback, void *Argument);
void TimerCreatePeriodic(TIMER *Timer, TIME Timeout, TIMER_TIMEOUT_CALLBACK Callback, void *Argument);
void TimerRelease(TIMER *Timer);

TIME TimerCurrentTime(void);

#pragma GCC visibility pop

LIBC_END_PROTOTYPES

#endif
