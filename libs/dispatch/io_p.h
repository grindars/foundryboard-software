#ifndef __IO__P__H__
#define __IO__P__H__

#include <io.h>

#define HANDLE_OPENING  ((unsigned int) -2)
#define HANDLE_CLOSING  ((unsigned int) -1)

struct IOP_HANDLE_DATA {
    IOP_DEVICE *Device;
    unsigned int Operations;

    unsigned char DriverData[];
};

typedef struct IOP_SYNC_CALL {
    volatile IOSTATUS Status;
    unsigned int Saved;
} IOP_SYNC_CALL;

typedef struct IOP_SYNCHRONIZE_DATA {
    volatile unsigned int Signaled;
    TIME *TimeElapsed;
    TIME Started;
} IOP_SYNCHRONIZE_DATA;

struct IOP_CORE_IO_OPERATION {
    IO_OPERATION User;
    HANDLE Handle;
    unsigned int Request;
    void *Buffer;
    size_t Size;

    unsigned char DriverData[];
};

struct IOP_USER_IO_OPERATION {
    IOP_CORE_IO_OPERATION *Core;
    IOSTATUS Status;
    unsigned int BytesTransferred;
    IO_OPERATION_COMPLETION_CALLBACK Callback;
    void *CallbackArg;
};

struct IOP_DEVICE {
    LIST_HEAD Head;
    char *Name;
    size_t NameLength;
    unsigned int OpenHandles;
    const IO_DRIVER *Driver;
    unsigned char DriverData[];
};

typedef struct IOP_DRV_ATTACH_DETACH_DATA {
    IOP_DEVICE *Device;
    IO_DRV_COMPLETION_CALLBACK Callback;
    void *CallbackArg;
} IOP_DRV_ATTACH_DETACH_DATA;

typedef struct IOP_DRV_OPEN_CLOSE_DATA {
    IOP_HANDLE_DATA *Handle;
    IO_DRV_COMPLETION_CALLBACK Callback;
    void *CallbackArg;
} IOP_DRV_OPEN_CLOSE_DATA;

typedef struct IOP_DRV_DUPLICATE_DATA {
    IOP_HANDLE_DATA *Handle;
    IOP_HANDLE_DATA *NewHandle;
    IO_DRV_COMPLETION_CALLBACK Callback;
    void *CallbackArg;
} IOP_DRV_DUPLICATE_DATA;

typedef struct IOP_DRV_OPERATION_DATA {
    IOP_CORE_IO_OPERATION *Operation;
    IO_DRV_COMPLETION_CALLBACK Callback;
    void *CallbackArg;
} IOP_DRV_OPERATION_DATA;

extern LIST_HEAD IopDevices;

void IopBeginSyncCall(IOP_SYNC_CALL *Sync);
IOSTATUS IopEndSyncCall(IOP_SYNC_CALL *Sync);
void IopCompleteSyncCall(IOSTATUS Status, void *Arg);

void IopSynchronizeCompleted(IOSTATUS Status, IO_OPERATION Operation, unsigned int BytesTransferred, void *Argument);
void IopSynchronizeTimeout(TIMER *Timer, TIME TimeElapsed, void *Argument);

char *IopCopyString(const char *String, size_t *StringLength);

int IopValidDeviceName(const char *Name);
int IopFindDevice(const char *Name, const char **Extension, IOP_DEVICE **Device);

void IopDrvAttachComplete(IOSTATUS Status, void *Argument);
void IopDrvDetachComplete(IOSTATUS Status, void *Argument);

void IopDrvOpenFileComplete(IOSTATUS Status, void *Argument);
void IopDrvDuplicateHandleComplete(IOSTATUS Status, void *Argument);
void IopDrvCloseHandleComplete(IOSTATUS Status, void *Argument);

void IopDrvSubmitComplete(IOSTATUS Status, void *Argument);

#endif

