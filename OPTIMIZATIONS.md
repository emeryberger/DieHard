# DieHard Performance Optimizations

This document describes the performance optimizations implemented in DieHard to minimize overhead while maintaining probabilistic memory safety guarantees.

## Overview

DieHard provides probabilistic protection against memory errors (buffer overflows, dangling pointers, double frees) through randomized allocation. The optimizations described here reduce the performance overhead of this randomization, particularly for multi-threaded workloads.

## Scalable Multi-Threaded Architecture

### Per-Thread Heaps (Scalable Mode)

The default build uses a scalable design with per-thread heaps instead of a single global locked heap:

```
Thread[0]                    Thread[1]                    Thread[N]
    |                            |                            |
    v                            v                            v
PerThreadHeap[0]            PerThreadHeap[1]            PerThreadHeap[N]
(lock-free alloc)           (lock-free alloc)           (lock-free alloc)
    |                            |                            |
    +----------------------------+----------------------------+
                                 |
                                 v
                         GlobalFreePool
                    (cross-thread frees batched
                     and redistributed to owners)
```

**Key components:**
- `ScalableHeap` - Thread-local heap management via `pthread_getspecific`
- `AtomicBitMap` - Lock-free bitmap using CAS for slot allocation
- `GlobalFreePool` - Per-owner-thread queues for cross-thread frees
- `OwnershipTrackingHeap` - Routes frees to correct owner heap

**Performance impact:** ~9x speedup over non-scalable mode at 8 threads.

### Lock-Free Bitmap Allocation

The `AtomicBitMap` class provides thread-safe, lock-free allocation:

```cpp
inline bool tryToSet(unsigned long long index) {
    // Atomic CAS loop to claim a bit
    WORD oldvalue = _bitarray[item].load(std::memory_order_relaxed);
    do {
        if (oldvalue & mask) return false;  // Already set
        newvalue = oldvalue | mask;
    } while (!_bitarray[item].compare_exchange_weak(oldvalue, newvalue,
                                                     std::memory_order_acq_rel,
                                                     std::memory_order_relaxed));
    return true;
}
```

### Cache Line Alignment

Atomic counters are aligned to cache lines to prevent false sharing:

```cpp
alignas(CACHE_LINE_SIZE) std::atomic<size_t> _available;
alignas(CACHE_LINE_SIZE) std::atomic<size_t> _inUse;
alignas(CACHE_LINE_SIZE) std::atomic<unsigned int> _miniHeapsInUse;
```

## Allocation Path Optimizations

### Size Class Caching

`DieHardHeap::free()` caches the last-used size class to exploit locality:

```cpp
inline bool free(void* ptr) {
    // Fast path: try cached size class first
    int cached = _cachedSizeClass;
    if (likely(cached >= 0 && cached < MAX_INDEX)) {
        if (getHeap(cached)->free(ptr)) {
            return true;
        }
    }
    // Slow path: search all size classes
    for (int i = 0; i < MAX_INDEX; i++) {
        if (i == cached) continue;
        if (getHeap(i)->free(ptr)) {
            _cachedSizeClass = i;
            return true;
        }
    }
    return false;
}
```

### Miniheap Caching

`RandomHeap::free()` caches the last-used miniheap:

```cpp
inline bool free(void* ptr) {
    int cached = _cachedMiniHeap;
    if (likely(cached >= 0 && cached < numHeaps)) {
        if (getMiniHeap(cached)->free(ptr)) {
            _inUse.fetch_sub(1, std::memory_order_relaxed);
            return true;
        }
    }
    // Fall back to largest-first search (O(1) expected time)
    ...
}
```

### Eliminated Redundant Operations

`OwnershipTrackingHeap::free()` was optimized to avoid calling both `getSize()` and `free()`:

```cpp
// Before: Called getSize() then free() - each iterates through size classes
// After: Call free() directly which returns success/failure
inline bool free(void* ptr) {
    if (likely(SuperHeap::free(ptr))) {
        return true;
    }
    return freeSlow(ptr);  // Cross-thread free path
}
```

### Branch Prediction Hints

Hot paths use `likely()`/`unlikely()` macros:

```cpp
if (unlikely(Numerator * inUse >= available * Denominator)) {
    growHeapIfNeeded();  // Slow path
}
```

### Forced Inlining

Critical functions use `ATTRIBUTE_ALWAYS_INLINE`:

```cpp
ATTRIBUTE_ALWAYS_INLINE void* malloc(size_t sz) { ... }
ATTRIBUTE_ALWAYS_INLINE bool free(void* ptr) { ... }
```

## Random Number Generator

### Wyrand RNG

DieHard uses Wyrand, a fast high-quality PRNG:

```cpp
inline uint64_t next() {
    _state += 0xa0761d6478bd642fULL;
    __uint128_t tmp = (__uint128_t)_state * (_state ^ 0xe7037ed1a0b428dbULL);
    return (uint64_t)((tmp >> 64) ^ tmp);
}
```

**Selection rationale:** Wyrand outperforms MWC64 for larger working sets due to better statistical quality (fewer bitmap collisions), despite slightly higher per-call cost.

| Working Set | MWC64 | Wyrand |
|-------------|-------|--------|
| 100 objects | 16.5 ns | 17.0 ns |
| 1000 objects | 17.0 ns | 17.6 ns |
| 100K objects | 72.1 ns | 74.5 ns |

For workloads with 1000+ objects per thread, Wyrand's better distribution reduces retry attempts.

## Miniheap Search Strategy

### O(1) Expected Time Lookup

The largest-to-smallest miniheap search provides O(1) expected time:

- Miniheaps grow exponentially: 1K, 2K, 4K, 8K, ... objects
- ~50% of objects are in the largest heap (1 check)
- ~25% in second-largest (2 checks)
- Expected checks: 0.5×1 + 0.25×2 + 0.125×3 + ... ≈ 2.0

Binary search was evaluated but provides only ~10% improvement over this approach.

## C++ Standard Optimizations

### C++20 Features

- `std::bit_width` for efficient log2 computation
- `std::has_single_bit` for power-of-two validation
- `[[likely]]` / `[[unlikely]]` attributes (via macros for compatibility)

### C++23 Features

- `std::unreachable()` via `DH_UNREACHABLE()` macro for optimization hints
- Auto-detected in CMake with fallback to C++20

### Modern C++ Practices

- `static constexpr` instead of enum for compile-time constants
- `constexpr` functions where applicable
- Proper memory ordering for atomics

## Build Configuration

### Link-Time Optimization (LTO)

CMake enables interprocedural optimization when supported:

```cmake
include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported)
if(ipo_supported)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()
```

### Compiler Flags

- `-fno-builtin-malloc` etc. to prevent compiler from replacing allocation calls
- `-fvisibility=hidden` to reduce symbol table size

## Performance Characteristics

### Benchmark Results (Apple M-series, 8 threads)

**threadtest (8 threads, 5M iterations, 64 objects):**
- Native allocator: ~0.007s
- DieHard: ~1.0s

**threadtest (1 thread, 250K iterations, 1000 objects):**
- Native allocator: ~3.6s
- DieHard: ~4.5s (1.25x overhead)

### Overhead by Working Set Size

| Working Set | Native | DieHard | Overhead |
|-------------|--------|---------|----------|
| 100 | 14.5 ns | 16.5 ns | 1.14x |
| 1,000 | 14.9 ns | 17.0 ns | 1.14x |
| 10,000 | 15.2 ns | 35.0 ns | 2.30x |
| 100,000 | 16.9 ns | 72.1 ns | 4.27x |

Small working sets achieve near-native performance. Larger working sets incur overhead from miniheap management.

## Design Tradeoffs

### Preserved Guarantees

All optimizations maintain DieHard's probabilistic safety guarantees:
- Random object placement within size classes
- Bitmap-based allocation (no inline metadata)
- Separated heap metadata (corruption resistant)

### Inherent Costs

Some overhead is fundamental to DieHard's design:
- Atomic CAS for thread-safe bitmap (~4ns per operation)
- Random number generation (~1ns per allocation)
- Miniheap search on free (~2 checks expected)

These costs provide the probabilistic protection against memory errors that is DieHard's core value proposition.
