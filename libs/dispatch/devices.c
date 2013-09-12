#include <stdlib.h>

#include "io_p.h"

LIST_HEAD IopDevices;

void IoDrvAttach(const char *Name, const IO_DRIVER *Driver, const void *PlatformData, IO_DRV_COMPLETION_CALLBACK Callback, void *CallbackArg) {
    if(!IopValidDeviceName(Name))
        return Callback(IOSTATUS_INVALID_NAME, CallbackArg);

    size_t NameLength;
    char *NameCopy = IopCopyString(Name, &NameLength);
    if(NameCopy == NULL)
        return Callback(IOSTATUS_OUT_OF_MEMORY, CallbackArg);

    IOP_DEVICE *Device = malloc(sizeof(IOP_DEVICE) + Driver->DeviceDataSize);
    if(Device == NULL) {
        free(NameCopy);

        return Callback(IOSTATUS_OUT_OF_MEMORY, CallbackArg);
    }

    Device->Name = NameCopy;
    Device->NameLength = NameLength;
    Device->OpenHandles = 0;
    Device->Driver = Driver;

    IOP_DRV_ATTACH_DETACH_DATA *Data = malloc(sizeof(IOP_DRV_ATTACH_DETACH_DATA));
    if(Data == NULL) {
        free(Device);
        free(NameCopy);

        return Callback(IOSTATUS_OUT_OF_MEMORY, CallbackArg);
    }

    Data->Device = Device;
    Data->Callback = Callback;
    Data->CallbackArg = CallbackArg;

    Device->Driver->Attach(Device, PlatformData, IopDrvAttachComplete, Data);
}

void IoDrvDetach(const char *Name, IO_DRV_COMPLETION_CALLBACK Callback, void *CallbackArg) {
    IOP_DEVICE *Device;

    if(!IopValidDeviceName(Name))
        return Callback(IOSTATUS_INVALID_NAME, CallbackArg);

    if(!IopFindDevice(Name, NULL, &Device))
        return Callback(IOSTATUS_DEVICE_NOT_FOUND, CallbackArg);

    if(Device->OpenHandles != 0)
        return Callback(IOSTATUS_BUSY, CallbackArg);

    IOP_DRV_ATTACH_DETACH_DATA *Data = malloc(sizeof(IOP_DRV_ATTACH_DETACH_DATA));
    if(Data == NULL)
        return Callback(IOSTATUS_OUT_OF_MEMORY, CallbackArg);

    Data->Device = Device;
    Data->Callback = Callback;
    Data->CallbackArg = CallbackArg;

    ListRemove(&Device->Head);

    Device->Driver->Detach(Device, IopDrvDetachComplete, Data);
}

void IopDrvAttachComplete(IOSTATUS Status, void *Argument) {
    IOP_DRV_ATTACH_DETACH_DATA *Data = Argument;
    IO_DRV_COMPLETION_CALLBACK NestedCallback = Data->Callback;
    void *NestedArgument = Data->CallbackArg;
    IOP_DEVICE *Device = Data->Device;
    free(Data);

    if(FAILURE(Status)) {
        free(Device->Name);
        free(Device);
    } else {
        ListAppend(&IopDevices, &Device->Head);
    }

    return NestedCallback(Status, NestedArgument);
}

void IopDrvDetachComplete(IOSTATUS Status, void *Argument) {
    IOP_DRV_ATTACH_DETACH_DATA *Data = Argument;
    IO_DRV_COMPLETION_CALLBACK NestedCallback = Data->Callback;
    void *NestedArgument = Data->CallbackArg;
    IOP_DEVICE *Device = Data->Device;
    free(Data);

    if(FAILURE(Status)) {
        ListAppend(&IopDevices, &Device->Head);
    } else {
        free(Device->Name);
        free(Device);
    }

    return NestedCallback(Status, NestedArgument);
}

int IopValidDeviceName(const char *Name) {
    if(Name == NULL)
        return 0;

    char *Separator = strchr(Name, ':');
    return Separator != NULL && Separator[1] == 0;
}

int IopFindDevice(const char *Name,  const char **Extension, IOP_DEVICE **OutDevice) {
    char *Separator = strchr(Name, ':');
    if(Separator == NULL)
        return 0;

    if(Extension)
        *Extension = Separator + 1;

    size_t Length = Separator - Name;

    for(LIST_HEAD *Entry = IopDevices.Next; Entry != &IopDevices; Entry = Entry->Next) {
        IOP_DEVICE *Device = LIST_DATA(Entry, IOP_DEVICE, Head);

        if(Device->NameLength == Length + 2 && memcmp(Device->Name, Name, Length) == 0) {
            *OutDevice = Device;

            return 1; 
        }
    }

    return 0;
}

void *IoDrvDeviceData(IO_DEVICE Device) {
    return Device->DriverData;
}
