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

#define ALIGNMENT 8
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT-1))

molecule_t* head = NULL;

// TODO Consider splitting the block if it's too large.
void* mol_alloc(size_t size) {
    if (size == 0) return NULL;

    size_t requested_size = ALIGN(size);
    molecule_t* curr = head;
    while (curr != NULL) {
        if (curr->is_free && curr->size >= requested_size){
            curr->is_free = 0;
            return (void*)(curr + 1);
        }
        curr = curr->next;
    }

    size_t total_block_size = ALIGN(sizeof(molecule_t)) + requested_size;
    molecule_t* new_block = sbrk(total_block_size);
    if (new_block == (void*) -1) {
       return NULL;
    }

    new_block->is_free = 0;
    new_block->size = requested_size,
    new_block->next = NULL;

    if (head == NULL) {
        head = new_block;
    } else {
        curr = head;
        while (curr->next != NULL) {
            curr = curr->next;
        }
        curr->next = new_block;
    }

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
