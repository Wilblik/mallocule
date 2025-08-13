#ifndef MALLOCULE_H
#define MALLOCULE_H

#include <stdlib.h>
#include <string.h>

/*
 * TODO:
 * - Add more advanced multithreading support using arenas
 * - Use mmap for large allocations
 * - Bins for small allocations
 * - Canaries and guard pages for security
 * - Bit packing for is_free flag optimization
 */

/*
 * The header for each memory block ("molecule").
 * This struct is placed at the beginning of every block of memory
 * and contains metadata about the block.
 */
typedef struct molecule_t {
    size_t size;             /* The size of the usable memory area (payload), in bytes. */
    unsigned is_free;        /* A flag indicating if the block is free (1) or in use (0). */
    struct molecule_t* next; /* A pointer to the next block in the heap. */
    struct molecule_t* prev; /* A pointer to the previous block in the heap. */
} molecule_t;

/* Public API function declarations. */
void* mol_alloc(size_t size);
void* mol_realloc(void* ptr, size_t size);
void mol_free(void* ptr);

/*
 * Debugging macro to print the heap state.
 * It expands to a function call if MALLOCULE_DEBUG is defined, otherwise to nothing.
 */
#ifdef MALLOCULE_DEBUG
void mol_print_heap();
#define DEBUG_PRINT_HEAP() mol_print_heap()
#else
#define DEBUG_PRINT_HEAP() (void)0
#endif

#ifdef MALLOCULE_IMPL

#include <unistd.h>
#include <pthread.h>

/* The alignment for memory blocks, in bytes. Must be a power of 2. */
#define ALIGNMENT 8
/* A macro to round up a size to the nearest multiple of ALIGNMENT. */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1))

/* Global pointers to the start (head) and end (tail) of the heap. */
static molecule_t* head = NULL;
static molecule_t* tail = NULL;

/* Define and initialize the global heap mutex */
static pthread_mutex_t heap_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Forward declarations for helper functions. */
static void* mol_alloc_unlocked(size_t size);
static void* mol_realloc_unlocked(void* ptr, size_t size);
static void mol_free_unlocked(void* ptr);
static void split_block(molecule_t* block, size_t new_size);
static molecule_t* merge_free_blocks(molecule_t* block);

/*
 * Allocates a block of memory from the heap.
 * First, it searches the existing list of blocks for a free one that is
 * large enough. If none is found, it requests more memory from the OS.
 */
void* mol_alloc(size_t size) {
    pthread_mutex_lock(&heap_mutex);
    void* ptr = mol_alloc_unlocked(size);
    pthread_mutex_unlock(&heap_mutex);
    return ptr;
}

/*
 * Resizes a memory block, handling shrinking and growing.
 * It attempts to resize in-place first by splitting (for shrinking) or
 * by merging with adjacent free blocks (for growing). If in-place
 * expansion is not possible, it allocates a new block, copies the data,
 * and frees the old block.
 */
void* mol_realloc(void* ptr, size_t size) {
    pthread_mutex_lock(&heap_mutex);
    void* new_ptr = mol_realloc_unlocked(ptr, size);
    pthread_mutex_unlock(&heap_mutex);
    return new_ptr;
}

/* Marks a block as free and merges it with any adjacent free blocks. */
void mol_free(void* ptr) {
    pthread_mutex_lock(&heap_mutex);
    mol_free_unlocked(ptr);
    pthread_mutex_unlock(&heap_mutex);
}

void* mol_alloc_unlocked(size_t size) {
    if (size == 0) return NULL;
    size_t requested_size = ALIGN(size);

    /* First-fit search: find the first free block that's large enough. */
    molecule_t* curr = head;
    while (curr != NULL) {
        if (curr->is_free && curr->size >= requested_size) {
            curr->is_free = 0;
            split_block(curr, requested_size);
            return (void*)(curr + 1);
        }
        curr = curr->next;
    }

    /* If no suitable block is found, extend the heap using sbrk. */
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

void* mol_realloc_unlocked(void* ptr, size_t size) {
    if (ptr == NULL) return mol_alloc_unlocked(size);
    if (size == 0) {
        mol_free_unlocked(ptr);
        return NULL;
    }

    molecule_t* block = (molecule_t*)ptr - 1;
    size_t new_size = ALIGN(size);

    /* Case 1: Shrink the block if the new size is smaller. */
    if (new_size <= block->size) {
        split_block(block, new_size);
        return ptr;
    }

    /* Case 2: Try to expand in-place by merging with neighbors. */
    size_t total_free_space = block->size;
    if (block->next && block->next->is_free) total_free_space += ALIGN(sizeof(molecule_t)) + block->next->size;
    if (block->prev && block->prev->is_free) total_free_space += ALIGN(sizeof(molecule_t)) + block->prev->size;
    if (total_free_space >= new_size) {
        size_t original_size = block->size;

        /* Temporarily mark as free to allow the general merge function to work. */
        block->is_free = 1;
        block = merge_free_blocks(block);
        /* Reclaim the newly merged block. */
        block->is_free = 0;

        /* If the block's start address changed (merged backward), move the data. */
        if ((void*)(block + 1) != ptr) {
            memmove((void*)(block + 1), ptr, original_size);
        }

        /* Split the block after in case it's too large. */
        split_block(block, new_size);
        return (void*)(block + 1);
    }

    /* Case 3: Fallback - allocate a new block and move the data. */
    void* new_ptr = mol_alloc_unlocked(new_size);
    if (new_ptr == NULL) return NULL;

    memcpy(new_ptr, ptr, block->size);
    mol_free_unlocked(ptr);
    return new_ptr;
}

/* Splits a block into a used part and a new free part if it's too large. */
static void split_block(molecule_t* block, size_t new_size) {
    /* A new block must be large enough to hold its header and at least 1 byte. */
    size_t min_block_size = ALIGN(sizeof(molecule_t)) + ALIGN(1);

    if (block->size >= new_size + min_block_size) {
        /* Calculate the address of the new free block in the leftover space. */
        molecule_t* new_free_block = (molecule_t*)((char*)block + ALIGN(sizeof(molecule_t)) + new_size);
        new_free_block->size = block->size - new_size - ALIGN(sizeof(molecule_t));
        new_free_block->is_free = 1;
        new_free_block->next = block->next;
        new_free_block->prev = block;

        block->size = new_size;
        block->next = new_free_block;

        if (new_free_block->next) new_free_block->next->prev = new_free_block;
        else tail = new_free_block;

        /* The new free block might be adjacent to another free block. */
        merge_free_blocks(new_free_block);
    }
}

void mol_free_unlocked(void* ptr) {
    if (ptr == NULL) return;
    molecule_t* block = (molecule_t*)ptr - 1;
    block->is_free = 1;
    merge_free_blocks(block);
}

/*
 * Merges a free block with any adjacent free blocks.
 * Returns a pointer to the start of the final, merged free block.
 */
static molecule_t* merge_free_blocks(molecule_t* block) {
    /* Merge backward with any adjacent free blocks. */
    while (block->prev && block->prev->is_free) {
        block->prev->size += ALIGN(sizeof(molecule_t)) + block->size;
        block->prev->next = block->next;
        if (block->next) block->next->prev = block->prev;
        else tail = block->prev;
        block = block->prev;
    }

    /* Merge forward with any adjacent free blocks. */
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

/*
 * Prints a visual representation of the entire heap state.
 * Only available when MALLOCULE_DEBUG is defined.
 */
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
#endif /* MALLOCULE DEBUG */

#endif /* MALLOCULE_IMPL */
#endif /* MALLOCULE_H */
