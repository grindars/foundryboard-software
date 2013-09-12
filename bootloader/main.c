#include <timer.h>
#include <clockmgmt.h>
#include <dispatch.h>
#include <io.h>
#include <stm32l/gpio.h>
#include <class/gpio.h>

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

static const IO_BOARD FoundryDemoBoard = {
    .Devices = {
        BOARD_DEVICE(&STM32LGPIO, "GPIOA:", &GPIOAConfig),
        BOARD_DEVICE(&STM32LGPIO, "GPIOB:", &GPIOBConfig),
        // GPIOC: oscillator only
        // GPIOH: oscillator only
        BOARD_END
    }
};

int main(void) {
    DispatchInit();
    TimerInit();
    ClockMgmtInit();
    IoInit();

    IOSTATUS Status = IoBringUp(&FoundryDemoBoard);
    if(FAILURE(Status))
        IoHaltWithStatus(Status);

    HANDLE Handle;
    Status = IoOpenFile("GPIOB:", 0, &Handle);

    if(FAILURE(Status))
        IoHaltWithStatus(Status);

    Status = GpioClaim(Handle, 1 << 1);
    if(FAILURE(Status))
        IoHaltWithStatus(Status);

    while(1) {
        GpioAssert(Handle, 1 << 1);
        IoSynchronize(NULL, 0, 1000, NULL);
        GpioDeassert(Handle, 1 << 1);
        IoSynchronize(NULL, 0, 1000, NULL);
    }

    return 0;
}
