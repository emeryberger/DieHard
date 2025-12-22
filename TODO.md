# Scalable Multi-Threaded DieHard

**Status: COMPLETE** - Build with `cmake -DBUILD_SCALABLE=ON ..`

## Performance Results

| Threads | Non-scalable | Scalable | Speedup |
|---------|--------------|----------|---------|
| 1       | 0.23s        | 0.21s    | 1.1x    |
| 2       | 0.40s        | 0.10s    | 4.0x    |
| 4       | 0.56s        | 0.05s    | 11.2x   |
| 8       | 0.53s        | 0.06s    | 8.8x    |

*(threadtest benchmark: 50 iterations, 30000 objects, 64-byte allocations)*

## Problem Statement

The original DieHard uses a single global `LockedHeap<PosixLockType>` protecting the entire heap. All malloc/free operations serialize on this lock, causing severe contention on multi-core systems.

## Solution

Per-thread heaps with heap-based ownership detection:

1. Each thread gets its own `DieHardHeap` via `ThreadSpecificHeap`
2. Ownership is determined by querying heap structure (`getSize()`), not per-object tracking
3. `HeapRegistry` tracks heap instances for cross-thread lookups
4. Cross-thread frees are batched via `GlobalFreePool`

## Architecture

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
                         HeapRegistry
                    (tracks heap instances for
                     cross-thread ownership lookup)
                                 |
                                 v
                       GlobalFreePool (batched)
                    (cross-thread frees collected here,
                     redistributed to owner heaps periodically)
```

## Implementation Summary

### Phase 1: Atomic Bitmap ✓

**File:** `src/include/util/atomicbitmap.h`

Lock-free bitmap using CAS operations for thread-safe slot allocation.

### Phase 2: Atomic Counters in RandomHeap ✓

**File:** `src/include/randomheap.h`

Converted shared counters to `std::atomic` with cache-line alignment to prevent false sharing.

### Phase 3: Template Parameter for Bitmap Type ✓

**Files:** `randomminiheap*.h`, `randomheap.h`, `diehardheap.h`

Added `BitMapType` template parameter to choose between `BitMap` and `AtomicBitMap`.

### Phase 4: Global Free Pool ✓

**File:** `src/include/globalfreepool.h`

Lock-free per-thread queues for cross-thread frees with batching.

### Phase 5: Per-Thread Heap Wrapper ✓

**File:** `src/source/libdieharder.cpp`

`ThreadSpecificHeap` composition for per-thread heap instances.

### Phase 6: Heap-Based Ownership Detection ✓

**Files:** `src/include/heapregistry.h`, `src/include/objectownership.h`

- `HeapRegistry` - Simple array of per-thread heap pointers indexed by thread ID
- `OwnershipTrackingHeap` - Wrapper that:
  - Checks own heap first via `getSize()` (fast path, O(1))
  - Searches `HeapRegistry` for cross-thread objects (slow path, O(T))
  - Routes cross-thread frees to `GlobalFreePool`

**Key insight:** Using heap structure for ownership detection avoids the scalability bottleneck of a global per-object hash table. The original `OwnershipTable` (16MB hash table) was replaced with a lightweight `HeapRegistry` (array of heap pointers).

## Modified Files

| File | Changes |
|------|---------|
| `src/include/util/atomicbitmap.h` | NEW - Lock-free bitmap with CAS operations |
| `src/include/heapregistry.h` | NEW - Registry of per-thread heap instances |
| `src/include/globalfreepool.h` | NEW - Cross-thread free pool with per-thread queues |
| `src/include/objectownership.h` | NEW - Ownership tracking heap wrapper |
| `src/include/randomheap.h` | Atomic counters, cache-line alignment, growth spinlock |
| `src/include/randomminiheap-core.h` | BitMapType template parameter |
| `src/include/randomminiheap-diehard.h` | BitMapType template parameter passthrough |
| `src/include/randomminiheap-dieharder.h` | BitMapType template parameter passthrough |
| `src/include/randomminiheap.h` | BitMapType template parameter passthrough |
| `src/include/diehardheap.h` | BitMapType template parameter passthrough |
| `src/source/libdieharder.cpp` | Conditional scalable heap composition |
| `CMakeLists.txt` | BUILD_SCALABLE option for all targets |

## Probabilistic Guarantees

All original guarantees are preserved:

| Guarantee | Status |
|-----------|--------|
| Buffer Overflow Protection | Preserved (per-thread randomization) |
| Dangling Pointer Protection | Preserved (per-thread randomization) |
| Double Free Detection | Preserved (bitmap checks) |
| Invalid Free Detection | Preserved (bounds checks) |
| Cross-Thread Safety | Enhanced (natural isolation + free pool) |

## Build Instructions

```bash
# Standard build (original locked design)
mkdir build && cd build
cmake ..
make

# Scalable build (per-thread heaps)
cmake -DBUILD_SCALABLE=ON ..
make

# Scalable DieHarder build
cmake -DBUILD_DIEHARDER=ON -DBUILD_SCALABLE=ON ..
make
```

## Future Improvements

- [ ] Add `getSizeFromAnyHeap` to `getSize()` for complete cross-thread size queries
- [ ] Consider thread exit cleanup (unregister from HeapRegistry)
- [ ] Profile with larger thread counts (>64 threads)
- [ ] Benchmark with real-world applications
