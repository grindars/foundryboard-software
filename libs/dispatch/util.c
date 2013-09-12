#include "io_p.h"

#include <stdlib.h>

IOSTATUS IoBringUp(const IO_BOARD *Board) {
    IOSTATUS Status = IOSTATUS_SUCCESS;

    const IO_BOARD_DEVICE *Device = Board->Devices;

    while(Device->Name) {
        Status = IoAttach(Device->Name, Device->Driver, Device->Data);
        if(FAILURE(Status))
            break;

        Device++;
    }

    return Status;
}

IOSTATUS IoExecute(HANDLE Handle, unsigned int Request, void *Buffer, size_t BufferSize, unsigned int *BytesTransferred) {
    IOSTATUS Status;
    IO_OPERATION Operation;

    Status = IoSubmit(Handle, Request, Buffer, BufferSize, &Operation);
    if(FAILURE(Status))
        return Status;

    Status = IoSynchronize(&Operation, 1, TIMEOUT_INFINITE, NULL);
    if(FAILURE(Status)) {
        IoCancel(Operation);
        IoRelease(Operation);

        return Status;
    }

    Status = IoGetResult(Operation, BytesTransferred);
    IoRelease(Operation);

    return Status;
}

void IopSynchronizeCompleted(IOSTATUS Status, IO_OPERATION Operation, unsigned int BytesTransferred, void *Argument) {
    (void) Status;
    (void) Operation;
    (void) BytesTransferred;

    IOP_SYNCHRONIZE_DATA *SynchronizeData = (IOP_SYNCHRONIZE_DATA *) Argument;
    if(SynchronizeData->TimeElapsed)
        *SynchronizeData->TimeElapsed = TimerCurrentTime() - SynchronizeData->Started;
    SynchronizeData->Signaled = 2;
}

void IopSynchronizeTimeout(TIMER *Timer, TIME TimeElapsed, void *Argument) {
    (void) Timer;

    IOP_SYNCHRONIZE_DATA *SynchronizeData = (IOP_SYNCHRONIZE_DATA *) Argument;
    if(SynchronizeData->Signaled == 0) {
        if(SynchronizeData->TimeElapsed)
            *SynchronizeData->TimeElapsed = TimeElapsed;
        
        SynchronizeData->Signaled = 1;
    }
}


char *IopCopyString(const char *String, size_t *StringLength) {
    if(String == NULL)
        return NULL;

    size_t Length = strlen(String) + 1;
    if(StringLength)
        *StringLength = Length;

    char *Buffer = malloc(Length);
    if(Buffer == NULL)
        return NULL;

    memcpy(Buffer, String, Length);

    return Buffer;
}

void IoHaltWithStatus(IOSTATUS Status) {
    (void) Status;
    while(1);
}
