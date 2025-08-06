#include <stdio.h>
#include <stdint.h>
#include <assert.h>

#define MALLOCULE_IMPL
#include "mallocule.h"

void test_basic() {
    printf("\n--- Running Basic Sanity Checks ---\n");

    int *p1 = mol_alloc(sizeof(int));
    assert(p1 != NULL);
    *p1 = 123;
    assert(*p1 == 123);
    printf("Test 1 Passed: Allocation and R/W successful.\n");

    double *p2 = mol_alloc(sizeof(double));
    assert(p2 != NULL);
    assert((void*)p1 != (void*)p2);
    printf("Test 2 Passed: Multiple allocations are distinct.\n");

    mol_free(p1);
    mol_free(p2);
}

void test_alignment() {
    printf("\n--- Running Alignment Test ---\n");

    void* p1 = mol_alloc(1);
    void* p2 = mol_alloc(3);
    void* p3 = mol_alloc(7);
    void* p4 = mol_alloc(15);

    assert(p1 != NULL && p2 != NULL && p3 != NULL && p4 != NULL);

    assert((uintptr_t)p1 % ALIGNMENT == 0);
    assert((uintptr_t)p2 % ALIGNMENT == 0);
    assert((uintptr_t)p3 % ALIGNMENT == 0);
    assert((uintptr_t)p4 % ALIGNMENT == 0);

    printf("Test Passed: All pointers are correctly aligned to %d bytes!\n", ALIGNMENT);

    mol_free(p1);
    mol_free(p2);
    mol_free(p3);
    mol_free(p4);
}

void test_reuse() {
    printf("\n--- Running Memory Reuse Test ---\n");

    int *p1 = mol_alloc(sizeof(int));
    assert(p1 != NULL);
    printf("Step 1: Allocated block at address: %p\n", (void*)p1);

    mol_free(p1);
    printf("Step 2: Freed the block.\n");

    int *p2 = mol_alloc(sizeof(int));
    assert(p2 != NULL);
    printf("Step 3: Allocated a new block at address: %p\n", (void*)p2);

    assert(p1 == p2);
    printf("Test Passed: Memory was successfully reused!\n");

    mol_free(p2);
}

void test_linking_and_traversal() {
    printf("\n--- Running Linking and Traversal Test ---\n");

    void *p1 = mol_alloc(100);
    void *p2 = mol_alloc(100);
    assert(p1 != NULL && p2 != NULL);
    printf("Step 1: Allocated p1 (%p) and p2 (%p).\n", (void*)p1, (void*)p2);

    mol_free(p2);
    printf("Step 2: Freed p2. The head (p1) is still in use.\n");

    void *p3 = mol_alloc(100);
    assert(p3 != NULL);
    printf("Step 3: Allocated p3 (%p). It should reuse the memory from p2.\n", (void*)p3);

    assert(p3 == p2);
    printf("Test Passed: Allocator correctly traversed the list to find a free block!\n");

    mol_free(p1);
    mol_free(p3);
}

void test_coalescing() {
    printf("\n--- Running Coalescing Test ---\n");

    void* p1 = mol_alloc(100);
    void* p2 = mol_alloc(100);
    void* p3 = mol_alloc(100);
    assert(p1 != NULL && p2 != NULL && p3 != NULL);
    printf("Step 1: Allocated p1, p2, and p3.\n");

    mol_free(p1);
    mol_free(p3);
    printf("Step 2: Freed p1 and p3.\n");

    mol_free(p2);
    printf("Step 3: Freed p2, triggering coalesce.\n");

    void* p4 = mol_alloc(300 + sizeof(molecule_t)); // Request space larger than any single block
    assert(p4 != NULL);
    printf("Step 4: Allocated large block p4.\n");

    assert(p4 == p1);
    printf("Test Passed: Blocks were successfully coalesced!\n");

    mol_free(p4);
}

void test_stress() {
    printf("\n--- Running Stress Test ---\n");
    void *p;

    p = mol_alloc(0);
    assert(p == NULL);
    mol_free(p);
    printf("malloc(0) test passed.\n");

    void *pointers[100];
    for (int i = 0; i < 100; i++) {
        pointers[i] = mol_alloc(i + 1);
    }
    for (int i = 0; i < 100; i++) {
        mol_free(pointers[i]);
    }
    printf("Rapid churn test passed.\n");
}


int main() {
    test_basic();
    test_alignment();
    test_reuse();
    test_linking_and_traversal();
    test_coalescing();
    test_stress();

    printf("\n--- TESTS FINISHED SUCCESSFULLY ---\n");
    return 0;
}
