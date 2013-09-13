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

void IoDrvOperationQueueInitialize(IO_DRV_OPERATION_QUEUE *Queue, IO_DRV_QUEUE_OPERATION Execute, IO_DRV_QUEUE_OPERATION Abort) {
    ListInit(&Queue->Head);
    Queue->Execute = Execute;
    Queue->Abort = Abort;
}

void IoDrvOperationQueueSubmit(IO_DRV_OPERATION_QUEUE *Queue, CORE_IO_OPERATION Operation) {
    int WasEmpty = ListEmpty(&Queue->Head);
    IO_DRV_OPERATION_QUEUE_HANDLE_DATA *Data = IoDrvData(Operation);
    ListAppend(&Queue->Head, &Data->Head);

    if(WasEmpty)
        Queue->Execute(Operation);
}

void IoDrvOperationQueueCancel(IO_DRV_OPERATION_QUEUE *Queue, CORE_IO_OPERATION Operation) {
    IO_DRV_OPERATION_QUEUE_HANDLE_DATA *Data = IoDrvData(Operation);

    if(&Data->Head == Queue->Head.Next) {
        Queue->Abort(Operation);
    } else {
        ListRemove(&Data->Head);
        IoDrvComplete(Operation, IOSTATUS_CANCELLED);
    }
}

void IoDrvOperationQueueComplete(IO_DRV_OPERATION_QUEUE *Queue, IOSTATUS Status) {
    CORE_IO_OPERATION Operation = IoDrvOperationQueueCurrent(Queue);
    ListRemove(Queue->Head.Next);

    IoDrvComplete(Operation, Status);

    if(!ListEmpty(&Queue->Head)) {
        Queue->Execute(IoDrvOperationQueueCurrent(Queue));
    }
}

void *IoDrvData(CORE_IO_OPERATION Operation) {
    return &Operation->DriverData;
}

CORE_IO_OPERATION IoDrvOperationQueueCurrent(IO_DRV_OPERATION_QUEUE *Queue) {
    return (CORE_IO_OPERATION)((char *) Queue->Head.Next - (unsigned int) IoDrvData(NULL));
}
