#include <stm32l1xx.h>
#include <string.h>
#include <clockmgmt.h>
#include <dispatch.h>
#include <timer.h>

#define MIN_HCLK_FREQUENCY      4000000
#define MAX_HCLK_FREQUENCY     32000000
#define MAX_PLL_VCO_FREQUENCY  96000000

typedef struct CLOCKMGMTP_PREPARED_CONSTRAINT {
    unsigned int MinFrequency;
    unsigned int MaxFrequency;
} CLOCKMGMTP_PREPARED_CONSTRAINT;

enum {
    FLASH_ACCESS_32ONLY = 0,
    FLASH_ACCESS_64ONEWS,
    FLASH_ACCESS_64ZEROWS
};

typedef struct CLOCKMGMTP_CLOCK_PARAMETERS {
    signed char PllMul;
    signed char PllDiv;
    unsigned char AhbDiv;
    unsigned char Apb1Div;
    unsigned char Apb2Div;
    unsigned char VoltageRange;
    unsigned char FlashAccess;
} CLOCKMGMTP_CLOCK_PARAMETERS;

LIST_HEAD ClockMgmtpConstraints;
static CLOCKMGMTP_CLOCK_PARAMETERS ClockMgmtpActiveParameters;

static const unsigned char ClockMgmtpPllMultiplers[] = { 3, 4, 6, 8, 12, 16, 24, 32, 48 };
static const unsigned char ClockMgmtpPllDivisors[]   = { 2, 3, 4 };

static void ClockMgmtpApplyConstraint(CLOCKMGMTP_PREPARED_CONSTRAINT *Constraint,
                                      unsigned int MinFrequency,
                                      unsigned int MaxFrequency) {

    if(MinFrequency != CLOCK_NO_CONSTRAINT &&
       (Constraint->MinFrequency == CLOCK_NO_CONSTRAINT || Constraint->MinFrequency < MinFrequency))
        Constraint->MinFrequency = MinFrequency;

    if(MaxFrequency != CLOCK_NO_CONSTRAINT &&
       (Constraint->MaxFrequency == CLOCK_NO_CONSTRAINT || Constraint->MaxFrequency > MaxFrequency))
        Constraint->MaxFrequency = MaxFrequency;
}

static void ClockMgmtpPropagateUp(const CLOCKMGMTP_PREPARED_CONSTRAINT *FromDomain,
                                  CLOCKMGMTP_PREPARED_CONSTRAINT *ToDomain,
                                  unsigned int MinDivisor,
                                  unsigned int MaxDivisor) {

    unsigned int MinSourceFrequency = FromDomain->MinFrequency;
    unsigned int MaxSourceFrequency = FromDomain->MaxFrequency;

    if(MinSourceFrequency != CLOCK_NO_CONSTRAINT)
        MinSourceFrequency *= MaxDivisor;

    if(MaxSourceFrequency != CLOCK_NO_CONSTRAINT)
        MaxSourceFrequency *= MinDivisor;

    ClockMgmtpApplyConstraint(ToDomain, MinSourceFrequency, MaxSourceFrequency);
}

static void ClockMgmtpPropagateDown(CLOCKMGMTP_PREPARED_CONSTRAINT *ToDomain,
                                    unsigned int FromDomain,
                                    unsigned int MinDivisor,
                                    unsigned int MaxDivisor) {

    ClockMgmtpApplyConstraint(ToDomain, FromDomain / MaxDivisor, FromDomain / MinDivisor);
}

static unsigned int ClockMgmtpCalculateAPBDivisor(const CLOCKMGMTP_PREPARED_CONSTRAINT *ApbDomain,
                                                  const CLOCKMGMTP_PREPARED_CONSTRAINT *TimerDomain,
                                                  unsigned int SourceFrequency,
                                                  unsigned int *OutBusFrequency,
                                                  unsigned int *OutTimerFrequency) {
    for(int DivisorIndex = 3; DivisorIndex >= 0; DivisorIndex--) {        
        unsigned int BusFrequency, TimerFrequency;
        if(DivisorIndex == 0) {
            BusFrequency = SourceFrequency;
            TimerFrequency = SourceFrequency;
        } else {
            BusFrequency = SourceFrequency >> DivisorIndex;
            TimerFrequency = SourceFrequency >> (DivisorIndex - 1);
        }

        if(BusFrequency > ApbDomain->MaxFrequency || TimerFrequency > TimerDomain->MaxFrequency)
            break;
        else if(BusFrequency >= ApbDomain->MinFrequency && TimerFrequency >= TimerDomain->MinFrequency) {
            *OutBusFrequency = BusFrequency;
            *OutTimerFrequency = TimerFrequency;

            return DivisorIndex;
        } 
    } 

    *OutTimerFrequency = SourceFrequency;
    *OutBusFrequency = SourceFrequency;

    return 0;
}

void ClockMgmtpApply(const CLOCKMGMTP_CLOCK_PARAMETERS *Parameters) {
    // If we are in voltage range 3, disable 64 bit access
    
    if((PWR->CR & PWR_CR_VOS) == PWR_CR_VOS) {
        FLASH->ACR |= FLASH_ACR_LATENCY;
        FLASH->ACR &= ~FLASH_ACR_PRFTEN;
        FLASH->ACR &= ~FLASH_ACR_ACC64;
    }

    // Switch to HSE
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_HSE;
    while((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSE);

    // Start PLL shutdown
    RCC->CR &= ~RCC_CR_PLLON;

    // Scale core voltage
    while(PWR->CSR & PWR_CSR_VOSF);
    PWR->CR = (PWR->CR & ~PWR_CR_VOS) | (Parameters->VoltageRange << 11);
    while(PWR->CSR & PWR_CSR_VOSF);

    // Apply divisors
    RCC->CFGR = (RCC->CFGR & ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2)) |
        (Parameters->AhbDiv << 4) |
        (Parameters->Apb1Div << 8) |
        (Parameters->Apb2Div << 11);
    
    // Configure flash interface
    switch(Parameters->FlashAccess) {
    case FLASH_ACCESS_32ONLY:
        FLASH->ACR &= ~(FLASH_ACR_PRFTEN | FLASH_ACR_ACC64 | FLASH_ACR_LATENCY);

        break;

    case FLASH_ACCESS_64ONEWS:
        FLASH->ACR |= FLASH_ACR_ACC64;
        FLASH->ACR |= FLASH_ACR_PRFTEN;
        FLASH->ACR |= FLASH_ACR_LATENCY;

        break;

    case FLASH_ACCESS_64ZEROWS:
        FLASH->ACR |= FLASH_ACR_ACC64;
        FLASH->ACR |= FLASH_ACR_PRFTEN;
        FLASH->ACR &= ~FLASH_ACR_LATENCY;
        
        break;
    }

    if(Parameters->PllDiv != -1 && Parameters->PllMul != -1) {
        while(RCC->CR & RCC_CR_PLLRDY);

        // Apply PLL parameters
        RCC->CFGR = (RCC->CFGR & ~(RCC_CFGR_PLLMUL | RCC_CFGR_PLLDIV)) |
                    RCC_CFGR_PLLSRC_HSE |
                    (Parameters->PllMul << 18) |
                    (Parameters->PllDiv << 22);

        // Start PLL
        RCC->CR |= RCC_CR_PLLON;
        while(!(RCC->CR & RCC_CR_PLLRDY));

        // Switch to PLL
        RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;
        while((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);
    }            
}

static void ClockMgmtpEvaluateConstraints(int ColdStart) {
    CLOCKMGMTP_PREPARED_CONSTRAINT Constraints[CLOCK_DOMAINS];
    unsigned int SelectedFrequencies[CLOCK_DOMAINS];

    CLOCKMGMTP_CLOCK_PARAMETERS Parameters = {
        .PllMul = -1,
        .PllDiv = -1,
        .AhbDiv = 0
    };

    for(unsigned int Domain = 0; Domain < CLOCK_DOMAINS; Domain++) {
        Constraints[Domain].MinFrequency = CLOCK_NO_CONSTRAINT;
        Constraints[Domain].MaxFrequency = CLOCK_NO_CONSTRAINT;
    }

    for(LIST_HEAD *Entry = ClockMgmtpConstraints.Next; Entry != &ClockMgmtpConstraints; Entry = Entry->Next) {
        CLOCKMGMT_CONSTRAINT *Constraint = LIST_DATA(Entry, CLOCKMGMT_CONSTRAINT, Head);

        ClockMgmtpApplyConstraint(&Constraints[Constraint->Domain], Constraint->MinFrequency, Constraint->MaxFrequency);
    }

    ClockMgmtpApplyConstraint(&Constraints[CLOCK_DOMAIN_HCLK], MIN_HCLK_FREQUENCY, MAX_HCLK_FREQUENCY);

    ClockMgmtpPropagateUp(&Constraints[CLOCK_DOMAIN_TIMCLK2], &Constraints[CLOCK_DOMAIN_HCLK],   1, 8);
    ClockMgmtpPropagateUp(&Constraints[CLOCK_DOMAIN_PCLK2],   &Constraints[CLOCK_DOMAIN_HCLK],   1, 16);
    ClockMgmtpPropagateUp(&Constraints[CLOCK_DOMAIN_TIMCLK1], &Constraints[CLOCK_DOMAIN_HCLK],   1, 8);
    ClockMgmtpPropagateUp(&Constraints[CLOCK_DOMAIN_PCLK1],   &Constraints[CLOCK_DOMAIN_HCLK],   1, 16);
    ClockMgmtpPropagateUp(&Constraints[CLOCK_DOMAIN_HCLK],    &Constraints[CLOCK_DOMAIN_SYSCLK], 1, 512);

    SelectedFrequencies[CLOCK_DOMAIN_PLLOUTPUT] = 0;
    SelectedFrequencies[CLOCK_DOMAIN_SYSCLK]    = HS_FREQUENCY;

    if(Constraints[CLOCK_DOMAIN_SYSCLK].MinFrequency > HS_FREQUENCY ||
       Constraints[CLOCK_DOMAIN_PLLOUTPUT].MinFrequency != CLOCK_NO_CONSTRAINT) {
        // Calculate PLL multipler and divisor

        ClockMgmtpApplyConstraint(&Constraints[CLOCK_DOMAIN_PLLOUTPUT], HS_FREQUENCY * 3, MAX_PLL_VCO_FREQUENCY);
        ClockMgmtpPropagateUp(&Constraints[CLOCK_DOMAIN_SYSCLK], &Constraints[CLOCK_DOMAIN_PLLOUTPUT], 2, 4);

        for(unsigned int MultiplerIndex = 0;
            MultiplerIndex < sizeof(ClockMgmtpPllMultiplers) / sizeof(unsigned char);
            MultiplerIndex++) {

            unsigned int Frequency = HS_FREQUENCY * ClockMgmtpPllMultiplers[MultiplerIndex];
            if(Frequency > Constraints[CLOCK_DOMAIN_PLLOUTPUT].MaxFrequency)
                break;
            else if(Frequency >= Constraints[CLOCK_DOMAIN_PLLOUTPUT].MinFrequency) {
                SelectedFrequencies[CLOCK_DOMAIN_PLLOUTPUT] = Frequency;            
                Parameters.PllMul = MultiplerIndex;
                break;
            }   
        }

        for(int DivisorIndex = sizeof(ClockMgmtpPllDivisors) / sizeof(unsigned char) - 1; DivisorIndex >= 0; DivisorIndex--) {        
            unsigned int Frequency = SelectedFrequencies[CLOCK_DOMAIN_PLLOUTPUT] / ClockMgmtpPllDivisors[DivisorIndex];

            if(Frequency > Constraints[CLOCK_DOMAIN_SYSCLK].MaxFrequency)
                break;
            else if(Frequency >= Constraints[CLOCK_DOMAIN_SYSCLK].MinFrequency) {
                SelectedFrequencies[CLOCK_DOMAIN_SYSCLK] = Frequency;            
                Parameters.PllDiv = DivisorIndex;
                break;
            } 
        }  
    }

    ClockMgmtpPropagateDown(&Constraints[CLOCK_DOMAIN_HCLK], SelectedFrequencies[CLOCK_DOMAIN_SYSCLK], 1, 512);
    
    // Calculate AHB divisor
    for(int DivisorIndex = 8; DivisorIndex >= 0; DivisorIndex--) {        
        unsigned int Frequency = SelectedFrequencies[CLOCK_DOMAIN_SYSCLK] >> DivisorIndex;

        if(Frequency > Constraints[CLOCK_DOMAIN_HCLK].MaxFrequency)
            break;
        else if(Frequency >= Constraints[CLOCK_DOMAIN_HCLK].MinFrequency) {
            SelectedFrequencies[CLOCK_DOMAIN_HCLK] = Frequency;  

            if(DivisorIndex == 0)
                Parameters.AhbDiv = DivisorIndex;
            else
                Parameters.AhbDiv = (1 << 3) | (DivisorIndex - 1);

            break;
        } 
    }  

    ClockMgmtpPropagateDown(&Constraints[CLOCK_DOMAIN_TIMCLK2], SelectedFrequencies[CLOCK_DOMAIN_HCLK], 1, 8);
    ClockMgmtpPropagateDown(&Constraints[CLOCK_DOMAIN_PCLK2],   SelectedFrequencies[CLOCK_DOMAIN_HCLK], 1, 16);
    ClockMgmtpPropagateDown(&Constraints[CLOCK_DOMAIN_TIMCLK1], SelectedFrequencies[CLOCK_DOMAIN_HCLK], 1, 8);
    ClockMgmtpPropagateDown(&Constraints[CLOCK_DOMAIN_PCLK1],   SelectedFrequencies[CLOCK_DOMAIN_HCLK], 1, 16);

    Parameters.Apb1Div = ClockMgmtpCalculateAPBDivisor(&Constraints[CLOCK_DOMAIN_PCLK1],
                                                       &Constraints[CLOCK_DOMAIN_TIMCLK1],
                                                       SelectedFrequencies[CLOCK_DOMAIN_HCLK],
                                                       &SelectedFrequencies[CLOCK_DOMAIN_PCLK1],
                                                       &SelectedFrequencies[CLOCK_DOMAIN_TIMCLK1]);

    Parameters.Apb2Div = ClockMgmtpCalculateAPBDivisor(&Constraints[CLOCK_DOMAIN_PCLK2],
                                                       &Constraints[CLOCK_DOMAIN_TIMCLK2],
                                                       SelectedFrequencies[CLOCK_DOMAIN_HCLK],
                                                       &SelectedFrequencies[CLOCK_DOMAIN_PCLK2],
                                                       &SelectedFrequencies[CLOCK_DOMAIN_TIMCLK2]);

    // Select appropriate voltage range
    if(SelectedFrequencies[CLOCK_DOMAIN_PLLOUTPUT] > 48000000)
        Parameters.VoltageRange = 1;
    else if(SelectedFrequencies[CLOCK_DOMAIN_PLLOUTPUT] > 24000000)
        Parameters.VoltageRange = 2;
    else
        Parameters.VoltageRange = 3;

    // Select flash access latency
    switch(Parameters.VoltageRange) {
    case 1:
        if(SelectedFrequencies[CLOCK_DOMAIN_HCLK] > 16000000)
            Parameters.FlashAccess = FLASH_ACCESS_64ONEWS;
        else
            Parameters.FlashAccess = FLASH_ACCESS_64ZEROWS;

        break;

    case 2:
        if(SelectedFrequencies[CLOCK_DOMAIN_HCLK] > 16000000)
            Parameters.FlashAccess = FLASH_ACCESS_32ONLY;
        else if(SelectedFrequencies[CLOCK_DOMAIN_HCLK] > 8000000)
            Parameters.FlashAccess = FLASH_ACCESS_64ONEWS;
        else
            Parameters.FlashAccess = FLASH_ACCESS_64ZEROWS;

        break;

    case 3:
        if(SelectedFrequencies[CLOCK_DOMAIN_HCLK] > 4200000)
            Parameters.FlashAccess = FLASH_ACCESS_32ONLY;
        else if(SelectedFrequencies[CLOCK_DOMAIN_HCLK] > 2100000)
            Parameters.FlashAccess = FLASH_ACCESS_64ONEWS;
        else
            Parameters.FlashAccess = FLASH_ACCESS_64ZEROWS;

        break;
    }

    if(ColdStart || memcmp(&Parameters, &ClockMgmtpActiveParameters, sizeof(CLOCKMGMTP_CLOCK_PARAMETERS)) != 0) {
        ClockMgmtpApply(&Parameters);
        memcpy(&ClockMgmtpActiveParameters, &Parameters, sizeof(CLOCKMGMTP_CLOCK_PARAMETERS));

        // Notify everyone about clock change

        for(LIST_HEAD *Entry = ClockMgmtpConstraints.Next; Entry != &ClockMgmtpConstraints; Entry = Entry->Next) {
            CLOCKMGMT_CONSTRAINT *Constraint = LIST_DATA(Entry, CLOCKMGMT_CONSTRAINT, Head);

            if(Constraint->FrequencyChanged != NULL) {
                unsigned int Frequency = SelectedFrequencies[Constraint->Domain];
                
                if((Constraint->MinFrequency != CLOCK_NO_CONSTRAINT && Constraint->MinFrequency > Frequency) ||
                   (Constraint->MaxFrequency != CLOCK_NO_CONSTRAINT && Constraint->MaxFrequency < Frequency))
                    Frequency = CLOCK_OUT_OF_RANGE;

                Constraint->FrequencyChanged(Frequency, Constraint->Arg);
            }
        }
    }
}

void ClockMgmtAddConstraint(CLOCKMGMT_CONSTRAINT *Constraint) {
    unsigned int Interrupts = DispatchDisableInterrupts();

    ListAppend(&ClockMgmtpConstraints, &Constraint->Head);
    ClockMgmtpEvaluateConstraints(0);

    DispatchEnableInterrupts(Interrupts);
}

void ClockMgmtRemoveConstraint(CLOCKMGMT_CONSTRAINT *Constraint) {
    unsigned int Interrupts = DispatchDisableInterrupts();

    ListRemove(&Constraint->Head);
    ClockMgmtpEvaluateConstraints(0);

    DispatchEnableInterrupts(Interrupts);
}

void ClockMgmtInit(void) {
    // Unlock RTC domain
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    PWR->CR      |= PWR_CR_DBP;

    // Enable LSE ready interrupt
    RCC->CIR     |= RCC_CIR_LSERDYIE;

    // Assign highest priority to RCC and enable RCC interrupt
    NVIC_SetPriority(RCC_IRQn, 0);
    NVIC_EnableIRQ(RCC_IRQn);

    ListInit(&ClockMgmtpConstraints);

    if(!(RCC->CSR & RCC_CSR_RTCEN)) {
        // Initiate LSE startup

        RCC->CSR |= RCC_CSR_RTCRST;
        RCC->CSR = (RCC->CSR & ~RCC_CSR_RTCRST) | RCC_CSR_LSEON;
    } else
        TimerRTCReady();

    // Start up HSE
    RCC->CR |= RCC_CR_HSEON;
    while(!(RCC->CR & RCC_CR_HSERDY));

    __disable_irq();

    ClockMgmtpEvaluateConstraints(1);

    __enable_irq();
}

void RCC_IRQHandler(void) {
    if(RCC->CIR & RCC_CIR_LSERDYF) {
        RCC->CIR |= RCC_CIR_LSERDYC;

        // LSE ready, start RTC now
        RCC->CSR = (RCC->CSR & ~RCC_CSR_RTCSEL) | RCC_CSR_RTCSEL_LSE | RCC_CSR_RTCEN;
        TimerRTCReady();
    }
}

unsigned int ClockMgmtCurrentFrequency(CLOCKMGMT_DOMAIN Domain) {
    unsigned int Source;

    switch(Domain) {
    case CLOCK_DOMAIN_PLLOUTPUT:
        if(ClockMgmtpActiveParameters.PllMul == -1 || ClockMgmtpActiveParameters.PllDiv == -1)
            return 0;
        else
            return HS_FREQUENCY * ClockMgmtpPllMultiplers[ClockMgmtpActiveParameters.PllMul];

    case CLOCK_DOMAIN_SYSCLK:
        if(ClockMgmtpActiveParameters.PllMul == -1 || ClockMgmtpActiveParameters.PllDiv == -1)
            return HS_FREQUENCY;
        else
            return HS_FREQUENCY * ClockMgmtpPllMultiplers[ClockMgmtpActiveParameters.PllMul] /
                                  ClockMgmtpPllDivisors[ClockMgmtpActiveParameters.PllDiv];

    case CLOCK_DOMAIN_HCLK:
        Source = ClockMgmtCurrentFrequency(CLOCK_DOMAIN_SYSCLK);
        if(ClockMgmtpActiveParameters.AhbDiv == 0)
            return Source;
        else
            return Source >> ((ClockMgmtpActiveParameters.AhbDiv & 7) + 1);

    case CLOCK_DOMAIN_PCLK1:
        return ClockMgmtCurrentFrequency(CLOCK_DOMAIN_HCLK) >> ClockMgmtpActiveParameters.Apb1Div;

    case CLOCK_DOMAIN_TIMCLK1:
        if(ClockMgmtpActiveParameters.Apb1Div == 0)
            return ClockMgmtCurrentFrequency(CLOCK_DOMAIN_HCLK);
        else
            return ClockMgmtCurrentFrequency(CLOCK_DOMAIN_HCLK) >> (ClockMgmtpActiveParameters.Apb1Div - 1);

    case CLOCK_DOMAIN_PCLK2:
        return ClockMgmtCurrentFrequency(CLOCK_DOMAIN_HCLK) >> ClockMgmtpActiveParameters.Apb2Div;

    case CLOCK_DOMAIN_TIMCLK2:
        if(ClockMgmtpActiveParameters.Apb2Div == 0)
            return ClockMgmtCurrentFrequency(CLOCK_DOMAIN_HCLK);
        else
            return ClockMgmtCurrentFrequency(CLOCK_DOMAIN_HCLK) >> (ClockMgmtpActiveParameters.Apb2Div - 1);

    default:
        return 0;
    }
}

static int ClockMgmtpFindPeripheral(void *Peripheral, volatile unsigned int **Enable, volatile unsigned int **Reset, unsigned int *Flag) {
    unsigned int Address = (unsigned int) Peripheral;

    if(Address >= 0x40000000 && Address <= 0x4000FFFF) { // APB1
        *Enable = &RCC->APB1ENR;
        *Reset = &RCC->APB1RSTR;
        *Flag  = (Address & 0xFFFF) >> 10;

    } else if(Address >= 0x40010000 && Address <= 0x4001FFFF) { // APB2
        *Enable = &RCC->APB2ENR;
        *Reset = &RCC->APB2RSTR;
        *Flag  = (Address & 0xFFFF) >> 10;

        
    } else if(Address >= 0x40020000 && Address <= 0x4002FFFF) { // AHB
        *Enable = &RCC->AHBENR;
        *Reset = &RCC->AHBRSTR;
        *Flag  = (Address & 0xFFFF) >> 10;

        
    } else // FSMC, AES, etc...
        return 0;
    
    return 1;
}

void ClockMgmtEnablePeripheral(void *Peripheral) {
    volatile unsigned int *Enable, *Reset;
    unsigned int Flag;
    if(!ClockMgmtpFindPeripheral(Peripheral, &Enable, &Reset, &Flag))
        return;

    *Enable |= 1 << Flag;
}

void ClockMgmtDisablePeripheral(void *Peripheral) {
    volatile unsigned int *Enable, *Reset;
    unsigned int Flag;
    if(!ClockMgmtpFindPeripheral(Peripheral, &Enable, &Reset, &Flag))
        return;

    *Enable &= ~(1 << Flag);   
}

void ClockMgmtResetPeripheral(void *Peripheral) {
    volatile unsigned int *Enable, *Reset;
    unsigned int Flag;
    if(!ClockMgmtpFindPeripheral(Peripheral, &Enable, &Reset, &Flag))
        return;

    *Reset |= 1 << Flag;
    *Reset &= ~(1 << Flag);      
}

