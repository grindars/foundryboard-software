#include <stdlib.h>

#include "io_p.h"

void IoDrvOpenFile(const char *Filename, unsigned int Mode, HANDLE *OutHandle, IO_DRV_COMPLETION_CALLBACK Callback, void *CallbackArg) {
    const char *Extension;
    IOP_DEVICE *Device;

    if(!IopFindDevice(Filename, &Extension, &Device))
        return Callback(IOSTATUS_DEVICE_NOT_FOUND, CallbackArg);

    IOP_HANDLE_DATA *Handle = malloc(sizeof(IOP_HANDLE_DATA) + Device->Driver->HandleDataSize);
    if(Handle == NULL)
        return Callback(IOSTATUS_OUT_OF_MEMORY, CallbackArg);

    Handle->Device = Device;
    Handle->Operations = HANDLE_OPENING;

    IOP_DRV_OPEN_CLOSE_DATA *Data = malloc(sizeof(IOP_DRV_OPEN_CLOSE_DATA));
    if(Data == NULL) {
        free(Handle);

        return Callback(IOSTATUS_OUT_OF_MEMORY, CallbackArg);
    }

    Data->Handle = Handle;
    Data->Callback = Callback;
    Data->CallbackArg = CallbackArg;

    *OutHandle = Handle;

    Device->OpenHandles += 1;

    return Device->Driver->Open(Extension, Mode, Handle, IopDrvOpenFileComplete, Data);
}

void IopDrvOpenFileComplete(IOSTATUS Status, void *Argument) {
    IOP_DRV_OPEN_CLOSE_DATA *Data = Argument;
    IO_DRV_COMPLETION_CALLBACK NestedCallback = Data->Callback;
    void *NestedArgument = Data->CallbackArg;
    HANDLE Handle = Data->Handle;
    free(Data);

    if(FAILURE(Status)) {
        Handle->Device->OpenHandles -= 1;
        free(Handle);
    } else {
        Handle->Operations = 0;
    }

    return NestedCallback(Status, NestedArgument);
}

void IoDrvDuplicateHandle(HANDLE Handle, HANDLE *OutNewHandle, IO_DRV_COMPLETION_CALLBACK Callback, void *CallbackArg) {
    if(Handle->Operations == HANDLE_OPENING || Handle->Operations == HANDLE_CLOSING)
        return Callback(IOSTATUS_BUSY, CallbackArg);

    IOP_HANDLE_DATA *NewHandle = malloc(sizeof(IOP_HANDLE_DATA) + Handle->Device->Driver->HandleDataSize);
    if(NewHandle == NULL)
        return Callback(IOSTATUS_OUT_OF_MEMORY, CallbackArg);

    NewHandle->Device = Handle->Device;
    NewHandle->Operations = HANDLE_OPENING;

    *OutNewHandle = NewHandle;

    IOP_DRV_DUPLICATE_DATA *Data = malloc(sizeof(IOP_DRV_DUPLICATE_DATA));
    if(Data == NULL) {
        free(Handle);

        return Callback(IOSTATUS_OUT_OF_MEMORY, CallbackArg);
    }

    Handle->Operations += 1;

    Data->Handle = Handle;
    Data->NewHandle = NewHandle;
    Data->Callback = Callback;
    Data->CallbackArg = CallbackArg;
    Handle->Device->OpenHandles += 1;

    return Handle->Device->Driver->Duplicate(Handle, NewHandle, IopDrvDuplicateHandleComplete, Data);
}

void IopDrvDuplicateHandleComplete(IOSTATUS Status, void *Argument) {
    IOP_DRV_DUPLICATE_DATA *Data = Argument;
    IO_DRV_COMPLETION_CALLBACK NestedCallback = Data->Callback;
    void *NestedArgument = Data->CallbackArg;
    HANDLE Handle = Data->Handle;
    HANDLE NewHandle = Data->NewHandle;
    free(Data);

    Handle->Operations -= 1;

    if(FAILURE(Status)) {
        free(Handle);

        Handle->Device->OpenHandles -= 1;
    } else {
        NewHandle->Operations = 0;
    }

    return NestedCallback(Status, NestedArgument);
}

void IoDrvCloseHandle(HANDLE Handle, IO_DRV_COMPLETION_CALLBACK Callback, void *CallbackArg) {
    if(Handle->Operations != 0)
        return Callback(IOSTATUS_BUSY, CallbackArg);

    IOP_DRV_OPEN_CLOSE_DATA *Data = malloc(sizeof(IOP_DRV_OPEN_CLOSE_DATA));
    if(Data == NULL) {
        free(Handle);

        return Callback(IOSTATUS_OUT_OF_MEMORY, CallbackArg);
    }

    Data->Handle = Handle;
    Data->Callback = Callback;
    Data->CallbackArg = CallbackArg;
    
    Handle->Operations = HANDLE_CLOSING;

    return Handle->Device->Driver->Close(Handle, IopDrvCloseHandleComplete, Data);
}

void IopDrvCloseHandleComplete(IOSTATUS Status, void *Argument) {
    IOP_DRV_OPEN_CLOSE_DATA *Data = Argument;
    IO_DRV_COMPLETION_CALLBACK NestedCallback = Data->Callback;
    void *NestedArgument = Data->CallbackArg;
    HANDLE Handle = Data->Handle;
    free(Data);

    if(FAILURE(Status)) {
        Handle->Operations = 0;
    } else {
        Handle->Device->OpenHandles -= 1;
        free(Handle);
    }

    return NestedCallback(Status, NestedArgument);
}

IO_DEVICE IoDrvDeviceForHandle(HANDLE Handle) {
    return Handle->Device;
}

void *IoDrvHandleData(HANDLE Handle) {
    return Handle->DriverData;
}
