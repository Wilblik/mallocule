#ifndef MALLOCULE_H
#define MALLOCULE_H

#include <stdlib.h>

// TODO:
// - Add mol_realloc()
// - Add multithreading support
// - Use mmap for large allocations
// - Bins for small allocations
// - Canaries and guard pages
// - Bit packing for is_free (?)

typedef struct molecule_t {
    size_t size;
    unsigned is_free;
    struct molecule_t* next;
    struct molecule_t* prev;
} molecule_t;

void* mol_alloc(size_t size);
void mol_free(void* ptr);

#ifdef MALLOCULE_DEBUG
void print_heap_state();
#define DEBUG_PRINT_HEAP() print_heap_state()
#else
#define DEBUG_PRINT_HEAP() (void)0
#endif

#ifdef MALLOCULE_IMPL

#include <unistd.h>

#define ALIGNMENT 8
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT-1))

static molecule_t* head = NULL;
static molecule_t* tail = NULL;

#ifdef MALLOCULE_DEBUG
void print_heap_state() {
    molecule_t* curr = head;
    if (curr == NULL) {
        printf("Heap is empty.\n");
        return;
    }
    printf("--- Heap State ---\n");
    printf("HEAD -> ");
    while (curr != NULL) {
        printf("[%s: %zu bytes @ %p]", curr->is_free ? "FREE" : "USED", curr->size, (void*)curr);
        if (curr->next != NULL) {
            printf(" <=> ");
        }
        curr = curr->next;
    }
    printf(" <- TAIL\n------------------\n");
}
#endif

static void split_block(molecule_t* block, size_t size) {
    size_t min_block_size = ALIGN(sizeof(molecule_t)) + ALIGN(1);

    if (block->size > size + min_block_size) {
        molecule_t* new_free_block = (molecule_t*)((char*)block + ALIGN(sizeof(molecule_t)) + size);
        new_free_block->size = block->size - size - ALIGN(sizeof(molecule_t));
        new_free_block->is_free = 1;
        new_free_block->next = block->next;
        new_free_block->prev = block;

        block->size = size;
        block->next = new_free_block;

        if (new_free_block->next) {
            new_free_block->next->prev = new_free_block;
        } else {
            tail = new_free_block;
        }
    }
}

void* mol_alloc(size_t size) {
    if (size == 0) return NULL;

    size_t requested_size = ALIGN(size);
    molecule_t* curr = head;
    while (curr != NULL) {
        if (curr->is_free && curr->size >= requested_size){
            curr->is_free = 0;
            split_block(curr, requested_size);
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

static void coalesce(molecule_t* block) {
    // Coalesce backward with any adjacent free blocks
    while (block->prev && block->prev->is_free) {
        block->prev->size += ALIGN(sizeof(molecule_t)) + block->size;
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
        block->size += ALIGN(sizeof(molecule_t)) + block->next->size;
        if (block->next == tail) {
            tail = block;
        }
        block->next = block->next->next;
        if (block->next) {
            block->next->prev = block;
        }
    }
}

void mol_free(void *ptr) {
    if (ptr == NULL) return;
    molecule_t* block = (molecule_t*)ptr - 1;
    block->is_free = 1;
    coalesce(block);
}

#endif // MALLOCULE_IMPL
#endif // MALLOCULE_H
