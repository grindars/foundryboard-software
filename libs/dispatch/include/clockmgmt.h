#ifndef __CLOCKMGMT__H__
#define __CLOCKMGMT__H__

#include <list.h>

#define CLOCK_NO_CONSTRAINT ((unsigned int) -1)
#define CLOCK_OUT_OF_RANGE  ((unsigned int) -1) // Returned by frequency change callback

#define HS_FREQUENCY            8000000
#define LS_FREQUENCY            32768

typedef enum CLOCKMGMT_DOMAIN {
    CLOCK_DOMAIN_PLLOUTPUT = 0,
    CLOCK_DOMAIN_SYSCLK,
    CLOCK_DOMAIN_HCLK,
    CLOCK_DOMAIN_PCLK1,
    CLOCK_DOMAIN_TIMCLK1,
    CLOCK_DOMAIN_PCLK2,
    CLOCK_DOMAIN_TIMCLK2,

    CLOCK_DOMAINS
} CLOCKMGMT_DOMAIN;

typedef struct CLOCKMGMT_CONSTRAINT {
    // for internal use only
    LIST_HEAD Head;

    CLOCKMGMT_DOMAIN Domain;

    // frequency in hertz or CLOCK_NO_CONSTRAINT
    unsigned int MinFrequency;
    unsigned int MaxFrequency;

    // may be called from interrupt context
    void (*FrequencyChanged)(unsigned int NewFrequency, void *Arg);
    void *Arg;
} CLOCKMGMT_CONSTRAINT;

LIBC_BEGIN_PROTOTYPES

void ClockMgmtInit(void);

#pragma GCC visibility push(default)
unsigned int ClockMgmtCurrentFrequency(CLOCKMGMT_DOMAIN Domain);
void ClockMgmtAddConstraint(CLOCKMGMT_CONSTRAINT *Constraint);
void ClockMgmtRemoveConstraint(CLOCKMGMT_CONSTRAINT *Constraint);

void ClockMgmtEnablePeripheral(void *Peripheral);
void ClockMgmtDisablePeripheral(void *Peripheral);
void ClockMgmtResetPeripheral(void *Peripheral);

#pragma GCC visibility pop

LIBC_END_PROTOTYPES

#endif
