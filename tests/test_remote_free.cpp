/**
 * @file test_remote_free.cpp
 * @brief Test to verify cross-thread frees preserve object contents.
 *
 * This test allocates objects in one thread, passes them to another thread
 * to free, then checks if the original contents are preserved.
 *
 * DieHard's dangling pointer protection requires that freed memory is not
 * immediately corrupted, so this test verifies that guarantee is maintained
 * even for cross-thread frees.
 *
 * Usage:
 *   # With DieHard (should PASS - 0 corruption):
 *   DYLD_INSERT_LIBRARIES=libdiehard.dylib ./test_remote_free  # macOS
 *   LD_PRELOAD=libdiehard.so ./test_remote_free                # Linux
 *
 *   # With system malloc (will show corruption - that's expected):
 *   ./test_remote_free
 */

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>

// Number of objects to test
static constexpr size_t NUM_OBJECTS = 1000;

// Object size (must be >= 8 bytes)
static constexpr size_t OBJECT_SIZE = 64;

// Magic pattern to fill objects with
static constexpr uint8_t MAGIC_BYTE = 0xAB;

// Synchronization
static std::atomic<bool> objects_ready{false};
static std::atomic<bool> frees_done{false};
static std::atomic<size_t> corruption_count{0};

// Shared object pointers
static void* objects[NUM_OBJECTS];

// Fill an object with a known pattern
void fill_object(void* ptr, size_t size) {
    memset(ptr, MAGIC_BYTE, size);
}

// Check if an object still has the expected pattern
size_t check_object(void* ptr, size_t size) {
    uint8_t* bytes = reinterpret_cast<uint8_t*>(ptr);
    size_t corrupted = 0;
    for (size_t i = 0; i < size; i++) {
        if (bytes[i] != MAGIC_BYTE) {
            corrupted++;
        }
    }
    return corrupted;
}

// Thread A: Allocates objects
void allocator_thread() {
    printf("Allocator: Allocating %zu objects of size %zu\n", NUM_OBJECTS, OBJECT_SIZE);

    for (size_t i = 0; i < NUM_OBJECTS; i++) {
        objects[i] = malloc(OBJECT_SIZE);
        if (objects[i] == nullptr) {
            fprintf(stderr, "Allocation failed at index %zu\n", i);
            exit(1);
        }
        fill_object(objects[i], OBJECT_SIZE);
    }

    printf("Allocator: All objects allocated and filled with 0x%02X\n", MAGIC_BYTE);

    // Signal that objects are ready
    objects_ready.store(true, std::memory_order_release);

    // Wait for frees to complete
    while (!frees_done.load(std::memory_order_acquire)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // Small delay to ensure any async operations complete
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Check all objects for corruption
    printf("Allocator: Checking objects for corruption...\n");

    size_t objects_corrupted = 0;
    size_t total_bytes_corrupted = 0;

    for (size_t i = 0; i < NUM_OBJECTS; i++) {
        size_t corrupted = check_object(objects[i], OBJECT_SIZE);
        if (corrupted > 0) {
            objects_corrupted++;
            total_bytes_corrupted += corrupted;

            if (objects_corrupted <= 3) {
                printf("  Object %zu: %zu/%zu bytes corrupted\n", i, corrupted, OBJECT_SIZE);
            }
        }
    }

    corruption_count.store(objects_corrupted, std::memory_order_release);

    printf("\n=== RESULTS ===\n");
    printf("Objects tested:    %zu\n", NUM_OBJECTS);
    printf("Objects corrupted: %zu (%.1f%%)\n",
           objects_corrupted, 100.0 * objects_corrupted / NUM_OBJECTS);
}

// Thread B: Frees objects allocated by Thread A
void freer_thread() {
    // Wait for objects to be ready
    while (!objects_ready.load(std::memory_order_acquire)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    printf("Freer: Freeing %zu objects from different thread\n", NUM_OBJECTS);

    for (size_t i = 0; i < NUM_OBJECTS; i++) {
        free(objects[i]);
    }

    printf("Freer: All objects freed\n");

    // Signal that frees are done
    frees_done.store(true, std::memory_order_release);
}

int main() {
    printf("=== Cross-Thread Free Test ===\n\n");

    std::thread allocator(allocator_thread);
    std::thread freer(freer_thread);

    allocator.join();
    freer.join();

    size_t corrupted = corruption_count.load();
    if (corrupted > 0) {
        printf("\nFAIL: Cross-thread frees corrupt object contents.\n");
        return 1;
    }

    printf("\nPASS: Object contents preserved after cross-thread free.\n");
    return 0;
}
