#ifndef MALLOCULE_H
#define MALLOCULE_H

#include <stdlib.h>

typedef struct molecule_t {
    size_t size;
    unsigned is_free;
    struct molecule_t* next;
} molecule_t;

void* mol_alloc(size_t size);
void mol_free(void* ptr);

#ifdef MALLOCULE_IMPL

#include <unistd.h>

molecule_t* head = NULL;

// TODO:
// - Align the requested size.
// - Consider splitting the block if it's too large.
void* mol_alloc(size_t size) {
    if (size == 0) return NULL;

    molecule_t* curr = head;
    while (curr != NULL) {
        if (!curr->is_free || curr->size < size){
            curr = curr->next;
            continue;
        }
        curr->is_free = 0;
        return (void*)(curr + 1);
    }

    molecule_t* new_block = sbrk(sizeof(molecule_t) + size);
    if (new_block == (void*) -1) {
       return NULL;
    }

    new_block->is_free = 0;
    new_block->size = size,
    new_block->next = NULL;

    if (head == NULL) {
        head = new_block;
        return (void*)(new_block + 1);
    }

    curr = head;
    while (curr->next != NULL) {
        curr = curr->next;
    }
    curr->next = new_block;

    return (void*)(new_block + 1);
}

// TODO Coalesce with the next block if it's also free.
void mol_free(void *ptr) {
    if (ptr == NULL) return;
    molecule_t* block_header = (molecule_t*)ptr - 1;
    block_header->is_free = 1;
}

#endif // MALLOCULE_IMPL
#endif // MALLOCULE_H
