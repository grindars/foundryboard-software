#include <stm32l/gpio.h>
#include <class/gpio.h>

#include <clockmgmt.h>
#include <dispatch.h>

typedef struct STM32LGPIO_DEVICE_DATA {
    GPIO_TypeDef *Device;
    unsigned short FreeLines;
    unsigned short DynamicMode;
} STM32LGPIO_DEVICE_DATA;

typedef struct STM32LGPIO_HANDLE_DATA {
    unsigned int AllocatedLines;
} STM32LGPIO_HANDLE_DATA;

static unsigned int STM32LGPIOpExpandMode(unsigned short Lines) {
    unsigned int Mask = 0;

    for(unsigned int Index = 0; Index < 16; Index++)
        if(Lines & (1 << Index))
            Mask |= 3 << (2 * Index);

    return Mask;
}

static void STM32LGPIOpAttach(IO_DEVICE Device, const void *PlatformData, IO_DRV_COMPLETION_CALLBACK Callback, void *Argument) {
    const STM32LGPIO_BOARD_CONFIG *Config = PlatformData;
    STM32LGPIO_DEVICE_DATA *Data = IoDrvDeviceData(Device);

    Data->Device = Config->Device;
    ClockMgmtEnablePeripheral(Data->Device);

    Data->Device->MODER = Config->Mode;
    Data->Device->OTYPER = Config->OutputType;
    Data->Device->OSPEEDR = Config->Speed;
    Data->Device->PUPDR = Config->Pull;
    Data->Device->ODR = Config->InitialData;
    Data->Device->AFR[0] = (unsigned int) Config->AF;
    Data->Device->AFR[1] = (unsigned int)(Config->AF >> 32);
    Data->FreeLines = Config->Claimable;
    Data->DynamicMode = Config->DynamicMode;

    Callback(IOSTATUS_SUCCESS, Argument);
}

static void STM32LGPIOpDetach(IO_DEVICE Device, IO_DRV_COMPLETION_CALLBACK Callback, void *Argument) {
    STM32LGPIO_DEVICE_DATA *Data = IoDrvDeviceData(Device);

    ClockMgmtDisablePeripheral(Data->Device);
    ClockMgmtResetPeripheral(Data->Device);

    Callback(IOSTATUS_SUCCESS, Argument);   
}

static void STM32LGPIOpOpen(const char *Filename, unsigned int Mode, HANDLE Handle, IO_DRV_COMPLETION_CALLBACK Callback, void *Argument) {
    (void) Filename;
    (void) Mode;

    STM32LGPIO_HANDLE_DATA *Data = IoDrvHandleData(Handle);
    Data->AllocatedLines = 0;

    Callback(IOSTATUS_SUCCESS, Argument);   
}

static void STM32LGPIOpDuplicate(HANDLE Handle, HANDLE NewHandle, IO_DRV_COMPLETION_CALLBACK Callback, void *Argument) {
    STM32LGPIO_HANDLE_DATA *Data = IoDrvHandleData(Handle);
    STM32LGPIO_HANDLE_DATA *NewData = IoDrvHandleData(NewHandle);
    
    NewData->AllocatedLines = 0;    

    Callback(Data->AllocatedLines == 0 ? IOSTATUS_SUCCESS : IOSTATUS_BUSY, Argument);
}

static void STM32LGPIOpClose(HANDLE Handle, IO_DRV_COMPLETION_CALLBACK Callback, void *Argument) {
    STM32LGPIO_HANDLE_DATA *HandleData = IoDrvHandleData(Handle); 
    IO_DEVICE Device = IoDrvDeviceForHandle(Handle);
    STM32LGPIO_DEVICE_DATA *Data = IoDrvDeviceData(Device);

    Data->FreeLines |= HandleData->AllocatedLines;

    Callback(IOSTATUS_SUCCESS, Argument);
} 

static void STM32LGPIOpClaim(CORE_IO_OPERATION Operation, const GPIO_SIMPLE_REQUEST *Request,
                             STM32LGPIO_HANDLE_DATA *HandleData, STM32LGPIO_DEVICE_DATA *DeviceData) {

    IoDrvAddTransferred(Operation, sizeof(GPIO_SIMPLE_REQUEST));

    if(Request->Lines & ~DeviceData->FreeLines)
        IoDrvComplete(Operation, IOSTATUS_BUSY);
    else {
        HandleData->AllocatedLines |= Request->Lines;
        DeviceData->FreeLines &= ~Request->Lines;
        IoDrvComplete(Operation, IOSTATUS_SUCCESS);
    }
}

static void STM32LGPIOpRelease(CORE_IO_OPERATION Operation, const GPIO_SIMPLE_REQUEST *Request,
                               STM32LGPIO_HANDLE_DATA *HandleData, STM32LGPIO_DEVICE_DATA *DeviceData) {

    IoDrvAddTransferred(Operation, sizeof(GPIO_SIMPLE_REQUEST));

    unsigned int Lines = Request->Lines & HandleData->AllocatedLines;

    HandleData->AllocatedLines &= ~Lines;
    DeviceData->FreeLines |= Lines;
    IoDrvComplete(Operation, IOSTATUS_SUCCESS);
}

static void STM32LGPIOpAssert(CORE_IO_OPERATION Operation, const GPIO_SIMPLE_REQUEST *Request,
                              STM32LGPIO_HANDLE_DATA *HandleData, STM32LGPIO_DEVICE_DATA *DeviceData) {

    IoDrvAddTransferred(Operation, sizeof(GPIO_SIMPLE_REQUEST));

    unsigned int Lines = Request->Lines & HandleData->AllocatedLines;
    DeviceData->Device->BSRRL = Lines;
    IoDrvComplete(Operation, IOSTATUS_SUCCESS);   
}

static void STM32LGPIOpDeassert(CORE_IO_OPERATION Operation, const GPIO_SIMPLE_REQUEST *Request,
                                STM32LGPIO_HANDLE_DATA *HandleData, STM32LGPIO_DEVICE_DATA *DeviceData) {

    IoDrvAddTransferred(Operation, sizeof(GPIO_SIMPLE_REQUEST));

    unsigned int Lines = Request->Lines & HandleData->AllocatedLines;
    DeviceData->Device->BSRRH = Lines;
    IoDrvComplete(Operation, IOSTATUS_SUCCESS);   
}

static void STM32LGPIOpRead(CORE_IO_OPERATION Operation, GPIO_SIMPLE_REQUEST *Request,
                            STM32LGPIO_HANDLE_DATA *HandleData, STM32LGPIO_DEVICE_DATA *DeviceData) {

    IoDrvAddTransferred(Operation, sizeof(GPIO_SIMPLE_REQUEST));

    Request->Lines = DeviceData->Device->IDR & HandleData->AllocatedLines;

    IoDrvComplete(Operation, IOSTATUS_SUCCESS);   
}

static void STM32LGPIOpWrite(CORE_IO_OPERATION Operation, GPIO_WRITE_REQUEST *Request,
                             STM32LGPIO_HANDLE_DATA *HandleData, STM32LGPIO_DEVICE_DATA *DeviceData) {

    IoDrvAddTransferred(Operation, sizeof(GPIO_WRITE_REQUEST));

    unsigned int Lines = Request->Lines & HandleData->AllocatedLines;
    unsigned int Values = Request->Values & HandleData->AllocatedLines;

    unsigned int Saved = DispatchDisableInterrupts();

    DeviceData->Device->ODR = (DeviceData->Device->ODR & ~Lines) | Values;

    DispatchEnableInterrupts(Saved);

    IoDrvComplete(Operation, IOSTATUS_SUCCESS);   
}

static void STM32LGPIOpSetInput(CORE_IO_OPERATION Operation, const GPIO_SIMPLE_REQUEST *Request,
                                STM32LGPIO_HANDLE_DATA *HandleData, STM32LGPIO_DEVICE_DATA *DeviceData) {

    IoDrvAddTransferred(Operation, sizeof(GPIO_SIMPLE_REQUEST));

    unsigned int Lines = STM32LGPIOpExpandMode(Request->Lines & HandleData->AllocatedLines & DeviceData->DynamicMode);
    unsigned int Saved = DispatchDisableInterrupts();

    DeviceData->Device->MODER = (DeviceData->Device->MODER & ~Lines) | 0x00000000;

    DispatchEnableInterrupts(Saved);

    IoDrvComplete(Operation, IOSTATUS_SUCCESS);   
}

static void STM32LGPIOpSetOutput(CORE_IO_OPERATION Operation, const GPIO_SIMPLE_REQUEST *Request,
                                STM32LGPIO_HANDLE_DATA *HandleData, STM32LGPIO_DEVICE_DATA *DeviceData) {

    IoDrvAddTransferred(Operation, sizeof(GPIO_SIMPLE_REQUEST));

    unsigned int Lines = STM32LGPIOpExpandMode(Request->Lines & HandleData->AllocatedLines & DeviceData->DynamicMode);
    unsigned int Saved = DispatchDisableInterrupts();

    DeviceData->Device->MODER = (DeviceData->Device->MODER & ~Lines) | 0x55555555;

    DispatchEnableInterrupts(Saved);

    IoDrvComplete(Operation, IOSTATUS_SUCCESS);   
}


static void STM32LGPIOpSubmit(CORE_IO_OPERATION Operation, IO_DRV_COMPLETION_CALLBACK Callback, void *Argument) {
    unsigned int Request = IoDrvRequest(Operation);

    HANDLE Handle = IoDrvHandle(Operation);
    STM32LGPIO_HANDLE_DATA *HandleData = IoDrvHandleData(Handle);

    IO_DEVICE Device = IoDrvDeviceForHandle(Handle);
    STM32LGPIO_DEVICE_DATA *DeviceData = IoDrvDeviceData(Device);

    void *Buffer = IoDrvBuffer(Operation);
    size_t BufferSize = IoDrvSize(Operation);

    if(Buffer == NULL ||
       (Request == GPIO_WRITE && BufferSize != sizeof(GPIO_WRITE_REQUEST)) ||
       (Request != GPIO_WRITE && BufferSize != sizeof(GPIO_SIMPLE_REQUEST)))
        return Callback(IOSTATUS_INVALID_BUFFER, Argument);

    switch(Request) {
    case GPIO_CLAIM:
        STM32LGPIOpClaim(Operation, Buffer, HandleData, DeviceData);
        break;

    case GPIO_RELEASE:
        STM32LGPIOpRelease(Operation, Buffer, HandleData, DeviceData);
        break;
    
    case GPIO_ASSERT:
        STM32LGPIOpAssert(Operation, Buffer, HandleData, DeviceData);
        break;

    case GPIO_DEASSERT:
        STM32LGPIOpDeassert(Operation, Buffer, HandleData, DeviceData);
        break;

    case GPIO_READ:
        STM32LGPIOpRead(Operation, Buffer, HandleData, DeviceData);
        break;

    case GPIO_WRITE:
        STM32LGPIOpWrite(Operation, Buffer, HandleData, DeviceData);
        break;

    case GPIO_SET_INPUT:
        STM32LGPIOpSetInput(Operation, Buffer, HandleData, DeviceData);
        break;

    case GPIO_SET_OUTPUT:
        STM32LGPIOpSetOutput(Operation, Buffer, HandleData, DeviceData);
        break;
    default:
        return Callback(IOSTATUS_UNSUPPORTED_REQUEST, Argument);
    }


    Callback(IOSTATUS_SUCCESS, Argument);
}

static void STM32LGPIOpCancel(CORE_IO_OPERATION Operation, IO_DRV_COMPLETION_CALLBACK Callback, void *Argument) {
    (void) Operation;
  
    Callback(IOSTATUS_SUCCESS, Argument);
}

const IO_DRIVER STM32LGPIO = {
    .DeviceDataSize = sizeof(STM32LGPIO_DEVICE_DATA),
    .HandleDataSize = sizeof(STM32LGPIO_HANDLE_DATA),
    .OperationDataSize = 0,

    .Attach = STM32LGPIOpAttach,
    .Detach = STM32LGPIOpDetach,

    .Open      = STM32LGPIOpOpen,
    .Duplicate = STM32LGPIOpDuplicate,
    .Close     = STM32LGPIOpClose,

    .Submit    = STM32LGPIOpSubmit,
    .Cancel    = STM32LGPIOpCancel
};
