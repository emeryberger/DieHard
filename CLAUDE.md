# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

DieHard is an error-resistant memory allocator that provides **probabilistic memory safety** for C/C++ programs. It tolerates memory errors (buffer overflows, dangling pointers, double frees, invalid frees) that would crash programs using standard allocators.

The repository contains three related systems:
- **DieHard**: Increases reliability by probabilistically avoiding memory errors
- **DieHarder**: Security-hardened variant with stronger randomization (CCS 2010)
- **Exterminator**: Automatically fixes memory errors using statistical inference (PLDI 2007)

## Build Commands

```bash
# Standard build
mkdir build && cd build
cmake ..
make

# Build with DieHarder (security-focused variant)
cmake -DBUILD_DIEHARDER=ON ..
make

# Build with scalable multi-threaded mode (per-thread heaps)
cmake -DBUILD_SCALABLE=ON ..
make

# Build with replicated mode support
cmake -DBUILD_DIEHARDER=ON -DBUILD_REPLICATED=ON ..
make

# Combine options as needed
cmake -DBUILD_DIEHARDER=ON -DBUILD_SCALABLE=ON ..
make
```

**Output libraries:**
- `libdiehard.so` / `libdiehard.dylib` - Main DieHard library
- `libdieharder.so` - Security-hardened variant (when enabled)
- `libdieharder_r.so` - Replicated variant for multi-replica execution

## Usage

```bash
# Linux: preload to replace malloc
LD_PRELOAD=$PWD/libdiehard.so ./your_application

# macOS: preload to replace malloc
DYLD_INSERT_LIBRARIES=$PWD/libdiehard.dylib ./your_application

# Replicated mode (Linux): run with N replicas for higher reliability
diehard 3 /path/to/libdiehard_r.so yourapp
```

## Architecture

### Core Heap Design

The allocator approximates "infinite-heap semantics" through randomization:

1. **Segregated size classes**: Heap partitioned into regions for power-of-two sizes (16B to 1MB on 64-bit; larger objects use mmap)
2. **Bitmap allocation**: One bit per object slot tracks allocation state (no per-object headers)
3. **Random placement**: Objects placed randomly within their size class region
4. **Metadata separation**: All heap metadata stored separately from heap objects (prevents heap corruption)

### Adaptive Miniheap Growth (Current Algorithm)

The current implementation uses an adaptive algorithm (different from the static heap in the original PLDI 2006 paper):

- Each size class maintains a collection of **miniheaps** that grow dynamically
- Miniheaps double in size: if allocation would exceed 1/M fullness, a new miniheap is created with 2x the capacity of the previous largest
- Objects are allocated randomly across all miniheaps for a given size class
- The heap multiplier M (default: Numerator=8, Denominator=7) ensures total capacity is M times the maximum live memory
- Random probing finds free slots in O(1) expected time since heap is at most 1/M full

This adaptive approach (from the Exterminator/PLDI 2007 work) avoids the need to pre-specify heap size while maintaining probabilistic safety guarantees.

### Key Source Files

- `src/source/libdieharder.cpp` - Main library entry point, defines `xxmalloc`/`xxfree`
- `src/include/diehardheap.h` - `DieHardHeap` class managing multiple size classes
- `src/include/randomheap.h` - `RandomHeap` managing mini-heaps for a specific size
- `src/include/randomminiheap.h` - Dispatches to DieHard or DieHarder mini-heap implementations
- `src/include/randomminiheap-diehard.h` - DieHard-specific allocation logic
- `src/include/randomminiheap-dieharder.h` - DieHarder-specific allocation with page table
- `src/include/util/bitmap.h` - Standard bitmap for allocation tracking
- `src/include/util/atomicbitmap.h` - Lock-free atomic bitmap (for scalable mode)
- `src/include/heapregistry.h` - Registry of per-thread heap instances (for scalable mode)
- `src/include/globalfreepool.h` - Cross-thread free pool (for scalable mode)
- `src/include/objectownership.h` - Ownership tracking heap wrapper (for scalable mode)

### Template Parameters

The heap is parameterized by:
- `Numerator/Denominator` - Heap expansion ratio (default 8/7, meaning heap is 8/7x max live size)
- `MaxSize` - Maximum object size for small object heap (default 1MB)
- `DieFastOn` - Enable fill patterns for overflow detection
- `DieHarderOn` - Enable security hardening with page tables

### Build Variants

Controlled by compile definitions:
- `DIEHARD_DIEHARDER=0` - Standard DieHard (reliability focused)
- `DIEHARD_DIEHARDER=1` - DieHarder (security focused)
- `DIEHARD_MULTITHREADED=1` - Thread-safe allocation
- `DIEHARD_REPLICATED=1` - Support for replica-based execution
- `DIEHARD_SCALABLE=1` - Scalable per-thread heap design

### Scalable Multi-Threaded Design (BUILD_SCALABLE=ON)

The scalable variant replaces the single global locked heap with per-thread heaps for improved multi-core performance:

**Architecture:**
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

**Key Components:**
- `src/include/util/atomicbitmap.h` - Lock-free bitmap using CAS operations
- `src/include/heapregistry.h` - Registry of per-thread heap instances for ownership detection
- `src/include/globalfreepool.h` - Per-owner-thread queues for cross-thread frees
- `src/include/objectownership.h` - Heap wrapper that routes cross-thread frees

**How it works:**
1. Each thread has its own `DieHardHeap` instance via `ThreadSpecificHeap`
2. Allocation uses `AtomicBitMap` with CAS for lock-free slot claiming
3. `RandomHeap` counters are atomic with cache-line alignment to prevent false sharing
4. On free, ownership is determined by querying each heap's `getSize()` (fast path checks own heap first)
5. Cross-thread frees are queued in the owner's `GlobalFreePool` queue
6. Owner thread drains its queue periodically (on allocation or when threshold reached)

**Performance:** The scalable mode achieves ~9x speedup over non-scalable at 8 threads.

**Probabilistic guarantees are preserved:** Each per-thread heap maintains the same randomization properties as the original design

### External Dependencies (fetched via CMake)

- [Heap-Layers](https://github.com/emeryberger/Heap-Layers) - Composable heap infrastructure
- [printf](https://github.com/emeryberger/printf) - Heap-safe printf implementation

## Technical Background

DieHard provides probabilistic guarantees against memory errors:

- **Buffer overflows**: Random placement means overflows likely hit empty space. With heap 1/2 full, ~50% chance single-object overflow is benign; with 3 replicas, >99% protection.
- **Dangling pointers**: Random reuse means freed objects unlikely to be immediately overwritten. For 8-byte objects with 10,000 intervening allocations, >99.5% safe.
- **Double/invalid frees**: Bitmap checks make these no-ops (completely eliminated).
- **Heap metadata corruption**: Impossible since metadata is segregated from heap data.

The replicated mode runs multiple processes with different random seeds; output is voted on to detect/mask errors including uninitialized reads.

## Platform Notes

- **Supported**: Linux, macOS (arm64, arm64e, x86_64)
- **Not supported**: Windows (currently)
- C++14 required
