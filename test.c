#include <stdio.h>
#include <stdint.h>
#include <assert.h>

#define MALLOCULE_IMPL
//#define MALLOCULE_DEBUG
#include "mallocule.h"

void test_basic() {
    printf("\n--- Running Basic Sanity Checks ---\n");
    DEBUG_PRINT_HEAP();

    int *p1 = mol_alloc(sizeof(int));
    assert(p1 != NULL);
    *p1 = 123;
    assert(*p1 == 123);
    printf("Test 1 Passed: Allocation and R/W successful.\n");
    DEBUG_PRINT_HEAP();

    double *p2 = mol_alloc(sizeof(double));
    assert(p2 != NULL);
    assert((void*)p1 != (void*)p2);
    printf("Test 2 Passed: Multiple allocations are distinct.\n");
    DEBUG_PRINT_HEAP();

    mol_free(p1);
    mol_free(p2);
}

void test_alignment() {
    printf("\n--- Running Alignment Test ---\n");
    DEBUG_PRINT_HEAP();

    void* p1 = mol_alloc(1);
    DEBUG_PRINT_HEAP();
    void* p2 = mol_alloc(3);
    DEBUG_PRINT_HEAP();
    void* p3 = mol_alloc(7);
    DEBUG_PRINT_HEAP();
    void* p4 = mol_alloc(15);
    DEBUG_PRINT_HEAP();

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
    DEBUG_PRINT_HEAP();

    int *p1 = mol_alloc(sizeof(int));
    assert(p1 != NULL);
    printf("Step 1: Allocated block at address: %p\n", (void*)p1);
    DEBUG_PRINT_HEAP();

    mol_free(p1);
    printf("Step 2: Freed the block.\n");
    DEBUG_PRINT_HEAP();

    int *p2 = mol_alloc(sizeof(int));
    assert(p2 != NULL);
    printf("Step 3: Allocated a new block at address: %p\n", (void*)p2);
    DEBUG_PRINT_HEAP();

    assert(p1 == p2);
    printf("Test Passed: Memory was successfully reused!\n");

    mol_free(p2);
}

void test_linking_and_traversal() {
    printf("\n--- Running Linking and Traversal Test ---\n");
    DEBUG_PRINT_HEAP();

    void *p1 = mol_alloc(36);
    DEBUG_PRINT_HEAP();
    void *p2 = mol_alloc(36);
    DEBUG_PRINT_HEAP();
    assert(p1 != NULL && p2 != NULL);
    printf("Step 1: Allocated p1 (%p) and p2 (%p).\n", (void*)p1, (void*)p2);

    mol_free(p2);
    printf("Step 2: Freed p2. The head (p1) is still in use.\n");
    DEBUG_PRINT_HEAP();

    void *p3 = mol_alloc(36);
    assert(p3 != NULL);
    printf("Step 3: Allocated p3 (%p). It should reuse a free block.\n", (void*)p3);
    DEBUG_PRINT_HEAP();

    assert(p3 == p2);
    printf("Test Passed: Allocator correctly traversed the list to find a free block!\n");

    mol_free(p1);
    mol_free(p3);
}

void test_splitting() {
    printf("\n--- Running Splitting Test ---\n");
    DEBUG_PRINT_HEAP();

    void* p1 = mol_alloc(50);
    assert(p1 != NULL);
    printf("Step 1: Allocated small block p1. This should split the block.\n");
    DEBUG_PRINT_HEAP();

    void* p2 = mol_alloc(16);
    assert(p2 != NULL);
    printf("Step 4: Allocated small block p3. It should use the leftover space.\n");
    DEBUG_PRINT_HEAP();

    void* expected_p2_addr = (char*)p1 + ALIGN(sizeof(molecule_t)) + ALIGN(50);
    assert(p2 == expected_p2_addr);

    printf("Test Passed: Block was successfully split and reused!\n");

    mol_free(p1);
    mol_free(p2);
}

void test_merging() {
    printf("\n--- Running Merging Test ---\n");
    DEBUG_PRINT_HEAP();

    void* p1 = mol_alloc(100);
    void* p2 = mol_alloc(100);
    void* p3 = mol_alloc(100);
    assert(p1 != NULL && p2 != NULL && p3 != NULL);
    printf("Step 1: Allocated p1, p2, and p3.\n");
    DEBUG_PRINT_HEAP();

    mol_free(p1);
    mol_free(p3);
    printf("Step 2: Freed p1 and p3.\n");
    DEBUG_PRINT_HEAP();

    mol_free(p2);
    printf("Step 3: Freed p2, triggering merge.\n");
    DEBUG_PRINT_HEAP();

    void* p4 = mol_alloc(300 + sizeof(molecule_t)); // Request space larger than any single block
    assert(p4 != NULL);
    printf("Step 4: Allocated large block p4.\n");
    DEBUG_PRINT_HEAP();

    assert(p4 == p1);
    printf("Test Passed: Blocks were successfully merged!\n");

    mol_free(p4);
}

void test_realloc() {
    printf("\n--- Running Realloc Test ---\n");
    DEBUG_PRINT_HEAP();

    printf("Step 1: Testing shrink and expanding in place.\n");
    int* p1 = mol_alloc(sizeof(int) * 20);
    for (int i = 0; i < 10; i++) p1[i] = i;
    DEBUG_PRINT_HEAP();

    int* p2 = mol_realloc(p1, sizeof(int) * 5);
    assert(p2 == p1);
    for (int i = 0; i < 5; i++) assert(p2[i] == i);
    printf("Shrink test passed.\n");
    DEBUG_PRINT_HEAP();

    int* p3 = mol_realloc(p2, sizeof(int) * 20);
    assert(p3 == p2);
    for(int i = 0; i < 5; i++) assert(p3[i] == i);
    printf("Expand in place test passed.\n");
    DEBUG_PRINT_HEAP();

    printf("\nStep 2: Testing expansion by moving.\n");
    int* p4 = mol_realloc(p3, sizeof(int) * 120);
    assert(p4 != NULL);
    assert(p4 != p3);
    for(int i = 0; i < 5; i++) assert(p4[i] == i);
    printf("Expand by moving test passed.\n");
    DEBUG_PRINT_HEAP();

    mol_free(p4);
}

void test_stress() {
    printf("\n--- Running Stress Test ---\n");
    DEBUG_PRINT_HEAP();

    void* p;
    p = mol_alloc(0);
    assert(p == NULL);
    mol_free(p);
    printf("malloc(0) test passed.\n");

    void* pointers[100];
    for (int i = 0; i < 100; i++) {
        pointers[i] = mol_alloc(i + 1);
    }
    DEBUG_PRINT_HEAP();
    for (int i = 0; i < 100; i++) {
        mol_free(pointers[i]);
    }
    DEBUG_PRINT_HEAP();
    printf("Rapid churn test passed.\n");
}


int main() {
    test_basic();
    test_alignment();
    test_reuse();
    test_linking_and_traversal();
    test_splitting();
    test_merging();
    test_realloc();
    test_stress();

    printf("\n--- TESTS FINISHED SUCCESSFULLY ---\n");
    return 0;
}
