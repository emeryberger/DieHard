#if !defined(_WIN32)
#define TRUE true
#define FALSE false
#define ULONG unsigned long
#define BOOL bool
#define HANDLE void *
#define WINAPI
#define LPVOID void *
#define DWORD unsigned long
#define SIZE_T size_t
#define LPCVOID const void *
#endif

#if PRINT_STATS
extern int mallocs;
extern float mallocSize;
#endif

#define DONT_REPLACE 0

extern "C" {

  HANDLE (WINAPI * real_GetProcessHeap) ();
  HANDLE (WINAPI * real_HeapCreate) (DWORD, SIZE_T, SIZE_T);
  BOOL (WINAPI * real_HeapDestroy) (HANDLE);
  LPVOID (WINAPI * real_HeapAlloc) (HANDLE, DWORD, SIZE_T);
  BOOL (WINAPI * real_HeapFree) (HANDLE, DWORD, LPVOID);
  SIZE_T (WINAPI * real_HeapSize) (HANDLE, ULONG, LPCVOID);
  LPVOID (WINAPI * real_HeapReAlloc) (HANDLE, DWORD, LPVOID, SIZE_T);

  HANDLE WINAPI local_GetProcessHeap (void) {
#if DONT_REPLACE
    return real_GetProcessHeap();
#else
    return real_GetProcessHeap();
    //    return (HANDLE) getCustomHeap();
#endif
  }

  HANDLE WINAPI local_HeapCreate (DWORD d, SIZE_T s1, SIZE_T s2)
  {
#if PRINT_STATS
    mallocs++;
    mallocSize += 1.0;
#endif
#if DONT_REPLACE
    return real_HeapCreate (d, s1, s2);
#else
    // note - this makes no apparent diff.
    void * ptr = VirtualAlloc (NULL, sizeof(TheCustomHeapType), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    void * h = (void *) new (ptr) TheCustomHeapType;
    return (HANDLE) h;
#endif
  }

  BOOL WINAPI local_HeapDestroy (HANDLE h)
  {
#if DONT_REPLACE
    return real_HeapDestroy (h);
#else
    if (h != local_GetProcessHeap()) {
      // FIX ME // delete ((TheCustomHeapType *) h);
      return TRUE;
    } else {
      return FALSE;
    }
#endif
  }

  LPVOID WINAPI local_HeapAlloc (HANDLE h, DWORD flags, SIZE_T sz) {
#if DONT_REPLACE
    if (h != local_GetProcessHeap()) {
#if PRINT_STATS
      mallocs++;
      mallocSize += sz;
#endif
    }
    return real_HeapAlloc (h, flags, sz);
#else
    if (h == local_GetProcessHeap()) {
      return real_HeapAlloc (h, flags, sz);
    } else {
#if PRINT_STATS
      mallocs++;
      mallocSize += sz;
#endif
      void * ptr = ((TheCustomHeapType *) h)->malloc (sz);
#if defined(_WIN32)
      if (flags & HEAP_ZERO_MEMORY) {
	memset (ptr, 0, sz);
      }
#endif
      return ptr;
    }
#endif
  }
  
  BOOL WINAPI local_HeapFree (HANDLE h, DWORD flags, LPVOID ptr) {
#if DONT_REPLACE
    return real_HeapFree (h, flags, ptr);
#else
    if (h == local_GetProcessHeap()) {
      return real_HeapFree (h, flags, ptr);
    } else {
      return ((TheCustomHeapType *) h)->free (ptr);
    }
#endif
  }
  
  SIZE_T WINAPI local_HeapSize (HANDLE h, ULONG flags, LPCVOID ptr) {
#if DONT_REPLACE
    return real_HeapSize (h, flags, ptr);
#else
    if (h == local_GetProcessHeap()) {
      return real_HeapSize (h, flags, ptr);
    } else {
      return ((TheCustomHeapType *) h)->getSize ((void *) ptr);
    }
#endif
  }
  
  LPVOID WINAPI local_HeapReAlloc (HANDLE h, DWORD flags, LPVOID ptr, SIZE_T sz) {
#if DONT_REPLACE
    return real_HeapReAlloc (h, flags, ptr, sz);
#else
    if (h == local_GetProcessHeap()) {
      return real_HeapReAlloc (h, flags, ptr, sz);
    }
    if (sz == 0) {
      local_HeapFree(h, 0, ptr);
      return NULL;
    }
    if (ptr == NULL) {
      return local_HeapAlloc (h, flags, sz);
    }
#if defined(_WIN32)
    if (flags & HEAP_REALLOC_IN_PLACE_ONLY) {
      return NULL;
    }
#endif
    size_t actualSize = local_HeapSize (h, flags, ptr);
    void * newptr = local_HeapAlloc (h, flags, sz);
    if (newptr) {
      size_t bytesToCopy = (actualSize < sz) ? actualSize : sz;
      memcpy (newptr, ptr, bytesToCopy);
    }
    local_HeapFree (h, 0, ptr);
    return newptr;
#endif
  }

  BOOL WINAPI local_HeapValidate (HANDLE hHeap,
				  DWORD dwFlags,
				  LPCVOID lpMem)
  {
    return true;
  }

  BOOL WINAPI local_HeapWalk (HANDLE hHeap,
			      LPPROCESS_HEAP_ENTRY lpEntry)
  {
    return 0;
  }
  

  SIZE_T WINAPI local_HeapCompact (HANDLE hHeap, DWORD dwFlags) {
    return 0;
  };

  BOOL WINAPI local_HeapLock (HANDLE hHeap) {
    return true;
  }

  BOOL WINAPI local_HeapUnlock (HANDLE hHeap) {
    return true;
  }

  BOOL WINAPI local_HeapSetInformation (HANDLE HeapHandle,
					HEAP_INFORMATION_CLASS HeapInformationClass,
					PVOID HeapInformation,
					SIZE_T HeapInformationLength)
  {
    return 0;
  }

  BOOL WINAPI local_HeapQueryInformation(HANDLE HeapHandle,
					 HEAP_INFORMATION_CLASS HeapInformationClass,
					 PVOID HeapInformation,
					 SIZE_T HeapInformationLength,
					 PSIZE_T ReturnLength) {
    return 0;
  }

}
