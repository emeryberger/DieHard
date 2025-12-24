// -*- C++ -*-

/**
 * @file   objectownership.h
 * @brief  Tracks object ownership for cross-thread free handling.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 * @note   Copyright (C) 2024 by Emery Berger, University of Massachusetts Amherst.
 *
 * This module provides ownership tracking for allocated objects, enabling
 * efficient cross-thread free handling in the scalable DieHard design.
 *
 * Ownership is determined by querying the heap structure itself (via getSize())
 * rather than maintaining a separate per-object hash table. This eliminates
 * the main scalability bottleneck of the original design.
 */

#ifndef DH_OBJECTOWNERSHIP_H
#define DH_OBJECTOWNERSHIP_H

#include <atomic>
#include <cstdint>
#include <cstddef>

#include "globalfreepool.h"
#include "heapregistry.h"

/**
 * @class OwnershipTrackingHeap
 * @brief Heap wrapper that tracks object ownership for cross-thread frees.
 *
 * This wrapper intercepts malloc/free calls and:
 * - On construction: Registers the heap with HeapRegistry
 * - On free: Checks ownership via getSize() and routes cross-thread frees
 *
 * Ownership detection uses the heap structure itself rather than a per-object
 * hash table, eliminating contention on the critical path.
 *
 * @tparam SuperHeap The underlying heap implementation.
 */
template <class SuperHeap>
class OwnershipTrackingHeap : public SuperHeap {
public:

  OwnershipTrackingHeap()
    : _threadId(ThreadIdManager::getThreadId()),
      _registered(false)
  {
  }

  inline void* malloc(size_t sz) {
    // Ensure we're registered (lazy initialization)
    ensureRegistered();

    // Drain any pending cross-thread frees for this thread
    drainPendingFrees();

    // Allocate from the underlying heap
    return SuperHeap::malloc(sz);
  }

  inline bool free(void* ptr) {
    if (ptr == nullptr) {
      return false;
    }

    // Ensure we're registered
    ensureRegistered();

    // Fast path: check if we own this object
    if (SuperHeap::getSize(ptr) > 0) {
      // We own it - free directly
      return SuperHeap::free(ptr);
    }

    // Slow path: find the owner via HeapRegistry
    size_t ownerId = HeapRegistry::getInstance().findOwner(ptr, _threadId);

    if (ownerId == INVALID_THREAD_ID) {
      // Unknown owner - possibly from large heap or invalid pointer
      // Try to free anyway (will return false if not found)
      return SuperHeap::free(ptr);
    }

    // Cross-thread free - route to the owner's queue
    GlobalFreePool::getInstance().crossThreadFree(ptr, ownerId);
    return true; // We've handled it (will be freed by owner later)
  }

  inline size_t getSize(void* ptr) {
    // Check if we own this object
    return SuperHeap::getSize(ptr);
  }

  /**
   * @brief Get this heap's thread ID.
   */
  size_t getThreadId() const {
    return _threadId;
  }

protected:

  /**
   * @brief Drain pending cross-thread frees for this thread.
   */
  void drainPendingFrees() {
    auto& pool = GlobalFreePool::getInstance();
    if (pool.shouldDrain(_threadId)) {
      pool.drainForThread(_threadId, [this](void* ptr) {
        SuperHeap::free(ptr);
      });
    }
  }

private:

  /**
   * @brief Ensure this heap is registered with HeapRegistry.
   *
   * Called lazily on first malloc/free to ensure the SuperHeap
   * is fully constructed before we register it.
   */
  void ensureRegistered() {
    if (!_registered) {
      // Register with the heap registry
      HeapRegistry::getInstance().registerHeap(
        static_cast<void*>(static_cast<SuperHeap*>(this)),
        _threadId,
        &OwnershipTrackingHeap::getSizeStatic
      );
      _registered = true;
    }
  }

  /**
   * @brief Static wrapper for getSize() used by HeapRegistry.
   */
  static size_t getSizeStatic(void* heap, void* ptr) {
    return static_cast<SuperHeap*>(heap)->getSize(ptr);
  }

  /// This heap's owner thread ID.
  const size_t _threadId;

  /// Whether we've registered with HeapRegistry.
  bool _registered;
};

#endif // DH_OBJECTOWNERSHIP_H
