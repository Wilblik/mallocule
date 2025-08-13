#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <assert.h>
#include <unistd.h>

#define MALLOCULE_IMPL
#define MALLOCULE_DEBUG
#include "mallocule.h"

/* Test Configuration */
#define NUM_THREADS 5
#define ITERATIONS_PER_THREAD 50000
#define MAX_ALLOC_SIZE 1024
#define POINTER_ARRAY_SIZE 64 /* Number of pointers each thread manages */

/*
 * Holds the state for a single worker thread.
 * Each thread manages its own set of allocated pointers.
 */
typedef struct {
    long thread_id;                         /* A unique ID for the thread. */
    void* pointers[POINTER_ARRAY_SIZE];     /* Array to store pointers to allocated memory. */
    size_t sizes[POINTER_ARRAY_SIZE];       /* The corresponding sizes of the allocations. */
} thread_data_t;

typedef enum {
    MEM_ACTION_ALLOC,
    MEM_ACTION_REALLOC,
    MEM_ACTION_FREE,
    MEM_ACTIONS_COUNT
} memory_action_t;

/*
 * The main function for each worker thread.
 * This function performs a series of random memory operations (alloc, realloc, free)
 * to create contention on the global heap. It verifies memory integrity by writing
 * the thread's ID into each allocated byte and checking it before subsequent operations.
 */
void* worker_thread_fn(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    /* Seed the random number generator uniquely for each thread. */
    unsigned seed = time(NULL) ^ (unsigned int)pthread_self();

    for (size_t i = 0; i < ITERATIONS_PER_THREAD; ++i) {
        memory_action_t action = rand_r(&seed) % MEM_ACTIONS_COUNT;
        size_t index = rand_r(&seed) % POINTER_ARRAY_SIZE;

        switch (action) {
            case MEM_ACTION_ALLOC: {
                /* Only allocate if the current slot is empty. */
                if (data->pointers[index] == NULL) {
                    size_t size = (rand_r(&seed) % MAX_ALLOC_SIZE) + 1;
                    data->pointers[index] = mol_alloc(size);
                    assert(data->pointers[index] != NULL);

                    /* If allocation succeeded, fill memory with the thread's ID.
                     * This is the key to verifying data integrity.
                     */
                    data->sizes[index] = size;
                    memset(data->pointers[index], (int)data->thread_id, size);
                    for (size_t j = 0; j < data->sizes[index]; ++j) {
                        assert(((char*)data->pointers[index])[j] == (char)data->thread_id);
                    }
                }
                break;
            }
            case MEM_ACTION_REALLOC: {
                /* Only reallocate if a pointer exists at this index. */
                if (data->pointers[index] != NULL) {
                    size_t new_size = (rand_r(&seed) % MAX_ALLOC_SIZE) + 1;

                    /* Before reallocating, check if the
                     * existing memory still contains the correct data.
                     */
                    for (size_t j = 0; j < data->sizes[index]; ++j) {
                        assert(((char*)data->pointers[index])[j] == (char)data->thread_id);
                    }

                    void* new_ptr = mol_realloc(data->pointers[index], new_size);
                    assert(new_ptr != NULL);

                    data->pointers[index] = new_ptr;
                    size_t check_size = (new_size < data->sizes[index]) ? new_size : data->sizes[index];

                    /* After reallocating, check if the original data was correctly copied. */
                    for (size_t j = 0; j < check_size; ++j) {
                        assert(((char*)new_ptr)[j] == (char)data->thread_id);
                    }

                    /* Fill the entire new block with the thread ID. */
                    memset(new_ptr, (int)data->thread_id, new_size);
                    data->sizes[index] = new_size;
                }
                break;
            }
            case MEM_ACTION_FREE: {
                /* Only free if a pointer exists at this index. */
                if (data->pointers[index] != NULL) {
                    /* Last check before freeing. */
                    for (size_t j = 0; j < data->sizes[index]; ++j) {
                        assert(((char*)data->pointers[index])[j] == (char)data->thread_id);
                    }
                    mol_free(data->pointers[index]);
                    data->pointers[index] = NULL;
                    data->sizes[index] = 0;
                }
                break;
            }
            default:
                /* Unreachable */
                assert(1);
        }
    }

    /* Free any blocks that were not freed during the main loop. */
    for (size_t i = 0; i < POINTER_ARRAY_SIZE; ++i) {
        if (data->pointers[i] != NULL) {
            mol_free(data->pointers[i]);
        }
    }

    return NULL;
}

int main() {
    pthread_t threads[NUM_THREADS] = {0};
    thread_data_t thread_data[NUM_THREADS] = {0};

    printf("\n🚀 Starting thread safety test with %d threads (%d iterations each)...\n", NUM_THREADS, ITERATIONS_PER_THREAD);

    /* Create and launch threads. */
    for (size_t i = 0; i < NUM_THREADS; ++i) {
        thread_data[i].thread_id = i + 1;
        if (pthread_create(&threads[i], NULL, worker_thread_fn, &thread_data[i]) != 0) {
            perror("ERROR: Could not create thread");
            return 1;
        }
    }

    /* Wait for all threads to complete their execution. */
    for (size_t i = 0; i < NUM_THREADS; ++i) {
        pthread_join(threads[i], NULL);
        printf("\nThread %zu finished.", i + 1);
    }

    printf("\n\n✅ All threads have completed.\n\n");

    /* Print the final state of the heap. It should consist of one large free block. */
    DEBUG_PRINT_HEAP();

    printf("\n✅ Test completed successfully.\n");

    return 0;
}
