/* -*- C++ -*- */

#ifndef __QIH_SIZEINMAPHEAP_H__
#define __QIH_SIZEINMAPHEAP_H__

/**
 * @file sizeinmapheap.h
 * @class SizeInMapHeap
 * @brief Keeps object sizes in a on-the-side map
 * @author Vitaliy Lvin
 */

#include "heaplayers.h"

#include "myassert.h"

namespace QIH {

  template <class SuperHeap, class MapHeap> 
  class SizeInMapHeap : public SuperHeap {
  private:

    HL::MyHashMap<void *, size_t, MapHeap> myMap;
      
    //    unsigned int liveObjects, liveBytes, maxLiveObjects, maxLiveBytes;  

  public:
    //SizeInMapHeap() : liveObjects(0), liveBytes(0), maxLiveObjects(0), maxLiveBytes(0) {}  

    inline void * malloc (size_t sz) {
      void * ptr = SuperHeap::malloc (sz);

      //ASSERT_LOG((ptr != NULL) || (sz == 0), "Malloc returned NULL to SizeInMapHeap");
     //  liveObjects += 1;
//       if (liveObjects > maxLiveObjects) {
// 	maxLiveObjects = liveObjects;
// 	logWrite(FATAL, "maxLiveObjects = %d\n", maxLiveObjects);
//       }

//       liveBytes += sz;
//       if (liveBytes > maxLiveBytes) {
// 	maxLiveBytes = liveBytes;
// 	logWrite(FATAL, "maxLiveBytes = %d\n", maxLiveBytes);
//       }

      myMap.set (ptr, sz);
      
      return ptr;
    }

    inline size_t getSize (void * ptr) {
      return SuperHeap::fixSize(myMap.get (ptr));
    }

	
    inline void free (void * ptr) {
      size_t sz = myMap.get (ptr);
      myMap.erase (ptr);
      
      //      liveObjects -= 1;
      //liveBytes   -= sz;
      //ASSERT_LOG(sz > 0, "Freeing unallocated object %p\n", ptr);

      SuperHeap::free (ptr, sz);
    }

    inline void free (void * ptr, size_t sz) {
      free(ptr);
    }
  };

}

#endif
