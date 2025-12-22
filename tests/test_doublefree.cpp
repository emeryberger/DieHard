/**
 * @file test_doublefree.cpp
 * @brief Test that DieHard handles double-free gracefully.
 *
 * DieHard's bitmap-based allocation makes double-frees a no-op
 * (the second free just clears an already-clear bit). This test
 * verifies that double-freeing doesn't crash the program.
 */

#include <cstdio>
#include <cstdlib>

int main() {
    printf("=== Double Free Test ===\n\n");
    printf("Testing that double-free doesn't crash...\n");

    // Allocate and double-free
    char* ptr = (char*)malloc(64);
    if (!ptr) {
        printf("FAIL: malloc returned null\n");
        return 1;
    }

    printf("Allocated: %p\n", (void*)ptr);

    free(ptr);
    printf("First free: OK\n");

    // This would crash with most allocators, but DieHard handles it
    free(ptr);
    printf("Second free: OK (no crash)\n");

    // Verify we can still allocate
    for (int i = 0; i < 10; i++) {
        char* p = (char*)malloc(64);
        if (!p) {
            printf("FAIL: malloc returned null after double-free\n");
            return 1;
        }
        free(p);
    }

    printf("\nPASS: Double-free handled gracefully.\n");
    return 0;
}
