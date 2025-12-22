// -*- C++ -*-

/**
 * @file   globalfreepool.h
 * @brief  Lock-free structure for handling cross-thread frees.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 * @note   Copyright (C) 2024 by Emery Berger, University of Massachusetts Amherst.
 *
 * The GlobalFreePool provides a scalable mechanism for handling the case
 * where thread A frees an object that was allocated by thread B. Rather
 * than requiring locks, freed objects are placed in per-owner-thread queues
 * and batched for efficiency.
 *
 * IMPORTANT: This implementation uses an array-based ring buffer instead of
 * embedding linked list nodes in freed objects. This preserves DieHard's
 * guarantee that freed memory is not immediately corrupted, which protects
 * against dangling pointer bugs.
 */

#ifndef DH_GLOBALFREEPOOL_H
#define DH_GLOBALFREEPOOL_H

#include <atomic>
#include <cstdint>
#include <cstddef>
#include <pthread.h>

/**
 * @brief Special thread ID indicating unknown or invalid ownership.
 */
static constexpr size_t INVALID_THREAD_ID = SIZE_MAX;

// Cache line size for alignment to prevent false sharing
#ifndef CACHE_LINE_SIZE
#define CACHE_LINE_SIZE 64
#endif

// Maximum number of threads supported (can be overridden at compile time)
#ifndef MAX_THREADS
#define MAX_THREADS 64
#endif

// Batch size threshold before signaling owner thread
#ifndef FREE_BATCH_SIZE
#define FREE_BATCH_SIZE 32
#endif

// Ring buffer capacity per thread (must be power of 2)
// Reduced from 4096 to keep static memory footprint reasonable (~512KB vs 8MB)
#ifndef FREE_QUEUE_CAPACITY
#define FREE_QUEUE_CAPACITY 1024
#endif

/**
 * @class PerThreadFreeQueue
 * @brief A lock-free MPSC queue for objects freed by other threads.
 *
 * Uses an array-based ring buffer. Multiple producer threads can push
 * concurrently, and the single owner thread drains the queue.
 *
 * CRITICAL: This implementation does NOT modify the freed objects' memory,
 * preserving DieHard's dangling pointer protection guarantees.
 */
class alignas(CACHE_LINE_SIZE) PerThreadFreeQueue {
public:
  static_assert((FREE_QUEUE_CAPACITY & (FREE_QUEUE_CAPACITY - 1)) == 0,
                "FREE_QUEUE_CAPACITY must be a power of 2");

  PerThreadFreeQueue()
    : _head(0),
      _tail(0)
  {
    // Initialize all slots to nullptr
    for (size_t i = 0; i < FREE_QUEUE_CAPACITY; i++) {
      _slots[i].store(nullptr, std::memory_order_relaxed);
    }
  }

  /**
   * @brief Push a freed object onto this thread's queue.
   * @param ptr Pointer to the freed object.
   * @return true if successfully queued, false if queue is full.
   * @note Thread-safe, lock-free. Called by non-owner threads.
   *       Does NOT modify the memory pointed to by ptr.
   */
  bool push(void* ptr) {
    // Claim a slot by atomically incrementing head
    size_t slot;
    size_t currentHead = _head.load(std::memory_order_relaxed);

    do {
      // Check if queue is full
      size_t currentTail = _tail.load(std::memory_order_acquire);
      if (currentHead - currentTail >= FREE_QUEUE_CAPACITY) {
        // Queue is full
        return false;
      }
    } while (!_head.compare_exchange_weak(currentHead, currentHead + 1,
                                           std::memory_order_acq_rel,
                                           std::memory_order_relaxed));

    // We've claimed slot at index (currentHead % CAPACITY)
    slot = currentHead & (FREE_QUEUE_CAPACITY - 1);

    // Store the pointer. Use release to ensure the pointer is visible
    // to the consumer after they see a non-null value.
    _slots[slot].store(ptr, std::memory_order_release);

    return true;
  }

  /**
   * @brief Drain all pending objects from the queue.
   * @param callback Function to call for each object (e.g., to actually free it).
   * @return Number of objects drained.
   * @note Called by the owner thread only.
   */
  template <typename Callback>
  size_t drain(Callback&& callback) {
    size_t processed = 0;
    size_t currentTail = _tail.load(std::memory_order_relaxed);
    size_t currentHead = _head.load(std::memory_order_acquire);

    while (currentTail < currentHead) {
      size_t slot = currentTail & (FREE_QUEUE_CAPACITY - 1);

      // Read the pointer from this slot
      void* ptr = _slots[slot].load(std::memory_order_acquire);

      // The slot might not be written yet if a producer claimed it but
      // hasn't finished storing. Spin briefly waiting for it.
      if (ptr == nullptr) {
        // Producer hasn't finished writing yet. We could spin here,
        // but to avoid blocking, just stop draining and come back later.
        // The remaining items will be processed on the next drain.
        break;
      }

      // Clear the slot before processing (so we don't process twice)
      _slots[slot].store(nullptr, std::memory_order_relaxed);

      // Process the freed object
      callback(ptr);
      processed++;

      // Advance tail
      currentTail++;
    }

    // Update tail
    _tail.store(currentTail, std::memory_order_release);

    return processed;
  }

  /**
   * @brief Check if the queue has pending objects above the batch threshold.
   * @return true if count exceeds FREE_BATCH_SIZE.
   */
  bool shouldDrain() const {
    size_t head = _head.load(std::memory_order_relaxed);
    size_t tail = _tail.load(std::memory_order_relaxed);
    return (head - tail) >= FREE_BATCH_SIZE;
  }

  /**
   * @brief Get approximate count of pending objects.
   */
  size_t approximateCount() const {
    size_t head = _head.load(std::memory_order_relaxed);
    size_t tail = _tail.load(std::memory_order_relaxed);
    return head - tail;
  }

  /**
   * @brief Check if queue is full.
   */
  bool isFull() const {
    size_t head = _head.load(std::memory_order_relaxed);
    size_t tail = _tail.load(std::memory_order_relaxed);
    return (head - tail) >= FREE_QUEUE_CAPACITY;
  }

private:
  // Producer index - atomically incremented by multiple producers
  alignas(CACHE_LINE_SIZE) std::atomic<size_t> _head;

  // Consumer index - only modified by the owner thread
  alignas(CACHE_LINE_SIZE) std::atomic<size_t> _tail;

  // Ring buffer slots - each slot holds a pointer to a freed object
  // nullptr means the slot is empty
  alignas(CACHE_LINE_SIZE) std::atomic<void*> _slots[FREE_QUEUE_CAPACITY];
};

/**
 * @class GlobalFreePool
 * @brief Manages cross-thread frees using per-owner-thread queues.
 *
 * When thread A frees an object allocated by thread B, the object is
 * placed in thread B's queue. Thread B drains its queue periodically
 * (e.g., on allocation or when threshold is reached).
 */
class GlobalFreePool {
public:

  GlobalFreePool() = default;

  /**
   * @brief Register a free from a non-owner thread.
   * @param ptr The object being freed.
   * @param ownerThreadId The thread ID that originally allocated the object.
   * @return true if successfully queued, false if queue was full.
   * @note Thread-safe, lock-free. Does NOT modify ptr's memory.
   */
  bool crossThreadFree(void* ptr, size_t ownerThreadId) {
    if (ownerThreadId < MAX_THREADS) {
      return _queues[ownerThreadId].push(ptr);
    }
    // If ownerThreadId is invalid, return false (caller should handle)
    return false;
  }

  /**
   * @brief Drain pending cross-thread frees for the current thread.
   * @param threadId The calling thread's ID.
   * @param callback Function to call for each freed object.
   * @return Number of objects processed.
   */
  template <typename Callback>
  size_t drainForThread(size_t threadId, Callback&& callback) {
    if (threadId < MAX_THREADS) {
      return _queues[threadId].drain(std::forward<Callback>(callback));
    }
    return 0;
  }

  /**
   * @brief Check if thread should drain its queue.
   * @param threadId The thread's ID.
   * @return true if pending count exceeds threshold.
   */
  bool shouldDrain(size_t threadId) const {
    if (threadId < MAX_THREADS) {
      return _queues[threadId].shouldDrain();
    }
    return false;
  }

  /**
   * @brief Check if a thread's queue is full.
   * @param threadId The thread's ID.
   * @return true if queue is at capacity.
   */
  bool isQueueFull(size_t threadId) const {
    if (threadId < MAX_THREADS) {
      return _queues[threadId].isFull();
    }
    return true;
  }

  /**
   * @brief Get singleton instance of the global free pool.
   */
  static GlobalFreePool& getInstance() {
    static GlobalFreePool instance;
    return instance;
  }

private:
  // Disable copying
  GlobalFreePool(const GlobalFreePool&) = delete;
  GlobalFreePool& operator=(const GlobalFreePool&) = delete;

  /// Per-thread queues for cross-thread frees.
  PerThreadFreeQueue _queues[MAX_THREADS];
};

/**
 * @class ThreadIdManager
 * @brief Assigns unique thread IDs for the free pool.
 *
 * Allocates unique sequential IDs. The ID is cached in the heap
 * instance to avoid TLS bootstrap issues.
 */
class ThreadIdManager {
public:

  /**
   * @brief Allocate a new unique thread ID.
   * @return A unique ID in [0, MAX_THREADS).
   *
   * This should be called once per heap instance, not per operation.
   * The result should be cached by the caller.
   */
  static size_t allocateThreadId() {
    static std::atomic<size_t> nextId{0};
    size_t id = nextId.fetch_add(1, std::memory_order_relaxed);
    // Wrap around if we exceed MAX_THREADS
    return id % MAX_THREADS;
  }

  /**
   * @brief Get a thread ID - for compatibility, allocates a new one.
   * @note Prefer allocateThreadId() and caching in the heap instance.
   */
  static size_t getThreadId() {
    return allocateThreadId();
  }
};

/**
 * @class CrossThreadFreeWrapper
 * @brief Heap wrapper that handles cross-thread frees via GlobalFreePool.
 *
 * Wraps a heap and intercepts free() calls. If the object was allocated
 * by a different thread, it's queued for the owner. Otherwise, it's
 * freed directly.
 *
 * @tparam SuperHeap The underlying heap implementation.
 */
template <class SuperHeap>
class CrossThreadFreeWrapper : public SuperHeap {
public:

  CrossThreadFreeWrapper()
    : _threadId(ThreadIdManager::getThreadId())
  {
  }

  inline void* malloc(size_t sz) {
    // Before allocating, drain any pending cross-thread frees
    maybeDrainPending();
    return SuperHeap::malloc(sz);
  }

  inline bool free(void* ptr) {
    // For now, we don't have ownership tracking per-object,
    // so all frees go directly to SuperHeap.
    // Phase 6 will add ownership tracking.
    return SuperHeap::free(ptr);
  }

  inline size_t getSize(void* ptr) {
    return SuperHeap::getSize(ptr);
  }

protected:

  /**
   * @brief Drain pending cross-thread frees if threshold is reached.
   */
  void maybeDrainPending() {
    auto& pool = GlobalFreePool::getInstance();
    if (pool.shouldDrain(_threadId)) {
      pool.drainForThread(_threadId, [this](void* ptr) {
        SuperHeap::free(ptr);
      });
    }
  }

  /// This wrapper's thread ID.
  const size_t _threadId;
};

#endif // DH_GLOBALFREEPOOL_H
