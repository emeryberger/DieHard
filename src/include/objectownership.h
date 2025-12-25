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
#include "platformspecific.h"

// How often to check for pending cross-thread frees (every N mallocs).
// Checking on every malloc adds overhead from atomic loads.
#ifndef DRAIN_CHECK_INTERVAL
#define DRAIN_CHECK_INTERVAL 16
#endif

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
      _registered(false),
      _allocCount(0)
  {
  }

  ATTRIBUTE_ALWAYS_INLINE void* malloc(size_t sz) {
    // Ensure we're registered (lazy initialization).
    // After first allocation, _registered is always true.
    if (unlikely(!_registered)) {
      doRegister();
    }

    // Periodically check for pending cross-thread frees.
    // Checking every malloc adds atomic load overhead, so we batch.
    if (unlikely((++_allocCount & (DRAIN_CHECK_INTERVAL - 1)) == 0)) {
      maybeDrainPendingFrees();
    }

    // Allocate from the underlying heap
    return SuperHeap::malloc(sz);
  }

  ATTRIBUTE_ALWAYS_INLINE bool free(void* ptr) {
    if (unlikely(ptr == nullptr)) {
      return false;
    }

    // Fast path: try to free directly from our heap.
    // free() returns true if we owned the object and freed it.
    // This avoids calling getSize() first, which would iterate
    // through size classes twice (once for getSize, once for free).
    if (likely(SuperHeap::free(ptr))) {
      return true;
    }

    // Slow path: cross-thread free or unknown object
    return freeSlow(ptr);
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
   * @brief Check and drain pending cross-thread frees if threshold reached.
   * @note Called periodically, not on every malloc.
   */
  NO_INLINE void maybeDrainPendingFrees() {
    auto& pool = GlobalFreePool::getInstance();
    if (pool.shouldDrain(_threadId)) {
      pool.drainForThread(_threadId, [this](void* ptr) {
        SuperHeap::free(ptr);
      });
    }
  }

private:

  /**
   * @brief Slow path for free: handle cross-thread frees.
   * @note Separated from fast path to keep hot code compact.
   */
  NO_INLINE bool freeSlow(void* ptr) {
    // Ensure we're registered (needed for cross-thread lookup)
    if (unlikely(!_registered)) {
      doRegister();
    }

    // Find the owner via HeapRegistry
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

  /**
   * @brief Register this heap with HeapRegistry.
   * @note Called once on first malloc/free. Marked NO_INLINE to keep hot path small.
   */
  NO_INLINE void doRegister() {
    HeapRegistry::getInstance().registerHeap(
      static_cast<void*>(static_cast<SuperHeap*>(this)),
      _threadId,
      &OwnershipTrackingHeap::getSizeStatic
    );
    _registered = true;
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

  /// Allocation counter for batching drain checks.
  unsigned int _allocCount;
};

#endif // DH_OBJECTOWNERSHIP_H
