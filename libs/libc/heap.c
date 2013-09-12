#include <stdlib.h>
#include <string.h>

typedef struct heap_block heap_block_t;

struct heap_block {
    heap_block_t *next;
    size_t size;
};

static heap_block_t free_list = {
    .next = &free_list,
    .size = 0
};

static const heap_locking_callbacks_t *lock_callbacks;
static void *lock_arg;

static void heap_lock(void) {
    if(lock_callbacks)
        lock_callbacks->lock(lock_arg);
}

static void heap_unlock(void) {
    if(lock_callbacks)
        lock_callbacks->unlock(lock_arg);
}

void heap_initialize_locking(const heap_locking_callbacks_t *callbacks, void *arg) {
    lock_callbacks = callbacks;
    lock_arg = arg;
}

void heap_seed(void *block, size_t size) {
    heap_block_t *header = block;
    header->size = size;
    free(header + 1);
}

void *malloc(size_t bytes) {
    if(bytes == 0)
        return NULL;

    heap_lock();

    heap_block_t *allocated = NULL;

    size_t alignment = sizeof(heap_block_t) - 1;
    bytes = (bytes + sizeof(heap_block_t) + alignment) & ~alignment;

    for(heap_block_t *p = &free_list, *i = p->next; i != &free_list; p = i, i = i->next) {
        if(i->size >= bytes) {
            allocated = i;

            if(i->size > bytes) {
                heap_block_t *rest = (heap_block_t *)((char *) i + bytes);
                rest->next = i->next;
                rest->size = i->size - bytes;
                p->next = rest;

                allocated->size = bytes;
            }

            break;
        }      
    } 

    heap_unlock();

    if(allocated == NULL)
        return NULL;
    else
        return allocated + 1;
}

void free(void *block) {
    if(block == NULL)
        return ;
    
    heap_lock();

    heap_block_t *header = (heap_block_t *) block - 1;
    heap_block_t *insert_after = &free_list;

    for(heap_block_t *i = free_list.next; i != &free_list; i = i->next) {
        if(i >= header)
            break;

        insert_after = i;
    }

    header->next = insert_after->next;
    insert_after->next = header;

    if(header->next->size != 0 && (char *) header + header->size == (char *) header->next) {
        header->size += header->next->size;
        header->next  = header->next->next;
    }

    if(insert_after->size != 0 && (char *) insert_after + insert_after->size == (char *) header) {
        insert_after->next  = header->next;
        insert_after->size += header->size;
    }

    heap_unlock();
}

void *realloc(void *block, size_t bytes) {
    void *new_block = NULL;

    if(bytes > 0) {
        new_block = malloc(bytes);

        if(new_block == NULL)
            return new_block;
    }

    if(block != NULL && new_block != NULL) {
        heap_block_t *oldheader = (heap_block_t *) block - 1;
        heap_block_t *newheader = (heap_block_t *) new_block - 1;
        size_t minsize;  

        if(oldheader->size < newheader->size)
            minsize = oldheader->size;
        else
            minsize = newheader->size;

        memcpy(new_block, block, minsize - sizeof(heap_block_t));
    }

    if(block != NULL)
        free(block);

    return new_block;
}

void *calloc(size_t count, size_t size) {
    void *block = malloc(count * size);
    
    if(block != NULL) {
        memset(block, 0, count * size);
    }

    return block;
}
