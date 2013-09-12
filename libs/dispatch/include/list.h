#ifndef __LIST__H__
#define __LIST__H__

#include <stddef.h>

typedef struct LIST_HEAD {
    struct LIST_HEAD *Next;
    struct LIST_HEAD *Prev;
} LIST_HEAD;


#if defined(__cplusplus)
extern "C" {
#endif

#pragma GCC visibility push(default)

static inline void ListInit(LIST_HEAD *Head) {
    Head->Next = Head;
    Head->Prev = Head;
}

static inline void ListAppend(LIST_HEAD *List, LIST_HEAD *Entry) {
    Entry->Next = List->Next;
    Entry->Prev = List;
    Entry->Next->Prev = Entry;
    List->Next = Entry;
}

static inline void ListPrepend(LIST_HEAD *List, LIST_HEAD *Entry) {
    Entry->Prev = List->Prev;
    Entry->Next = List;
    Entry->Prev->Next = Entry;
    List->Prev = Entry;
}

static inline void ListRemove(LIST_HEAD *Entry) {
    Entry->Prev->Next = Entry->Next;
    Entry->Next->Prev = Entry->Prev;
}

static inline int ListEmpty(LIST_HEAD *Head) {
    return Head->Next == Head;
}

static inline LIST_HEAD *ListTakeFirst(LIST_HEAD *Head) {
    LIST_HEAD *Entry = Head->Next;
    ListRemove(Entry);

    return Entry;
}

static inline LIST_HEAD *ListTakeLast(LIST_HEAD *Head) {
    LIST_HEAD *Entry = Head->Prev;
    ListRemove(Entry);

    return Entry;
}

#define LIST_DATA(Head,Type,Field) ((Type *)((char *) (Head) - offsetof(Type, Field)))

#pragma GCC visibility pop

#if defined(__cplusplus)
}
#endif


#endif
