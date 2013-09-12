#ifndef __IO_H__
#define __IO_H__

#include <string.h>
#include <timer.h>

#define TIMEOUT_INFINITE    0xFFFFFFFFFFFFFFFFULL

#define IO_CLASS_GPIO        0x0000
#define IO_CLASS_USER_FIRST  0x8000
#define IO_CLASS_USER_LAST   0xFFFF

#define IO_CLASS_REQUEST(Class,Request) (((Class) << 16) | (Request))

typedef struct IOP_HANDLE_DATA IOP_HANDLE_DATA, *HANDLE;
typedef struct IOP_USER_IO_OPERATION IOP_USER_IO_OPERATION, *IO_OPERATION;
typedef struct IOP_CORE_IO_OPERATION IOP_CORE_IO_OPERATION, *CORE_IO_OPERATION;
typedef struct IOP_DEVICE IOP_DEVICE, *IO_DEVICE;

typedef enum IOSTATUS {
    IOSTATUS_SUCCESS             =  0,
    IOSTATUS_PENDING             = -1,
    IOSTATUS_OUT_OF_MEMORY       = -2,
    IOSTATUS_TIMEOUT             = -3,
    IOSTATUS_INVALID_NAME        = -4,
    IOSTATUS_DEVICE_NOT_FOUND    = -5,
    IOSTATUS_BUSY                = -6,
    IOSTATUS_INVALID_BUFFER      = -7,
    IOSTATUS_UNSUPPORTED_REQUEST = -8
} IOSTATUS;

typedef void (*IO_DRV_COMPLETION_CALLBACK)(IOSTATUS Status, void *Argument);
typedef void (*IO_OPERATION_COMPLETION_CALLBACK)(IOSTATUS Status, IO_OPERATION Operation, unsigned int BytesTransferred, void *Argument);

#define FAILURE(Status) ((int)(Status) < 0)
#define SUCCESS(Status) (!FAILURE(Status)))

typedef struct IO_DRIVER {
    void *DriverParameter;
    size_t DeviceDataSize;
    size_t HandleDataSize;
    size_t OperationDataSize;

    void (*Attach)(IO_DEVICE Device, const void *PlatformData, IO_DRV_COMPLETION_CALLBACK Callback, void *Argument);
    void (*Detach)(IO_DEVICE Device, IO_DRV_COMPLETION_CALLBACK Callback, void *Argument);

    void (*Open)(const char *Filename, unsigned int Mode, HANDLE Handle, IO_DRV_COMPLETION_CALLBACK Callback, void *Argument);
    void (*Duplicate)(HANDLE Handle, HANDLE NewHandle, IO_DRV_COMPLETION_CALLBACK Callback, void *Argument);
    void (*Close)(HANDLE Handle, IO_DRV_COMPLETION_CALLBACK Callback, void *Argument);

    void (*Submit)(CORE_IO_OPERATION Operation, IO_DRV_COMPLETION_CALLBACK Callback, void *Argument);
    void (*Cancel)(CORE_IO_OPERATION Operation, IO_DRV_COMPLETION_CALLBACK Callback, void *Argument);
} IO_DRIVER;

typedef struct IO_BOARD_DEVICE {
    const IO_DRIVER *Driver;
    const char *Name;
    const void *Data;
} IO_BOARD_DEVICE;

#define BOARD_DEVICE(aDriver,aName,aData) { .Driver = aDriver, .Name = aName, .Data = aData }
#define BOARD_END BOARD_DEVICE(NULL, NULL, NULL)

typedef struct IO_BOARD {
    unsigned char Dummy;

    IO_BOARD_DEVICE Devices[];
} IO_BOARD;

LIBC_BEGIN_PROTOTYPES

void IoInit(void);

#pragma GCC visibility push(default)

/* Application API */

IOSTATUS IoOpenFile(const char *Filename, unsigned int Mode, HANDLE *Handle);
IOSTATUS IoDuplicateHandle(HANDLE Handle, HANDLE *NewHandle);
IOSTATUS IoCloseHandle(HANDLE Handle);

IOSTATUS IoSubmit(HANDLE Handle, unsigned int Request, void *Buffer, size_t BufferSize, IO_OPERATION *Operation);
IOSTATUS IoCancel(IO_OPERATION Operation);
IOSTATUS IoGetResult(IO_OPERATION Operation, unsigned int *BytesTransferred);
IOSTATUS IoSynchronize(IO_OPERATION *Operations, size_t Count, TIME Timeout, TIME *TimeElapsed);
IOSTATUS IoRelease(IO_OPERATION Operation);

IOSTATUS IoAttach(const char *Name, const IO_DRIVER *Driver, const void *PlatformData);
IOSTATUS IoDetach(const char *Name);

/* Driver API */

void IoDrvOpenFile(const char *Filename, unsigned int Mode, HANDLE *Handle, IO_DRV_COMPLETION_CALLBACK Callback, void *CallbackArg);
void IoDrvDuplicateHandle(HANDLE Handle, HANDLE *NewHandle, IO_DRV_COMPLETION_CALLBACK Callback, void *CallbackArg);
void IoDrvCloseHandle(HANDLE Handle, IO_DRV_COMPLETION_CALLBACK Callback, void *CallbackArg);

void IoDrvSubmit(HANDLE Handle, unsigned int Request, void *Buffer, size_t BufferSize, IO_OPERATION *Operation, IO_DRV_COMPLETION_CALLBACK Callback, void *CallbackArg);
void IoDrvCancel(IO_OPERATION Operation, IO_DRV_COMPLETION_CALLBACK Callback, void *CallbackArg);
void IoDrvSubscribe(IO_OPERATION Operation, IO_OPERATION_COMPLETION_CALLBACK Callback, void *CallbackArg);

void IoDrvAttach(const char *Name, const IO_DRIVER *Driver, const void *PlatformData, IO_DRV_COMPLETION_CALLBACK Callback, void *CallbackArg);
void IoDrvDetach(const char *Name, IO_DRV_COMPLETION_CALLBACK Callback, void *CallbackArg);

void *IoDrvDeviceData(IO_DEVICE Device);
IO_DEVICE IoDrvDeviceForHandle(HANDLE Handle);
void *IoDrvHandleData(HANDLE Handle);

void IoDrvAddTransferred(CORE_IO_OPERATION Operation, unsigned int Bytes);
void IoDrvComplete(CORE_IO_OPERATION Operation, IOSTATUS Status);
HANDLE IoDrvHandle(CORE_IO_OPERATION Operation);
unsigned int IoDrvRequest(CORE_IO_OPERATION Operation);
void *IoDrvBuffer(CORE_IO_OPERATION Operation);
size_t IoDrvSize(CORE_IO_OPERATION Operation);

/* Utility API */
IOSTATUS IoBringUp(const IO_BOARD *Board);
IOSTATUS IoExecute(HANDLE Handle, unsigned int Operation, void *Buffer, size_t BufferSize, unsigned int *BytesTransferred);

void IoHaltWithStatus(IOSTATUS Status);

#pragma GCC visibility pop

LIBC_END_PROTOTYPES

#endif
