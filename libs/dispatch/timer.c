#include <timer.h>
#include <dispatch.h>
#include <clockmgmt.h>

#define TIMER_FREQUENCY 50
#define TIMER_QUANTA    (1000 / TIMER_FREQUENCY)

static LIST_HEAD TimerpList;
static volatile unsigned char TimerpDeferredBusy;
static volatile unsigned int TimerpTriggered;
static volatile TIME TimerpCurrent;
static DISPATCH_DEFERRED_PROC TimerpDeferred;

static void TimerpDeferredHandler(DISPATCH_DEFERRED_PROC *Proc) {
    (void) Proc;

    TimerpDeferredBusy = 0;

    while(1) {
        unsigned int Saved = DispatchDisableInterrupts(), Triggered;

        if(TimerpTriggered != 0) {
            Triggered = 1;
            TimerpTriggered--;
        } else {
            Triggered = 0;
        }

        DispatchEnableInterrupts(Saved);

        if(!Triggered)
            break;

        for(LIST_HEAD *Entry = TimerpList.Next, *Next; Entry != &TimerpList; Entry = Next) {
            Next = Entry->Next;
            TIMER *Timer = LIST_DATA(Entry, TIMER, Head);

            if(Timer->Timeout != 0) {
                if(Timer->Timeout <= TIMER_QUANTA) {
                    Timer->Timeout = 0;
                } else {
                    Timer->Timeout -= TIMER_QUANTA;
                }

                if(Timer->Timeout == 0) {
                    unsigned int Current = TimerCurrentTime(), Elapsed = Current - Timer->Started;
                    
                    Timer->Started = Current;
                    Timer->Timeout = Timer->Reload;

                    Timer->Callback(Timer, Elapsed, Timer->Argument);
                }
            }
        }
    }
}

void TimerInit(void) {
    ListInit(&TimerpList);
    TimerpDeferred.Handler = TimerpDeferredHandler;
}

void TimerRTCReady(void) {
    RTC->WPR = 0xCA;
    RTC->WPR = 0x53;

    RTC->CR &= ~RTC_CR_WUTE;
    while(!(RTC->ISR & RTC_ISR_WUTWF));
    
    RTC->CR = (RTC->CR & ~RTC_CR_WUCKSEL) | RTC_CR_WUTIE | RTC_CR_WUTE | RTC_CR_WUCKSEL_0 | RTC_CR_WUCKSEL_1;
    RTC->WUTR = LS_FREQUENCY / 2 / TIMER_FREQUENCY - 1;

    EXTI->IMR |= EXTI_IMR_MR20;
    EXTI->RTSR = EXTI_RTSR_TR20;

    NVIC_SetPriority(RTC_WKUP_IRQn, 8);
    NVIC_EnableIRQ(RTC_WKUP_IRQn);

    RTC->ISR &= ~RTC_ISR_WUTF;
}

void RTC_WKUP_IRQHandler(void) {
    if(EXTI->PR & EXTI_PR_PR20) {
        EXTI->PR = EXTI_PR_PR20;
        RTC->ISR &= ~RTC_ISR_WUTF;

        TimerpCurrent += TIMER_QUANTA;

        TimerpTriggered++;

        if(!TimerpDeferredBusy) {
            TimerpDeferredBusy = 1;
            DispatchDefer(&TimerpDeferred);
        }
    }
}

void TimerCreateSingleShot(TIMER *Timer, TIME Timeout, TIMER_TIMEOUT_CALLBACK Callback, void *Argument) {
    unsigned int Saved = DispatchEnterSystem();

    Timer->Timeout = Timeout;
    Timer->Reload = 0;
    Timer->Callback = Callback;
    Timer->Argument = Argument;
    Timer->Started = TimerCurrentTime();
    ListAppend(&TimerpList, &Timer->Head);

    DispatchLeaveSystem(Saved);
}

void TimerCreatePeriodic(TIMER *Timer, TIME Timeout, TIMER_TIMEOUT_CALLBACK Callback, void *Argument) {
    unsigned int Saved = DispatchEnterSystem();

    Timer->Timeout = Timeout;
    Timer->Reload = Timeout;
    Timer->Callback = Callback;
    Timer->Argument = Argument;
    Timer->Started = TimerCurrentTime();
    ListAppend(&TimerpList, &Timer->Head);

    DispatchLeaveSystem(Saved);
}

void TimerRelease(TIMER *Timer) {
    unsigned int Saved = DispatchEnterSystem();

    ListRemove(&Timer->Head);

    DispatchLeaveSystem(Saved);
}

TIME TimerCurrentTime(void) {
    TIME Value;

    unsigned int Saved = DispatchDisableInterrupts();

    Value = TimerpCurrent;

    DispatchEnableInterrupts(Saved);

    return Value;
}
