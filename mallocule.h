#ifndef MALLOCULE_H
#define MALLOCULE_H

#include <stdlib.h>
#include <string.h>

// TODO:
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
void* mol_realloc(void* ptr, size_t size);
void mol_free(void* ptr);

#ifdef MALLOCULE_DEBUG
void mol_print_heap();
#define DEBUG_PRINT_HEAP() mol_print_heap()
#else
#define DEBUG_PRINT_HEAP() (void)0
#endif

#ifdef MALLOCULE_IMPL

#include <unistd.h>

#define ALIGNMENT 8
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1))

static molecule_t* head = NULL;
static molecule_t* tail = NULL;

static void split_block(molecule_t* block, size_t new_size);
static molecule_t* merge_free_blocks(molecule_t* block);

void* mol_alloc(size_t size) {
    if (size == 0) return NULL;
    size_t requested_size = ALIGN(size);

    /* Check if there is already a free block with reqeuested size */
    molecule_t* curr = head;
    while (curr != NULL) {
        if (curr->is_free && curr->size >= requested_size) {
            curr->is_free = 0;
            split_block(curr, requested_size);
            return (void*)(curr + 1);
        }
        curr = curr->next;
    }

    /* If no appropriate block is found allocate new block */
    size_t total_block_size = ALIGN(sizeof(molecule_t)) + requested_size;
    molecule_t* new_block = sbrk(total_block_size);
    if (new_block == (void*)-1) return NULL;

    new_block->is_free = 0;
    new_block->size = requested_size;
    new_block->next = NULL;
    new_block->prev = tail;

    if (head == NULL) head = new_block;
    else tail->next = new_block;
    tail = new_block;

    return (void*)(new_block + 1);
}

void* mol_realloc(void* ptr, size_t size) {
    if (ptr == NULL) return mol_alloc(size);
    if (size == 0) {
        mol_free(ptr);
        return NULL;
    }

    molecule_t* block = (molecule_t*)ptr - 1;
    size_t new_size = ALIGN(size);

    /* Shrink block if requested size is smaller than current block size*/
    if (new_size <= block->size) {
        split_block(block, new_size);
        return ptr;
    }

    /* Check if adjacent blocks can be used */
    size_t total_free_space = block->size;
    if (block->next && block->next->is_free) total_free_space += ALIGN(sizeof(molecule_t)) + block->next->size;
    if (block->prev && block->prev->is_free) total_free_space += ALIGN(sizeof(molecule_t)) + block->prev->size;
    if (total_free_space >= new_size) {
        size_t original_size = block->size;

        /* Mark current block as free to properly merge all blocks into one */
        block->is_free = 1;
        block = merge_free_blocks(block);
        /* Immediately mark block as used after mergin blocks */
        block->is_free = 0;

        /* Move data to a new block */
        if ((void*)(block + 1) != ptr) {
            memmove((void*)(block + 1), ptr, original_size);
        }

        /* Split block after merging in case it is too big */
        split_block(block, new_size);
        return (void*)(block + 1);
    }

    /* If everything fails then allocate new block and free old one */
    void* new_ptr = mol_alloc(new_size);
    if (new_ptr == NULL) return NULL;

    memcpy(new_ptr, ptr, block->size);
    mol_free(ptr);
    return new_ptr;
}

void mol_free(void *ptr) {
    if (ptr == NULL) return;
    molecule_t* block = (molecule_t*)ptr - 1;
    block->is_free = 1;
    merge_free_blocks(block);
}

/* size is expected to be aligned */
static void split_block(molecule_t* block, size_t new_size) {
    /* Minimum usable block size is header + one byte */
    size_t min_block_size = ALIGN(sizeof(molecule_t)) + ALIGN(1);

    /* If block size is too big then we can split it */
    if (block->size >= new_size + min_block_size) {
        /* Get start of leftover free space */
        molecule_t* new_free_block = (molecule_t*)((char*)block + ALIGN(sizeof(molecule_t)) + new_size);
        new_free_block->size = block->size - new_size - ALIGN(sizeof(molecule_t));
        new_free_block->is_free = 1;
        new_free_block->next = block->next;
        new_free_block->prev = block;

        block->size = new_size;
        block->next = new_free_block;

        if (new_free_block->next) new_free_block->next->prev = new_free_block;
        else tail = new_free_block;

        /* Merge adjacent free blocks to new free block */
        merge_free_blocks(new_free_block);
    }
}

/* block is expected to be free for merging to work properly */
/* Returns address of block that consists of all merged free blocks */
static molecule_t* merge_free_blocks(molecule_t* block) {
    /* Merge backward with any adjacent free blocks */
    while (block->prev && block->prev->is_free) {
        block->prev->size += ALIGN(sizeof(molecule_t)) + block->size;
        block->prev->next = block->next;
        if (block->next) block->next->prev = block->prev;
        else tail = block->prev;
        block = block->prev;
    }

    /* Merge forward with any adjacent free blocks */
    while (block->next && block->next->is_free) {
        block->size += ALIGN(sizeof(molecule_t)) + block->next->size;
        block->next = block->next->next;
        if (block->next) block->next->prev = block;
        else tail = block;
    }

    return block;
}

#ifdef MALLOCULE_DEBUG
#include <stdio.h>

void mol_print_heap() {
    printf("--- Heap State ---\n");
    printf("HEAD -> ");
    molecule_t* curr = head;
    while (curr != NULL) {
        printf("[%s: %zu bytes @ %p]", curr->is_free ? "FREE" : "USED", curr->size, (void*)curr);
        if (curr->next != NULL) printf(" <=> ");
        curr = curr->next;
    }
    printf(" <- TAIL\n------------------\n");
}
#endif

#endif // MALLOCULE_IMPL
#endif // MALLOCULE_H
