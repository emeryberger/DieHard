#ifndef _REENTRANTHEAP_H_
#define _REENTRANTHEAP_H_

/**
 * @class ReentrantHeap
 * @brief Allocates from a static buffer if the heap has not yet been initialized.
 */

template <class Super, int BufferSize = 65536>
class ReentrantHeap : public Super {
public:
  ReentrantHeap (void)
    : _inMalloc (false),
      _bufferPosition (0),
      _remaining (BufferSize)
  {
  }

  inline void * malloc (size_t sz) {
    if (_inMalloc) {
      return nextChunk (sz);
    } else {
      _inMalloc = true;
      void * ptr = Super::malloc (sz);
      _inMalloc = false;
      return ptr;
    }
  }

  inline bool free (void * ptr) {
    if (!_inMalloc) {
      return Super::free (ptr);
    } else {
      return true;
    }
  }

private:

  void * nextChunk (size_t sz) {
    if (_remaining < sz) {
      return NULL;
    } else {
      void * ptr = &_buffer[_bufferPosition];
      _bufferPosition += sz;
      _remaining -= sz;
      return ptr;
    }
  }

  /// True if we're in malloc.
  bool _inMalloc;

  /// The static buffer for uninitialized allocations.
  char _buffer[BufferSize];

  /// Position in the allocation buffer.
  int _bufferPosition;

  /// How much space remains in the buffer.
  size_t _remaining;
};


#endif
