#include <portable/uc1601s.h>
#include <class/gpio.h>
#include <timer.h>

typedef struct UC1601S_GPIO_LOWER_DEVICE_DATA {
    HANDLE GPIO;
    const UC1601S_GPIO_LOWER *Config;
    IO_DRV_OPERATION_QUEUE Queue;

    union {
        GPIO_SIMPLE_REQUEST Request;
        GPIO_WRITE_REQUEST Write;
    };
    unsigned int AddressPins;
    unsigned int Position;
    IO_OPERATION Operation;
    IOSTATUS Status;

    IO_DRV_COMPLETION_CALLBACK Callback;
    void *Argument;

    TIMER Timer;
} UC1601S_GPIO_LOWER_DEVICE_DATA;

static void UC1601SGPIOLowerpExecute(CORE_IO_OPERATION Operation);
static void UC1601SGPIOLowerpAbort(CORE_IO_OPERATION Operation);

static void UC1601SGPIOLowerpBeginWriteCycle(UC1601S_GPIO_LOWER_DEVICE_DATA *Data);
static void UC1601SGPIOLowerpBeginReadCycle(UC1601S_GPIO_LOWER_DEVICE_DATA *Data);

static void UC1601SGPIOLowerpAttachCleanupCloseComplete(IOSTATUS Status, void *Argument) {
    (void) Status;

    UC1601S_GPIO_LOWER_DEVICE_DATA *Data = Argument;

    Data->Callback(Data->Status, Data->Argument);
}

static void UC1601SGPIOLowerpAttachAssertComplete(IOSTATUS Status, IO_OPERATION Operation, unsigned int BytesTransferred, void *Argument) {
    (void) BytesTransferred;
    
    UC1601S_GPIO_LOWER_DEVICE_DATA *Data = Argument;

    IoRelease(Operation);

    if(FAILURE(Status)) {
        Data->Status = Status;
        IoDrvCloseHandle(Data->GPIO, UC1601SGPIOLowerpAttachCleanupCloseComplete, Data);        
    } else {
        Data->Callback(IOSTATUS_SUCCESS, Data->Argument);
    }
}

static void UC1601SGPIOLowerpAttachAssertSubmitComplete(IOSTATUS Status, void *Argument) {
    UC1601S_GPIO_LOWER_DEVICE_DATA *Data = Argument;

    if(FAILURE(Status)) {
        Data->Status = Status;
        IoDrvCloseHandle(Data->GPIO, UC1601SGPIOLowerpAttachCleanupCloseComplete, Data);
    } else {
        IoDrvSubscribe(Data->Operation, UC1601SGPIOLowerpAttachAssertComplete, Data);
    }
}

static void UC1601SGPIOLowerpAttachClaimComplete(IOSTATUS Status, IO_OPERATION Operation, unsigned int BytesTransferred, void *Argument) {
    (void) BytesTransferred;
    
    UC1601S_GPIO_LOWER_DEVICE_DATA *Data = Argument;

    IoRelease(Operation);

    if(FAILURE(Status)) {
        Data->Status = Status;
        IoDrvCloseHandle(Data->GPIO, UC1601SGPIOLowerpAttachCleanupCloseComplete, Data);        
    } else {
        IoDrvSubmit(Data->GPIO, GPIO_ASSERT, &Data->Request, sizeof(Data->Request), &Data->Operation,
                    UC1601SGPIOLowerpAttachAssertSubmitComplete, Data);
    }
}

static void UC1601SGPIOLowerpAttachClaimSubmitComplete(IOSTATUS Status, void *Argument) {
    UC1601S_GPIO_LOWER_DEVICE_DATA *Data = Argument;

    if(FAILURE(Status)) {
        Data->Status = Status;
        IoDrvCloseHandle(Data->GPIO, UC1601SGPIOLowerpAttachCleanupCloseComplete, Data);
    } else {
        IoDrvSubscribe(Data->Operation, UC1601SGPIOLowerpAttachClaimComplete, Data);
    }
}

static void UC1601SGPIOLowerpAttachOpenComplete(IOSTATUS Status, void *Argument) {
    UC1601S_GPIO_LOWER_DEVICE_DATA *Data = Argument;

    if(FAILURE(Status)) {
        Data->Callback(Status, Data->Argument);
    } else {
        Data->Request.Lines = Data->Config->CD | Data->Config->RST | Data->Config->CS | Data->Config->RD | Data->Config->WR |
                              (0xFF << Data->Config->DB);

        IoDrvSubmit(Data->GPIO, GPIO_CLAIM, &Data->Request, sizeof(Data->Request), &Data->Operation,
                    UC1601SGPIOLowerpAttachClaimSubmitComplete, Data);
    }
}

static void UC1601SGPIOLowerpAttach(IO_DEVICE Device, const void *PlatformData, IO_DRV_COMPLETION_CALLBACK Callback, void *Argument) {
    const UC1601S_GPIO_LOWER *Config = PlatformData;
    UC1601S_GPIO_LOWER_DEVICE_DATA *Data = IoDrvDeviceData(Device);

    Data->Config = Config;
    Data->Callback = Callback;
    Data->Argument = Argument;

    IoDrvOperationQueueInitialize(&Data->Queue, UC1601SGPIOLowerpExecute, UC1601SGPIOLowerpAbort);

    IoDrvOpenFile(Config->GPIO, 0, &Data->GPIO, UC1601SGPIOLowerpAttachOpenComplete, Data);
}

static void UC1601SGPIOLowerpDetachCloseComplete(IOSTATUS Status, void *Argument) {
    UC1601S_GPIO_LOWER_DEVICE_DATA *Data = Argument;

    Data->Callback(Status, Data->Argument);
}

static void UC1601SGPIOLowerpDetach(IO_DEVICE Device, IO_DRV_COMPLETION_CALLBACK Callback, void *Argument) {
    UC1601S_GPIO_LOWER_DEVICE_DATA *Data = IoDrvDeviceData(Device);
    
    Data->Callback = Callback;
    Data->Argument = Argument;

    IoDrvCloseHandle(Data->GPIO, UC1601SGPIOLowerpDetachCloseComplete, Data);
}

static void UC1601SGPIOLowerpOpen(const char *Filename, unsigned int Mode, HANDLE Handle, IO_DRV_COMPLETION_CALLBACK Callback, void *Argument) {
    (void) Filename;
    (void) Mode;
    (void) Handle;

    Callback(IOSTATUS_SUCCESS, Argument);
}

static void UC1601SGPIOLowerpDuplicate(HANDLE Handle, HANDLE NewHandle, IO_DRV_COMPLETION_CALLBACK Callback, void *Argument) {
    (void) Handle;
    (void) NewHandle;

    Callback(IOSTATUS_SUCCESS, Argument);
}

static void UC1601SGPIOLowerpClose(HANDLE Handle, IO_DRV_COMPLETION_CALLBACK Callback, void *Argument) {
    (void) Handle;

    Callback(IOSTATUS_SUCCESS, Argument);
}

static void UC1601SGPIOLowerpSubmit(CORE_IO_OPERATION Operation, IO_DRV_COMPLETION_CALLBACK Callback, void *Argument) {
    HANDLE Handle = IoDrvHandle(Operation);
    IO_DEVICE Device = IoDrvDeviceForHandle(Handle);
    UC1601S_GPIO_LOWER_DEVICE_DATA *Data = IoDrvDeviceData(Device);
    
    IoDrvOperationQueueSubmit(&Data->Queue, Operation);

    Callback(IOSTATUS_SUCCESS, Argument);
}

static void UC1601SGPIOLowerpCancel(CORE_IO_OPERATION Operation, IO_DRV_COMPLETION_CALLBACK Callback, void *Argument) {
    HANDLE Handle = IoDrvHandle(Operation);
    IO_DEVICE Device = IoDrvDeviceForHandle(Handle);
    UC1601S_GPIO_LOWER_DEVICE_DATA *Data = IoDrvDeviceData(Device);
    
    IoDrvOperationQueueCancel(&Data->Queue, Operation);

    Callback(IOSTATUS_SUCCESS, Argument);    
}

static void UC1601SGPIOLowerpResetHighTimeout(TIMER *Timer, TIME TimeElapsed, void *Argument) {
    (void) TimeElapsed;

    UC1601S_GPIO_LOWER_DEVICE_DATA *Data = Argument;
    
    TimerRelease(Timer);

    IoDrvOperationQueueComplete(&Data->Queue, IOSTATUS_SUCCESS);
}

static void UC1601SGPIOLowerpResetAssertComplete(IOSTATUS Status, IO_OPERATION Operation, unsigned int BytesTransferred, void *Argument) {
    (void) BytesTransferred;

    UC1601S_GPIO_LOWER_DEVICE_DATA *Data = Argument;
    
    IoRelease(Operation);

    if(FAILURE(Status))
        IoDrvOperationQueueComplete(&Data->Queue, Status);
    else {
        TimerCreateSingleShot(&Data->Timer, 50, UC1601SGPIOLowerpResetHighTimeout, Data);
    }
}

static void UC1601SGPIOLowerpResetAssertSubmitComplete(IOSTATUS Status, void *Argument) {
    UC1601S_GPIO_LOWER_DEVICE_DATA *Data = Argument;
    
    if(FAILURE(Status))
        IoDrvOperationQueueComplete(&Data->Queue, Status);
    else
        IoDrvSubscribe(Data->Operation, UC1601SGPIOLowerpResetAssertComplete, Data);     
}

static void UC1601SGPIOLowerpResetLowTimeout(TIMER *Timer, TIME TimeElapsed, void *Argument) {
    (void) TimeElapsed;

    UC1601S_GPIO_LOWER_DEVICE_DATA *Data = Argument;
    
    TimerRelease(Timer);

    IoDrvSubmit(Data->GPIO, GPIO_ASSERT, &Data->Request, sizeof(Data->Request), &Data->Operation,
            UC1601SGPIOLowerpResetAssertSubmitComplete, Data);
}

static void UC1601SGPIOLowerpResetDeassertComplete(IOSTATUS Status, IO_OPERATION Operation, unsigned int BytesTransferred, void *Argument) {
    (void) BytesTransferred;

    UC1601S_GPIO_LOWER_DEVICE_DATA *Data = Argument;
    
    IoRelease(Operation);

    if(FAILURE(Status))
        IoDrvOperationQueueComplete(&Data->Queue, Status);
    else {
        TimerCreateSingleShot(&Data->Timer, 50, UC1601SGPIOLowerpResetLowTimeout, Data);
    }
}

static void UC1601SGPIOLowerpResetDeassertSubmitComplete(IOSTATUS Status, void *Argument) {
    UC1601S_GPIO_LOWER_DEVICE_DATA *Data = Argument;
    
    if(FAILURE(Status))
        IoDrvOperationQueueComplete(&Data->Queue, Status);
    else
        IoDrvSubscribe(Data->Operation, UC1601SGPIOLowerpResetDeassertComplete, Data);     
}

static void UC1601SGPIOLowerpReset(UC1601S_GPIO_LOWER_DEVICE_DATA *Data) {
    Data->Request.Lines = Data->Config->RST;
    IoDrvSubmit(Data->GPIO, GPIO_DEASSERT, &Data->Request, sizeof(Data->Request), &Data->Operation,
                UC1601SGPIOLowerpResetDeassertSubmitComplete, Data);
}

static void UC1601SGPIOLowerpWriteInputComplete(IOSTATUS Status, IO_OPERATION Operation, unsigned int BytesTransferred, void *Argument) {
    (void) BytesTransferred;

    UC1601S_GPIO_LOWER_DEVICE_DATA *Data = Argument;
    
    IoRelease(Operation);
    CORE_IO_OPERATION InitialOperation = IoDrvOperationQueueCurrent(&Data->Queue);

    if(FAILURE(Status) || Data->Position == IoDrvSize(InitialOperation)) {
        IoDrvOperationQueueComplete(&Data->Queue, Status);
    } else {
        UC1601SGPIOLowerpBeginWriteCycle(Data);
    }
}

static void UC1601SGPIOLowerpWriteInputSubmitComplete(IOSTATUS Status, void *Argument) {
    UC1601S_GPIO_LOWER_DEVICE_DATA *Data = Argument;
    
    if(FAILURE(Status))
        IoDrvOperationQueueComplete(&Data->Queue, Status);
    else
        IoDrvSubscribe(Data->Operation, UC1601SGPIOLowerpWriteInputComplete, Data);     
}

static void UC1601SGPIOLowerpWriteAssertComplete(IOSTATUS Status, IO_OPERATION Operation, unsigned int BytesTransferred, void *Argument) {
    (void) BytesTransferred;

    UC1601S_GPIO_LOWER_DEVICE_DATA *Data = Argument;
    
    IoRelease(Operation);

    if(FAILURE(Status))
        IoDrvOperationQueueComplete(&Data->Queue, Status);
    else {
        Data->Request.Lines = 0xFF << Data->Config->DB;
        IoDrvSubmit(Data->GPIO, GPIO_SET_INPUT, &Data->Request, sizeof(Data->Request), &Data->Operation,
                    UC1601SGPIOLowerpWriteInputSubmitComplete, Data);           
    }
}

static void UC1601SGPIOLowerpWriteAssertSubmitComplete(IOSTATUS Status, void *Argument) {
    UC1601S_GPIO_LOWER_DEVICE_DATA *Data = Argument;
    
    if(FAILURE(Status))
        IoDrvOperationQueueComplete(&Data->Queue, Status);
    else
        IoDrvSubscribe(Data->Operation, UC1601SGPIOLowerpWriteAssertComplete, Data);     
}

static void UC1601SGPIOLowerpWriteWriteComplete(IOSTATUS Status, IO_OPERATION Operation, unsigned int BytesTransferred, void *Argument) {
    (void) BytesTransferred;

    UC1601S_GPIO_LOWER_DEVICE_DATA *Data = Argument;
    
    IoRelease(Operation);

    if(FAILURE(Status))
        IoDrvOperationQueueComplete(&Data->Queue, Status);
    else {
        CORE_IO_OPERATION Operation = IoDrvOperationQueueCurrent(&Data->Queue);

        if(IoDrvSize(Operation) > Data->Position)
            Data->Write.Lines = Data->Config->WR | (0xFF << Data->Config->DB);

        IoDrvSubmit(Data->GPIO, GPIO_ASSERT, &Data->Request, sizeof(Data->Request), &Data->Operation,
                    UC1601SGPIOLowerpWriteAssertSubmitComplete, Data);   
    }
}

static void UC1601SGPIOLowerpWriteWriteSubmitComplete(IOSTATUS Status, void *Argument) {
    UC1601S_GPIO_LOWER_DEVICE_DATA *Data = Argument;
    
    if(FAILURE(Status))
        IoDrvOperationQueueComplete(&Data->Queue, Status);
    else
        IoDrvSubscribe(Data->Operation, UC1601SGPIOLowerpWriteWriteComplete, Data);     
}

static void UC1601SGPIOLowerpWriteOutputComplete(IOSTATUS Status, IO_OPERATION Operation, unsigned int BytesTransferred, void *Argument) {
    (void) BytesTransferred;

    UC1601S_GPIO_LOWER_DEVICE_DATA *Data = Argument;
    
    IoRelease(Operation);

    if(FAILURE(Status))
        IoDrvOperationQueueComplete(&Data->Queue, Status);
    else {
        CORE_IO_OPERATION Operation = IoDrvOperationQueueCurrent(&Data->Queue);
        unsigned char *Buffer = IoDrvBuffer(Operation);
        IoDrvAddTransferred(Operation, 1);

        Data->Write.Lines = Data->AddressPins | Data->Config->WR | (0xFF << Data->Config->DB) | Data->Config->CS;
        Data->Write.Values = Buffer[Data->Position++] << Data->Config->DB;
        IoDrvSubmit(Data->GPIO, GPIO_WRITE, &Data->Write, sizeof(Data->Write), &Data->Operation,
                    UC1601SGPIOLowerpWriteWriteSubmitComplete, Data);           
    }
}

static void UC1601SGPIOLowerpWriteOutputSubmitComplete(IOSTATUS Status, void *Argument) {
    UC1601S_GPIO_LOWER_DEVICE_DATA *Data = Argument;
    
    if(FAILURE(Status))
        IoDrvOperationQueueComplete(&Data->Queue, Status);
    else
        IoDrvSubscribe(Data->Operation, UC1601SGPIOLowerpWriteOutputComplete, Data);     
}

static void UC1601SGPIOLowerpBeginWriteCycle(UC1601S_GPIO_LOWER_DEVICE_DATA *Data) {
    Data->Request.Lines = 0xFF << Data->Config->DB;

    IoDrvSubmit(Data->GPIO, GPIO_SET_OUTPUT, &Data->Request, sizeof(Data->Request), &Data->Operation,
                UC1601SGPIOLowerpWriteOutputSubmitComplete, Data);   
}

static void UC1601SGPIOLowerpWrite(UC1601S_GPIO_LOWER_DEVICE_DATA *Data, unsigned int AddressPins) {
    Data->AddressPins = AddressPins;
    Data->Position = 0;

    UC1601SGPIOLowerpBeginWriteCycle(Data);
}

static void UC1601SGPIOLowerpReadAssertComplete(IOSTATUS Status, IO_OPERATION Operation, unsigned int BytesTransferred, void *Argument) {
    (void) BytesTransferred;

    UC1601S_GPIO_LOWER_DEVICE_DATA *Data = Argument;
    
    IoRelease(Operation);

    CORE_IO_OPERATION InitialOperation = IoDrvOperationQueueCurrent(&Data->Queue);

    if(FAILURE(Status) || Data->Position == IoDrvSize(InitialOperation)) {
        IoDrvOperationQueueComplete(&Data->Queue, Status);
    } else {
        UC1601SGPIOLowerpBeginReadCycle(Data);
    }
}

static void UC1601SGPIOLowerpReadAssertSubmitComplete(IOSTATUS Status, void *Argument) {
    UC1601S_GPIO_LOWER_DEVICE_DATA *Data = Argument;
    
    if(FAILURE(Status))
        IoDrvOperationQueueComplete(&Data->Queue, Status);
    else
        IoDrvSubscribe(Data->Operation, UC1601SGPIOLowerpReadAssertComplete, Data);     
}

static void UC1601SGPIOLowerpReadReadComplete(IOSTATUS Status, IO_OPERATION Operation, unsigned int BytesTransferred, void *Argument) {
    (void) BytesTransferred;

    UC1601S_GPIO_LOWER_DEVICE_DATA *Data = Argument;
    
    IoRelease(Operation);

    if(FAILURE(Status))
        IoDrvOperationQueueComplete(&Data->Queue, Status);
    else {
        CORE_IO_OPERATION Operation = IoDrvOperationQueueCurrent(&Data->Queue);
        unsigned char *Buffer = IoDrvBuffer(Operation);
        IoDrvAddTransferred(Operation, 1);

        Buffer[Data->Position++] = (Data->Request.Lines >> Data->Config->DB) & 0xFF;

        if(Data->Position == IoDrvSize(Operation))
            Data->Request.Lines = Data->AddressPins | Data->Config->RD | Data->Config->CS;
        else
            Data->Request.Lines = Data->Config->RD;

        IoDrvSubmit(Data->GPIO, GPIO_ASSERT, &Data->Request, sizeof(Data->Request), &Data->Operation,
                    UC1601SGPIOLowerpReadAssertSubmitComplete, Data);  
    }
}

static void UC1601SGPIOLowerpReadReadSubmitComplete(IOSTATUS Status, void *Argument) {
    UC1601S_GPIO_LOWER_DEVICE_DATA *Data = Argument;
    
    if(FAILURE(Status))
        IoDrvOperationQueueComplete(&Data->Queue, Status);
    else
        IoDrvSubscribe(Data->Operation, UC1601SGPIOLowerpReadReadComplete, Data);     
}

static void UC1601SGPIOLowerpReadDeassertComplete(IOSTATUS Status, IO_OPERATION Operation, unsigned int BytesTransferred, void *Argument) {
    (void) BytesTransferred;

    UC1601S_GPIO_LOWER_DEVICE_DATA *Data = Argument;
    
    IoRelease(Operation);

    if(FAILURE(Status))
        IoDrvOperationQueueComplete(&Data->Queue, Status);
    else {
        IoDrvSubmit(Data->GPIO, GPIO_READ, &Data->Request, sizeof(Data->Request), &Data->Operation,
                    UC1601SGPIOLowerpReadReadSubmitComplete, Data);           
    }
}

static void UC1601SGPIOLowerpReadDeassertSubmitComplete(IOSTATUS Status, void *Argument) {
    UC1601S_GPIO_LOWER_DEVICE_DATA *Data = Argument;
    
    if(FAILURE(Status))
        IoDrvOperationQueueComplete(&Data->Queue, Status);
    else
        IoDrvSubscribe(Data->Operation, UC1601SGPIOLowerpReadDeassertComplete, Data);     
}

static void UC1601SGPIOLowerpBeginReadCycle(UC1601S_GPIO_LOWER_DEVICE_DATA *Data) {
    Data->Request.Lines = Data->AddressPins | Data->Config->RD | Data->Config->CS;
    IoDrvSubmit(Data->GPIO, GPIO_DEASSERT, &Data->Request, sizeof(Data->Request), &Data->Operation,
                UC1601SGPIOLowerpReadDeassertSubmitComplete, Data);        
}

static void UC1601SGPIOLowerpRead(UC1601S_GPIO_LOWER_DEVICE_DATA *Data, unsigned int AddressPins) {
    Data->AddressPins = AddressPins;
    Data->Position = 0;

    UC1601SGPIOLowerpBeginReadCycle(Data);
}

static void UC1601SGPIOLowerpExecute(CORE_IO_OPERATION Operation) {
    unsigned int Request = IoDrvRequest(Operation), Size = IoDrvSize(Operation);
    HANDLE Handle = IoDrvHandle(Operation);
    IO_DEVICE Device = IoDrvDeviceForHandle(Handle);
    UC1601S_GPIO_LOWER_DEVICE_DATA *Data = IoDrvDeviceData(Device);

    if((Request == UC1601S_LOWER_RESET && Size != 0) ||
       (Request != UC1601S_LOWER_RESET && Size == 0))
        return IoDrvOperationQueueComplete(&Data->Queue, IOSTATUS_INVALID_BUFFER);

    switch(IoDrvRequest(Operation)) {
    case UC1601S_LOWER_RESET:
        UC1601SGPIOLowerpReset(Data);
        break;

    case UC1601S_LOWER_WRITE_COMMAND:
        UC1601SGPIOLowerpWrite(Data, Data->Config->CD);
        break;

    case UC1601S_LOWER_READ_STATUS:
        UC1601SGPIOLowerpRead(Data, Data->Config->CD);
        break;

    case UC1601S_LOWER_WRITE_DATA:
        UC1601SGPIOLowerpWrite(Data, 0);
        break;

    case UC1601S_LOWER_READ_DATA:
        UC1601SGPIOLowerpRead(Data, 0);
        break;

    default:
        IoDrvOperationQueueComplete(&Data->Queue, IOSTATUS_UNSUPPORTED_REQUEST);

        break;
    }
}

static void UC1601SGPIOLowerpAbort(CORE_IO_OPERATION Operation) {
    (void) Operation;
}

const IO_DRIVER UC1601SGPIOLower = {
    .DeviceDataSize    = sizeof(UC1601S_GPIO_LOWER_DEVICE_DATA),
    .HandleDataSize    = 0,
    .OperationDataSize = sizeof(IO_DRV_OPERATION_QUEUE_HANDLE_DATA),

    .Attach    = UC1601SGPIOLowerpAttach,
    .Detach    = UC1601SGPIOLowerpDetach,

    .Open      = UC1601SGPIOLowerpOpen,
    .Duplicate = UC1601SGPIOLowerpDuplicate,
    .Close     = UC1601SGPIOLowerpClose,

    .Submit    = UC1601SGPIOLowerpSubmit,
    .Cancel    = UC1601SGPIOLowerpCancel
};
