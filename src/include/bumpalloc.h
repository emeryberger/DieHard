#ifndef DH_BUMPALLOC_H
#define DH_BUMPALLOC_H

/**
 * @class BumpAlloc
 * @brief Obtains memory in chunks and bumps a pointer through the chunks.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 */

template <int ChunkSize,
	  class Super>
class BumpAlloc : public Super {
public:

  enum { Alignment = 1 };

  BumpAlloc (void)
    : _bump (NULL),
      _remaining (0)
  {}

  inline void * malloc (size_t sz) {
    // If there's not enough space left to fulfill this request, get
    // another chunk.
    if (_remaining < sz) {
      refill(sz);
    }
    char * old = _bump;
    _bump += sz;
    _remaining -= sz;
    return old;
  }

  /// Free is disabled (we only bump, never reclaim).
  inline bool free (void *) { return false; }

private:

  /// The bump pointer.
  char * _bump;

  /// How much space remains in the current chunk.
  size_t _remaining;

  // Get another chunk.
  void refill (size_t sz) {
    if (sz < ChunkSize) {
      sz = ChunkSize;
    }
    _bump = (char *) Super::malloc (sz);
    _remaining = sz;
  }

};

#endif
