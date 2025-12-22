// -*- C++ -*-

/**
 * @file   heapregistry.h
 * @brief  Registry of per-thread heap instances for ownership detection.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 * @note   Copyright (C) 2024 by Emery Berger, University of Massachusetts Amherst.
 *
 * This module provides a simple registry of per-thread heap instances,
 * enabling efficient ownership detection without a per-object hash table.
 * Ownership is determined by asking each heap if it contains a given pointer.
 */

#ifndef DH_HEAPREGISTRY_H
#define DH_HEAPREGISTRY_H

#include <atomic>
#include <cstdint>
#include <cstddef>

#include "globalfreepool.h"  // For MAX_THREADS, INVALID_THREAD_ID, ThreadIdManager

/**
 * @class HeapRegistry
 * @brief Maintains a registry of all per-thread heap instances.
 *
 * When a thread creates its heap, it registers the heap pointer here.
 * On cross-thread free, we iterate through registered heaps to find
 * which one owns the pointer (via getSize() check).
 *
 * This replaces the per-object OwnershipTable with a much smaller
 * per-heap registry, eliminating the main scalability bottleneck.
 */
class HeapRegistry {
public:

  /**
   * @brief Function type for checking if a heap owns a pointer.
   * @param heap The heap instance pointer.
   * @param ptr The pointer to check.
   * @return The size of the object if owned, 0 otherwise.
   */
  using GetSizeFunc = size_t (*)(void* heap, void* ptr);

  /**
   * @brief Get the singleton instance.
   */
  static HeapRegistry& getInstance() {
    static HeapRegistry instance;
    return instance;
  }

  /**
   * @brief Register a heap instance for a thread.
   * @param heap Pointer to the heap instance.
   * @param threadId The owning thread's ID.
   * @param getSizeFunc Function to check ownership (calls heap->getSize(ptr)).
   *
   * Thread-safe. Called once per thread when its heap is created.
   */
  void registerHeap(void* heap, size_t threadId, GetSizeFunc getSizeFunc) {
    if (threadId >= MAX_THREADS) {
      return;
    }

    // Store directly at the thread's index for O(1) lookup
    _entries[threadId].heap = heap;
    _entries[threadId].getSizeFunc = getSizeFunc;
    _entries[threadId].registered.store(true, std::memory_order_release);

    // Track the maximum thread ID we've seen for iteration bounds
    size_t oldMax = _maxThreadId.load(std::memory_order_relaxed);
    while (oldMax < threadId + 1) {
      _maxThreadId.compare_exchange_weak(oldMax, threadId + 1,
                                          std::memory_order_release,
                                          std::memory_order_relaxed);
    }
  }

  /**
   * @brief Find which thread owns a given pointer.
   * @param ptr The pointer to look up.
   * @param excludeThreadId Skip this thread (optimization for checking own heap first).
   * @return The owner's thread ID, or INVALID_THREAD_ID if not found.
   *
   * Thread-safe. Iterates through registered heaps and checks ownership.
   */
  size_t findOwner(void* ptr, size_t excludeThreadId = INVALID_THREAD_ID) const {
    size_t maxId = _maxThreadId.load(std::memory_order_acquire);

    for (size_t i = 0; i < maxId; i++) {
      if (i == excludeThreadId) {
        continue;
      }

      const Entry& entry = _entries[i];
      if (!entry.registered.load(std::memory_order_acquire)) {
        continue;
      }

      // Safety check for null function pointer
      if (entry.getSizeFunc == nullptr || entry.heap == nullptr) {
        continue;
      }

      // Check if this heap owns the pointer
      if (entry.getSizeFunc(entry.heap, ptr) > 0) {
        return i;
      }
    }

    return INVALID_THREAD_ID;
  }

  /**
   * @brief Check if a specific thread's heap owns a pointer.
   * @param ptr The pointer to check.
   * @param threadId The thread to check.
   * @return true if the thread's heap owns the pointer.
   */
  bool isOwnedBy(void* ptr, size_t threadId) const {
    if (threadId >= MAX_THREADS) {
      return false;
    }

    const Entry& entry = _entries[threadId];
    if (!entry.registered.load(std::memory_order_acquire)) {
      return false;
    }

    return entry.getSizeFunc(entry.heap, ptr) > 0;
  }

  /**
   * @brief Get the size of an object from any registered heap.
   * @param ptr The pointer to look up.
   * @param excludeThreadId Skip this thread (already checked by caller).
   * @return The size of the object, or 0 if not found in any heap.
   *
   * This is needed because malloc_usable_size must work for objects
   * allocated by any thread.
   */
  size_t getSizeFromAnyHeap(void* ptr, size_t excludeThreadId = INVALID_THREAD_ID) const {
    size_t maxId = _maxThreadId.load(std::memory_order_acquire);

    for (size_t i = 0; i < maxId; i++) {
      if (i == excludeThreadId) {
        continue;
      }

      const Entry& entry = _entries[i];
      if (!entry.registered.load(std::memory_order_acquire)) {
        continue;
      }

      size_t sz = entry.getSizeFunc(entry.heap, ptr);
      if (sz > 0) {
        return sz;
      }
    }

    return 0;
  }

private:

  HeapRegistry() : _maxThreadId(0) {
    for (size_t i = 0; i < MAX_THREADS; i++) {
      _entries[i].heap = nullptr;
      _entries[i].getSizeFunc = nullptr;
      _entries[i].registered.store(false, std::memory_order_relaxed);
    }
  }

  // Disable copying
  HeapRegistry(const HeapRegistry&) = delete;
  HeapRegistry& operator=(const HeapRegistry&) = delete;

  /// An entry in the registry.
  struct Entry {
    void* heap;                      // Pointer to the heap instance
    GetSizeFunc getSizeFunc;         // Function to check ownership
    std::atomic<bool> registered;    // Whether this entry is valid
  };

  /// Registry entries indexed by thread ID.
  Entry _entries[MAX_THREADS];

  /// Maximum thread ID seen (for iteration bounds).
  std::atomic<size_t> _maxThreadId;
};

#endif // DH_HEAPREGISTRY_H
