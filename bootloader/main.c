#include <timer.h>
#include <clockmgmt.h>
#include <dispatch.h>
#include <io.h>

#include <stm32l/gpio.h>
#include <portable/uc1601s.h>

static const STM32LGPIO_BOARD_CONFIG GPIOAConfig = {
    .Device      = GPIOA,
    .Mode        = 0x2A808000,
    .OutputType  = 0x0000,
    .Speed       = 0x3FC00000,
    .Pull        = 0x54111555,
    .InitialData = 0x0000,
    .DynamicMode = 0x0000,
    .Claimable   = 0x074D,
    .AF          = 0x000AA00020000000
};

static const STM32LGPIO_BOARD_CONFIG GPIOBConfig = {
    .Device      = GPIOB,
    .Mode        = 0x00006554,
    .OutputType  = 0x0000,
    .Speed       = 0xFFFFFFFC,
    .Pull        = 0x55554001,
    .InitialData = 0x0000,
    .DynamicMode = 0xFF00,
    .Claimable   = 0xFF3E,
    .AF          = 0x0000000077000000
};

static UC1601S_GPIO_LOWER UC1601SGPIOLowerConfig = {
    .GPIO = "GPIOB:",
    .CD   = 1 << 1,
    .RST  = 1 << 2,
    .CS   = 1 << 3,
    .RD   = 1 << 4,
    .WR   = 1 << 5,
    .DB   = 8
};

static const IO_BOARD FoundryDemoBoard = {
    .Devices = {
        BOARD_DEVICE(&STM32LGPIO, "GPIOA:", &GPIOAConfig),
        BOARD_DEVICE(&STM32LGPIO, "GPIOB:", &GPIOBConfig),
        // GPIOC: oscillator only
        // GPIOH: oscillator only,
        BOARD_DEVICE(&UC1601SGPIOLower, "UC1601S:", &UC1601SGPIOLowerConfig),
        BOARD_END
    }
};

static inline void SingleCommand(HANDLE Handle, unsigned char Command) {
    IoExecute(Handle, UC1601S_LOWER_WRITE_COMMAND, &Command, sizeof(Command), NULL);
}

static inline void DoubleCommand(HANDLE Handle, unsigned char Command1, unsigned char Command2) {
    unsigned char Command[] = { Command1, Command2 };

    IoExecute(Handle, UC1601S_LOWER_WRITE_COMMAND, Command, sizeof(Command), NULL);
}

int main(void) {
    DispatchInit();
    TimerInit();
    ClockMgmtInit();
    IoInit();

    static CLOCKMGMT_CONSTRAINT Faster = {
        .Domain = CLOCK_DOMAIN_HCLK,
        .MinFrequency = 32000000,
        .MaxFrequency = CLOCK_NO_CONSTRAINT,
        .FrequencyChanged = NULL,
        .Arg = NULL
    };

    ClockMgmtAddConstraint(&Faster);

    IOSTATUS Status = IoBringUp(&FoundryDemoBoard);
    if(FAILURE(Status))
        IoHaltWithStatus(Status);

    HANDLE Handle;
    Status = IoOpenFile("UC1601S:", 0, &Handle);

    if(FAILURE(Status))
        IoHaltWithStatus(Status);

    Status = IoExecute(Handle, UC1601S_LOWER_RESET, NULL, 0, NULL);
    if(FAILURE(Status))
        IoHaltWithStatus(Status);


    SingleCommand(Handle, 0x24);
    SingleCommand(Handle, 0xC0);
    SingleCommand(Handle, 0xA0);
    SingleCommand(Handle, 0xEB);
    DoubleCommand(Handle, 0x81, 96);
    SingleCommand(Handle, 0xAF);

    IoCloseHandle(Handle);
    IoHaltWithStatus(IOSTATUS_SUCCESS);

    return 0;
}
