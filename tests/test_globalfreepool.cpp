/**
 * @file test_globalfreepool.cpp
 * @brief Unit test for GlobalFreePool to verify it preserves object contents.
 *
 * This test verifies that GlobalFreePool::push() does NOT corrupt freed object
 * contents. The array-based ring buffer design stores pointers externally rather
 * than embedding linked list nodes in the freed objects.
 *
 * This is critical for DieHard's dangling pointer protection: freed memory must
 * not be immediately corrupted so that dangling pointer reads still see valid data.
 */

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>

// Include the GlobalFreePool directly
#include "globalfreepool.h"

// Number of objects to test
static constexpr size_t NUM_OBJECTS = 100;

// Object size
static constexpr size_t OBJECT_SIZE = 64;

// Magic pattern
static constexpr uint8_t MAGIC_BYTE = 0xCD;

struct TestObject {
    uint8_t data[OBJECT_SIZE];
};

void fill_object(TestObject* obj) {
    memset(obj->data, MAGIC_BYTE, OBJECT_SIZE);
}

// Check corruption and return number of corrupted bytes
size_t check_object(TestObject* obj) {
    size_t corrupted = 0;
    for (size_t i = 0; i < OBJECT_SIZE; i++) {
        if (obj->data[i] != MAGIC_BYTE) {
            corrupted++;
        }
    }
    return corrupted;
}

void print_hex(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        printf("%02X ", data[i]);
    }
}

int main() {
    printf("=== GlobalFreePool Preservation Test ===\n\n");
    printf("Verifying that GlobalFreePool::push() preserves object contents.\n\n");

    // Allocate test objects
    std::vector<TestObject*> objects;
    for (size_t i = 0; i < NUM_OBJECTS; i++) {
        TestObject* obj = new TestObject;
        fill_object(obj);
        objects.push_back(obj);
    }

    printf("Allocated %zu objects of size %zu, filled with 0x%02X\n",
           NUM_OBJECTS, OBJECT_SIZE, MAGIC_BYTE);

    // Verify all objects are initially uncorrupted
    for (size_t i = 0; i < NUM_OBJECTS; i++) {
        if (check_object(objects[i]) > 0) {
            printf("FAIL: Object %zu corrupted before test!\n", i);
            return 1;
        }
    }
    printf("Verified: All objects have expected contents before push.\n");

    // Get the global free pool
    GlobalFreePool& pool = GlobalFreePool::getInstance();

    // Push all objects to the free pool (simulating cross-thread free)
    constexpr size_t OWNER_THREAD_ID = 0;
    printf("Pushing all objects to GlobalFreePool...\n");

    for (size_t i = 0; i < NUM_OBJECTS; i++) {
        pool.crossThreadFree(objects[i], OWNER_THREAD_ID);
    }

    // Check for corruption AFTER push but BEFORE drain
    printf("Checking objects for corruption after push...\n");

    size_t objects_corrupted = 0;
    for (size_t i = 0; i < NUM_OBJECTS; i++) {
        size_t corrupted = check_object(objects[i]);
        if (corrupted > 0) {
            objects_corrupted++;
            if (objects_corrupted <= 3) {
                printf("  Object %zu: %zu bytes corrupted\n", i, corrupted);
                printf("    Got:      ");
                print_hex(objects[i]->data, 16);
                printf("\n    Expected: ");
                for (int j = 0; j < 16; j++) printf("%02X ", MAGIC_BYTE);
                printf("\n");
            }
        }
    }

    // Clean up - drain the pool
    size_t drained = pool.drainForThread(OWNER_THREAD_ID, [](void*) {});

    // Delete objects
    for (auto obj : objects) {
        delete obj;
    }

    // Report results
    printf("\n=== RESULTS ===\n");
    printf("Objects tested:    %zu\n", NUM_OBJECTS);
    printf("Objects corrupted: %zu\n", objects_corrupted);
    printf("Objects drained:   %zu\n", drained);

    if (objects_corrupted > 0) {
        printf("\nFAIL: GlobalFreePool corrupts object contents!\n");
        return 1;
    }

    printf("\nPASS: Object contents preserved.\n");
    return 0;
}
