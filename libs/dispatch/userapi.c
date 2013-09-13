#include <dispatch.h>
#include <timer.h>
#include <stdlib.h>

#include "io_p.h"

void IoInit(void) {
    ListInit(&IopDevices);
}

IOSTATUS IoOpenFile(const char *Filename, unsigned int Mode, HANDLE *Handle) {
    IOP_SYNC_CALL Sync;

    IopBeginSyncCall(&Sync);

    IoDrvOpenFile(Filename, Mode, Handle, IopCompleteSyncCall, &Sync);

    return IopEndSyncCall(&Sync);
}

IOSTATUS IoDuplicateHandle(HANDLE Handle, HANDLE *NewHandle) {
    IOP_SYNC_CALL Sync;

    IopBeginSyncCall(&Sync);

    IoDrvDuplicateHandle(Handle, NewHandle, IopCompleteSyncCall, &Sync);

    return IopEndSyncCall(&Sync);
}

IOSTATUS IoCloseHandle(HANDLE Handle) {
    IOP_SYNC_CALL Sync;

    IopBeginSyncCall(&Sync);

    IoDrvCloseHandle(Handle, IopCompleteSyncCall, &Sync);

    return IopEndSyncCall(&Sync);
}

IOSTATUS IoSubmit(HANDLE Handle, unsigned int Request, void *Buffer, size_t BufferSize, IO_OPERATION *Operation) {
    IOP_SYNC_CALL Sync;

    IopBeginSyncCall(&Sync);

    IoDrvSubmit(Handle, Request, Buffer, BufferSize, Operation, IopCompleteSyncCall, &Sync);

    return IopEndSyncCall(&Sync);
}

IOSTATUS IoCancel(IO_OPERATION Operation) {
    IOP_SYNC_CALL Sync;

    IopBeginSyncCall(&Sync);

    IoDrvCancel(Operation, IopCompleteSyncCall, &Sync);

    return IopEndSyncCall(&Sync);
}

IOSTATUS IoGetResult(IO_OPERATION Operation, unsigned int *BytesTransferred) {
    unsigned int Saved = DispatchEnterSystem();

    if(BytesTransferred)
        *BytesTransferred = Operation->BytesTransferred;

    IOSTATUS Status = Operation->Status;

    DispatchLeaveSystem(Saved);

    return Status;
}

IOSTATUS IoSynchronize(IO_OPERATION *Operations, size_t Count, TIME Timeout, TIME *TimeElapsed) {
    IOP_SYNCHRONIZE_DATA SynchronizeData = { .Signaled = 0, .TimeElapsed = TimeElapsed, .Started = TimerCurrentTime() };
    TIMER Timer;

    unsigned int Locked = DispatchEnterSystem();

    for(unsigned int Operation = 0; Operation < Count; Operation++) {
        IoDrvSubscribe(Operations[Operation], IopSynchronizeCompleted, &SynchronizeData);
    }

    if(Timeout != TIMEOUT_INFINITE)
        TimerCreateSingleShot(&Timer, Timeout, IopSynchronizeTimeout, &SynchronizeData);

    DispatchLeaveSystem(Locked);

    while(SynchronizeData.Signaled == 0)
        DispatchIdle();

    DispatchEnterSystem(); 

    for(unsigned int Operation = 0; Operation < Count; Operation++) {
        IoDrvSubscribe(Operations[Operation], NULL, NULL);
    }       

    if(Timeout != TIMEOUT_INFINITE)
        TimerRelease(&Timer);

    DispatchLeaveSystem(Locked);

    if(SynchronizeData.Signaled == 1)
        return IOSTATUS_TIMEOUT;
    else
        return IOSTATUS_SUCCESS;    
}

IOSTATUS IoRelease(IO_OPERATION Operation) {
    unsigned int Saved = DispatchEnterSystem();

    if(Operation->DeferredProc.Handler) {
        DispatchAbandon(&Operation->DeferredProc);
    }

    Operation->Core->User = NULL;
    free(Operation);

    DispatchLeaveSystem(Saved);

    return IOSTATUS_SUCCESS;
}

IOSTATUS IoAttach(const char *Name, const IO_DRIVER *Driver, const void *PlatformData) {
    IOP_SYNC_CALL Sync;

    IopBeginSyncCall(&Sync);

    IoDrvAttach(Name, Driver, PlatformData, IopCompleteSyncCall, &Sync);

    return IopEndSyncCall(&Sync);
}

IOSTATUS IoDetach(const char *Name) {
    IOP_SYNC_CALL Sync;

    IopBeginSyncCall(&Sync);

    IoDrvDetach(Name, IopCompleteSyncCall, &Sync);

    return IopEndSyncCall(&Sync);
}

void IopBeginSyncCall(IOP_SYNC_CALL *Sync) {
    Sync->Status = IOSTATUS_PENDING;
    Sync->Saved = DispatchEnterSystem();
}

IOSTATUS IopEndSyncCall(IOP_SYNC_CALL *Sync) {
    DispatchLeaveSystem(Sync->Saved);

    while(Sync->Status == IOSTATUS_PENDING)
        DispatchIdle();

    return Sync->Status;
}

void IopCompleteSyncCall(IOSTATUS Status, void *Arg) {
    IOP_SYNC_CALL *Sync = (IOP_SYNC_CALL *) Arg;
    Sync->Status = Status;
}
