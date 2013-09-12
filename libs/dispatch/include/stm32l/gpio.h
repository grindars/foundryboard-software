#ifndef __STM32L__GPIO__H__
#define __STM32L__GPIO__H__

#include <io.h>
#include <stm32l1xx.h>

typedef struct STM32LGPIO_BOARD_CONFIG {
    GPIO_TypeDef *Device;
    unsigned long long AF;

    unsigned int Mode;
    unsigned int Speed;
    unsigned int Pull;

    unsigned short InitialData;
    unsigned short OutputType;
    unsigned short DynamicMode;
    unsigned short Claimable;
} STM32LGPIO_BOARD_CONFIG;

extern const IO_DRIVER STM32LGPIO;

#endif
