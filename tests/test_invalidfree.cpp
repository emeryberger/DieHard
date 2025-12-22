/**
 * @file test_invalidfree.cpp
 * @brief Test that DieHard handles invalid frees gracefully.
 *
 * DieHard verifies that freed pointers are valid heap addresses
 * before attempting to free. Invalid frees (stack pointers, middle
 * of allocations, etc.) are detected and ignored rather than crashing.
 */

#include <cstdio>
#include <cstdlib>

int main() {
    printf("=== Invalid Free Test ===\n\n");
    printf("Testing that invalid frees don't crash...\n");

    char stack_buf[256];
    char* heap_ptr = (char*)malloc(64);

    if (!heap_ptr) {
        printf("FAIL: malloc returned null\n");
        return 1;
    }

    printf("Heap allocation: %p\n", (void*)heap_ptr);
    printf("Stack buffer:    %p\n", (void*)stack_buf);

    // Try to free a stack pointer (invalid)
    printf("\nTrying to free stack pointer...\n");
    free(&stack_buf[4]);
    printf("Stack free: OK (ignored, no crash)\n");

    // Try to free middle of heap allocation (invalid)
    printf("\nTrying to free middle of heap object...\n");
    free(heap_ptr + 1);
    printf("Middle free: OK (ignored, no crash)\n");

    // Clean up properly
    free(heap_ptr);
    printf("Proper free: OK\n");

    // Verify we can still allocate
    for (int i = 0; i < 10; i++) {
        char* p = (char*)malloc(64);
        if (!p) {
            printf("FAIL: malloc returned null after invalid frees\n");
            return 1;
        }
        free(p);
    }

    printf("\nPASS: Invalid frees handled gracefully.\n");
    return 0;
}
