#include <stdlib.h>

#include "io_p.h"

void IoDrvSubmit(HANDLE Handle, unsigned int Request, void *Buffer, size_t BufferSize,
                 IO_OPERATION *OutOperation, IO_DRV_COMPLETION_CALLBACK Callback, void *CallbackArg) {

    if(Handle->Operations == HANDLE_OPENING || Handle->Operations == HANDLE_CLOSING)
        return Callback(IOSTATUS_BUSY, CallbackArg);

    IOP_CORE_IO_OPERATION *Core = malloc(sizeof(IOP_CORE_IO_OPERATION) + Handle->Device->Driver->OperationDataSize);
    if(Core == NULL)
        return Callback(IOSTATUS_OUT_OF_MEMORY, CallbackArg);

    IOP_USER_IO_OPERATION *User = malloc(sizeof(IOP_USER_IO_OPERATION));
    if(User == NULL) {
        free(Core);
        return Callback(IOSTATUS_OUT_OF_MEMORY, CallbackArg);
    }

    IOP_DRV_OPERATION_DATA *Data = malloc(sizeof(IOP_DRV_OPERATION_DATA));
    if(Data == NULL) {
        free(User);
        free(Core);

        return Callback(IOSTATUS_OUT_OF_MEMORY, CallbackArg);   
    }

    Core->User = User;
    Core->Handle = Handle;
    Core->Request = Request;
    Core->Buffer = Buffer;
    Core->Size = BufferSize;

    User->DeferredProc.Handler = NULL;
    User->Core = Core;
    User->Status = IOSTATUS_PENDING;
    User->BytesTransferred = 0;
    User->Callback = NULL;
    User->CallbackArg = NULL;

    Data->Operation = Core;
    Data->Callback = Callback;
    Data->CallbackArg = CallbackArg;

    *OutOperation = User;

    Handle->Operations += 1;

    return Handle->Device->Driver->Submit(Core, IopDrvSubmitComplete, Data);
}

void IopDrvSubmitComplete(IOSTATUS Status, void *Argument) {
    IOP_DRV_OPERATION_DATA *Data = Argument;
    IO_DRV_COMPLETION_CALLBACK NestedCallback = Data->Callback;
    void *NestedArgument = Data->CallbackArg;
    IOP_CORE_IO_OPERATION *Operation = Data->Operation;
    free(Data);

    if(FAILURE(Status)) {
        free(Operation->User);
        Operation->Handle->Operations -= 1;
        free(Operation);
    }

    return NestedCallback(Status, NestedArgument);
}

void IoDrvCancel(IO_OPERATION Operation, IO_DRV_COMPLETION_CALLBACK Callback, void *CallbackArg) {
    return Operation->Core->Handle->Device->Driver->Cancel(Operation->Core, Callback, CallbackArg);    
}

void IoDrvSubscribe(IO_OPERATION Operation, IO_OPERATION_COMPLETION_CALLBACK Callback, void *CallbackArg) {
    Operation->Callback = Callback;
    Operation->CallbackArg = CallbackArg;

    if(Operation->Status != IOSTATUS_PENDING && Operation->Callback != NULL && Operation->DeferredProc.Handler == NULL)
        IopInvokeCallback(Operation);
}

void IopInvokeCallback(IO_OPERATION Operation) {
    Operation->DeferredProc.Handler = IopInvokeDeferred;
    DispatchDefer(&Operation->DeferredProc);
}

void IopInvokeDeferred(DISPATCH_DEFERRED_PROC *Proc) {
    IO_OPERATION Operation = (IO_OPERATION) Proc;
    Operation->Callback(Operation->Status, Operation, Operation->BytesTransferred, Operation->CallbackArg);
}

void IoDrvAddTransferred(CORE_IO_OPERATION Operation, unsigned int Bytes) {
    if(Operation->User)
        Operation->User->BytesTransferred += Bytes;
}

void IoDrvComplete(CORE_IO_OPERATION Operation, IOSTATUS Status) {
    Operation->Handle->Operations -= 1;

    IO_OPERATION User = Operation->User;
    free(Operation);

    if(User) {
        User->Status = Status;
        if(User->Callback && User->DeferredProc.Handler == NULL)
            IopInvokeCallback(User);
    }
}

HANDLE IoDrvHandle(CORE_IO_OPERATION Operation) {
    return Operation->Handle;
}

unsigned int IoDrvRequest(CORE_IO_OPERATION Operation) {
    return Operation->Request;
}

void *IoDrvBuffer(CORE_IO_OPERATION Operation) {
    return Operation->Buffer;
}

size_t IoDrvSize(CORE_IO_OPERATION Operation) {
    return Operation->Size;
}
