#ifndef MALLOCULE_H
#define MALLOCULE_H

#include <stdlib.h>

// TODO:
// - Split block on allocate if it's too large.
// - Add mol_realloc()
// - Add multithreading support
// - Use mmap for large allocations
// - canaries and guard pages

typedef struct molecule_t {
    size_t size;
    unsigned is_free;
    struct molecule_t* next;
    struct molecule_t* prev;
} molecule_t;

void* mol_alloc(size_t size);
void mol_free(void* ptr);

#ifdef MALLOCULE_IMPL

#include <unistd.h>

#define ALIGNMENT 8
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT-1))

static molecule_t* head = NULL;
static molecule_t* tail = NULL;

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
    new_block->prev = tail;

    if (head == NULL) {
        head = new_block;
    } else {
        tail->next = new_block;
    }
    tail = new_block;

    return (void*)(new_block + 1);
}

void mol_free(void *ptr) {
    if (ptr == NULL) return;
    molecule_t* block = (molecule_t*)ptr - 1;
    block->is_free = 1;

    // Coalesce backward with any adjacent free blocks
    while (block->prev && block->prev->is_free) {
        block->prev->size += sizeof(molecule_t) + block->size;
        block->prev->next = block->next;
        if (block->next) {
            block->next->prev = block->prev;
        }
        if (block == tail) {
            tail = block->prev;
        }
        block = block->prev;
    }

    // Coalesce forward with any adjacent free blocks
    while (block->next && block->next->is_free) {
        block->size += sizeof(molecule_t) + block->next->size;
        if (block->next == tail) {
            tail = block;
        }
        block->next = block->next->next;
        if (block->next) {
            block->next->prev = block;
        }
    }
}

#endif // MALLOCULE_IMPL
#endif // MALLOCULE_H
