
/*
 * Main library file for DieHard Near-Inifinite Heap
 * @author Vitaliy Lvin
 */

#include <new>

#include "log.h"
#include "objectperpageheap.h"
#include "sparseheaps.h"
#include "signals.h"
#include "mman.h"

#define QIH_VERSION_MAJOR 0
#define QIH_VERSION_MINOR 1

namespace QIH {
  void initialize() {
  
    stdoutWrite(INFO, "***************************************** ***\n");
    stdoutWrite(INFO, "Archipelago memory allocator, version %d.%d ***\n", QIH_VERSION_MAJOR, QIH_VERSION_MINOR);
      
    install_handler(SIGSEGV, sigsegv_handler, SA_NODEFER);

#if !defined(OPP_ONLY_HEAP) && defined(VMSIG)
    //stdoutWrite(INFO, "Installing SIGVM handler...\n");
    install_handler(SIGVM, sigvm_handler, 0);
#endif
    
    stdoutWrite(INFO, "***************************************** ***\n");    
  }
}

volatile int anyThreadCreated = 1;

typedef QIH::ObjectPerPageHeap TheCustomHeapType;


/* This function is C++ magic that ensures that the heap is
     initialized before its first use. */
static TheCustomHeapType *getCustomHeap() {

#ifdef REPLACE_SIGACTION

  TheCustomHeapType::getInstance()->initialize(QIH::initialize);

#else
  static bool ready = false;

  if (!ready) {
    QIH::initialize();
    ready = true;
  }

#endif
  
  return TheCustomHeapType::getInstance();
}



namespace QIH {

  unsigned int getAllocationClock() {
    return getCustomHeap()->getAllocationClock();
  }

  bool tryRestorePage(void *ptr) {
#ifdef OPP_ONLY_HEAP
    return false;
#else
    return /*ObjectPerPageHeap::getInstance()*/
      getCustomHeap()->tryRestorePage(ptr);
#endif
  }

#ifndef OPP_ONLY_HEAP
  bool pageUnused(void *page) {
    return getCustomHeap()->isManaged(page) && !getCustomHeap()->isAllocated(page);
  }

#if defined(VMSIG) //|| defined(IV)
  int getVictimBuffer(void **buf) {
    return getCustomHeap()->getVictimBuffer(buf);
  }

  void clearVictimBuffer() {
    getCustomHeap()->clearVictimBuffer();
  }
#endif
#endif

}



// #include "wrapper.cpp" //generic file that overrides standard functions
