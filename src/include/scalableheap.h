// -*- C++ -*-

/**
 * @file   scalableheap.h
 * @brief  Scalable per-thread heap with early initialization fallback.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 * @note   Copyright (C) 2024 by Emery Berger, University of Massachusetts Amherst.
 *
 * This file provides a scalable heap design that works correctly on macOS
 * with DYLD_INSERT_LIBRARIES. The key challenge is that malloc is called
 * very early during process startup (before main, during ObjC runtime init),
 * and pthread TLS may not be ready yet.
 *
 * Solution: Use a simple fallback heap during early initialization, then
 * switch to per-thread heaps once initialization is complete.
 *
 * The per-thread heap is expected to handle cross-thread frees via
 * OwnershipTrackingHeap and HeapRegistry - this class doesn't need to
 * implement ownership detection.
 */

#ifndef DH_SCALABLEHEAP_H
#define DH_SCALABLEHEAP_H

#include <atomic>
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <pthread.h>
#endif
#include "heaplayers.h"
#include "platformspecific.h"

// Platform-specific lock type
#if defined(_WIN32)
using ScalableHeap_LockType = HL::WinLockType;
#else
using ScalableHeap_LockType = HL::PosixLockType;
#endif

// Maximum number of per-thread heaps
#ifndef MAX_THREAD_HEAPS
#define MAX_THREAD_HEAPS 256
#endif

/**
 * @class ScalableHeap
 * @brief A scalable heap with safe early initialization.
 *
 * During early process startup (before pthread is ready), all allocations
 * go to a shared fallback heap protected by a lock. Once initialization
 * is complete, each thread gets its own heap for lock-free allocation.
 *
 * Free operations are always routed through the current thread's heap,
 * which is expected to handle ownership detection (via OwnershipTrackingHeap).
 *
 * @tparam PerThreadHeap The heap type to instantiate per-thread.
 * @tparam FallbackHeap The heap type to use during early init (should be locked).
 */
template <class PerThreadHeap, class FallbackHeap>
class ScalableHeap {
public:

  enum { Alignment = PerThreadHeap::Alignment };

  ScalableHeap() {}

  ATTRIBUTE_ALWAYS_INLINE void* malloc(size_t sz) {
    auto* heap = getHeapSafe();
    if (likely(heap != nullptr)) {
      return heap->malloc(sz);
    }
    // Fallback for early init (rare after startup)
    return getFallbackHeap()->malloc(sz);
  }

  ATTRIBUTE_ALWAYS_INLINE void free(void* ptr) {
    if (unlikely(ptr == nullptr)) return;

    // Try per-thread heap first (handles cross-thread frees via OwnershipTrackingHeap)
    auto* heap = getHeapSafe();
    if (likely(heap != nullptr)) {
      heap->free(ptr);
      return;
    }

    // During early init, free to fallback heap
    getFallbackHeap()->free(ptr);
  }

  inline size_t getSize(void* ptr) {
    if (unlikely(ptr == nullptr)) return 0;

    // Try per-thread heap first
    auto* heap = getHeapSafe();
    if (likely(heap != nullptr)) {
      size_t sz = heap->getSize(ptr);
      if (sz > 0) return sz;
    }

    // Check fallback heap
    return getFallbackHeap()->getSize(ptr);
  }

  inline void* memalign(size_t alignment, size_t sz) {
    auto* heap = getHeapSafe();
    if (likely(heap != nullptr)) {
      return heap->memalign(alignment, sz);
    }
    return getFallbackHeap()->memalign(alignment, sz);
  }

  inline void lock() {
    _fallbackLock.lock();
  }

  inline void unlock() {
    _fallbackLock.unlock();
  }

private:

  /**
   * @brief Get a per-thread heap, or nullptr if not safe yet.
   *
   * Returns nullptr during early initialization when TLS
   * is not ready. The caller should use the fallback heap in this case.
   *
   * @note Fast path: after initialization, just reads TLS and decodes pointer.
   */
  ATTRIBUTE_ALWAYS_INLINE PerThreadHeap* getHeapSafe() {
    // Fast path: after init, just get from TLS.
    // The relaxed load is safe because once true, _initialized never changes back.
    if (likely(_initialized.load(std::memory_order_relaxed))) {
      void* tlsValue = getTlsValue();
      if (likely(tlsValue != nullptr)) {
        // Decode the heap pointer (offset by 1 to distinguish from nullptr)
        return reinterpret_cast<PerThreadHeap*>(reinterpret_cast<uintptr_t>(tlsValue) - 1);
      }
      // First allocation on this thread - create a new heap
      return createHeapForThread();
    }

    // Slow path: initialization not complete
    return getHeapSlow();
  }

  /**
   * @brief Slow path for getHeapSafe when not yet initialized.
   */
  NO_INLINE PerThreadHeap* getHeapSlow() {
    if (!tryInitialize()) {
      return nullptr;
    }

    // Now initialized, get thread-local heap
    void* tlsValue = getTlsValue();
    if (tlsValue == nullptr) {
      return createHeapForThread();
    }
    return reinterpret_cast<PerThreadHeap*>(reinterpret_cast<uintptr_t>(tlsValue) - 1);
  }

  /**
   * @brief Try to initialize TLS.
   * @return true if initialization succeeded, false if not ready.
   */
  bool tryInitialize() {
    // Use a simple spinlock to serialize initialization attempts
    bool expected = false;
    if (!_initializing.compare_exchange_strong(expected, true,
                                                std::memory_order_acq_rel)) {
      // Another thread is initializing - spin briefly then use fallback
      for (int i = 0; i < 1000; i++) {
        if (_initialized.load(std::memory_order_acquire)) {
          return true;
        }
      }
      return false;
    }

    // Double-check after acquiring the "lock"
    if (_initialized.load(std::memory_order_acquire)) {
      _initializing.store(false, std::memory_order_release);
      return true;
    }

    // Try to create the TLS key
#if defined(_WIN32)
    _heapKey = TlsAlloc();
    if (_heapKey == TLS_OUT_OF_INDEXES) {
      // TLS not ready yet
      _initializing.store(false, std::memory_order_release);
      return false;
    }
#else
    int result = pthread_key_create(&_heapKey, nullptr);
    if (result != 0) {
      // pthread not ready yet
      _initializing.store(false, std::memory_order_release);
      return false;
    }
#endif

    // Success!
    _initialized.store(true, std::memory_order_release);
    _initializing.store(false, std::memory_order_release);
    return true;
  }

  /**
   * @brief Create a new heap for the current thread.
   */
  PerThreadHeap* createHeapForThread() {
    // Allocate heap using mmap (to avoid recursion)
    void* buf = HL::MmapWrapper::map(sizeof(PerThreadHeap));
#if defined(_WIN32)
    if (buf == nullptr) {
      return nullptr;
    }
#else
    if (buf == nullptr || buf == MAP_FAILED) {
      return nullptr;
    }
#endif

    auto* heap = new (buf) PerThreadHeap();

    // Store in TLS (add 1 to distinguish from nullptr)
    setTlsValue(reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(heap) + 1));

    return heap;
  }

  /**
   * @brief Get the fallback heap (used during early init).
   */
  FallbackHeap* getFallbackHeap() {
    // Use placement new in static buffer to avoid malloc during init
#if defined(_MSC_VER)
    __declspec(align(16)) static char buf[sizeof(FallbackHeap)];
#else
    static char buf[sizeof(FallbackHeap)] __attribute__((aligned(16)));
#endif
    static FallbackHeap* heap = new (buf) FallbackHeap();
    return heap;
  }

  // Platform-specific TLS accessors
  void* getTlsValue() {
#if defined(_WIN32)
    return TlsGetValue(_heapKey);
#else
    return pthread_getspecific(_heapKey);
#endif
  }

  void setTlsValue(void* value) {
#if defined(_WIN32)
    TlsSetValue(_heapKey, value);
#else
    pthread_setspecific(_heapKey, value);
#endif
  }

  // Initialization state
  std::atomic<bool> _initialized{false};
  std::atomic<bool> _initializing{false};

  // TLS key for per-thread heap lookup
#if defined(_WIN32)
  DWORD _heapKey;
#else
  pthread_key_t _heapKey;
#endif

  // Lock for fallback heap
  ScalableHeap_LockType _fallbackLock;
};

#endif // DH_SCALABLEHEAP_H
