#include <dispatch.h>
#include <stdlib.h>

static LIST_HEAD DispatchpDeferredQueue;

extern unsigned char g_pfnVectors, _sheap, _eheap;
static unsigned int DispatchpHeapLock;

static void DispatchpLockHeap(void *Arg) {
    (void) Arg;

    DispatchpHeapLock = DispatchEnterSystem();
}

static void DispatchpUnlockHeap(void *Arg) {
    (void) Arg;

    DispatchLeaveSystem(DispatchpHeapLock);
}

static const heap_locking_callbacks_t DispatchpHeapLocks = {
    .lock = DispatchpLockHeap,
    .unlock = DispatchpUnlockHeap
};

void DispatchInit(void) {
    SCB->VTOR = (unsigned int) &g_pfnVectors;

    heap_seed(&_sheap, &_eheap - &_sheap);
    heap_initialize_locking(&DispatchpHeapLocks, NULL);

    // Configure PendSV to lowest priority
    NVIC_SetPriority(PendSV_IRQn, DISPATCH_PENDSV_PRIORITY);
    ListInit(&DispatchpDeferredQueue);
}

void DispatchDefer(DISPATCH_DEFERRED_PROC *Proc) {
    unsigned int Interrupts = DispatchDisableInterrupts();

    ListAppend(&DispatchpDeferredQueue, &Proc->Head);

    DispatchEnableInterrupts(Interrupts);

    SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
    __DSB();
}

void PendSV_Handler(void) {
    while(1) {
        __disable_irq();

        if(ListEmpty(&DispatchpDeferredQueue)) {
            __enable_irq();

            break;
        }

        LIST_HEAD *Entry = ListTakeFirst(&DispatchpDeferredQueue);

        __enable_irq();

        DISPATCH_DEFERRED_PROC *Proc = LIST_DATA(Entry, DISPATCH_DEFERRED_PROC, Head);
        Proc->Handler(Proc);
    }
}
