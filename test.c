#include <stdio.h>
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
    printf("Basic checks complete.\n");
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
    test_reuse();
    test_linking_and_traversal();
    test_stress();

    printf("\n--- TESTS FINISHED SUCCESSFULLY ---\n");
    return 0;
}
