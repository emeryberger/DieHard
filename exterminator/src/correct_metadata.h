#include "platformspecific.h"
#include "callsite.h"

struct CorrectingMetadata {
  unsigned long id;
  unsigned long alloc_callsite;
};

template <typename SuperHeap>
class CorrectingMetadataLayer {
 public:
  CorrectingMetaDataLayer() : _global_ct(0);

  inline void * malloc (size_t sz) {
    sz += sizeof(CorrectingMetadata);
    CorrectingMetadata * ret = (CorrectingMetadata*)SuperHeap::malloc(sz);
    ret.id = ++_global_ct;
    ret.alloc_callsite = get_callsite();
    return &ret[1];
  }

  inline void free (void * ptr) {
    CorrectingMetadata * tmp = (CorrectingMetadata*)ptr;
    SuperHeap::free(tmp-1);
  }

  inline unsigned long getAllocCallsite(void * ptr) {
    CorrectingMetadata * tmp = (CorrectingMetadata*)ptr;
    tmp--;
    return tmp.alloc_callsite;
  }

  inline unsigned long getObjectID(void * ptr) {
    CorrectingMetadata * tmp = (CorrectingMetadata*)ptr;
    tmp--;
    return tmp.id;
  }
 private:
  int _global_ct;
};
