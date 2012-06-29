/**
 * @file   winhard.cpp
 * @brief  For Windows: source for the Windows DieHard library (.lib,.dll).
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 * @note   Copyright (C) 2005 by Emery Berger, University of Massachusetts Amherst.
 *
 **/

/*

Compile with compile-winhard.cmd.

*/


#include <windows.h>

#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0500

#pragma inline_depth(255)

#pragma warning(disable: 4273)
#pragma warning(disable: 4098)  // Library conflict.
#pragma warning(disable: 4355)  // 'this' used in base member initializer list.
#pragma warning(disable: 4074)	// initializers put in compiler reserved area.

//#pragma init_seg(compiler)
#pragma comment(linker, "/merge:.CRT=.data")
#pragma comment(linker, "/disallowlib:libc.lib")
#pragma comment(linker, "/disallowlib:libcd.lib")
#pragma comment(linker, "/disallowlib:libcmt.lib")
#pragma comment(linker, "/disallowlib:libcmtd.lib")
#pragma comment(linker, "/disallowlib:msvcrtd.lib")

void (*hard_memcpy_ptr)(void *dest, const void *src, size_t count);
void (*hard_memset_ptr)(void *dest, int c, size_t count);

const char *RlsCRTLibraryName[] = {"MSVCR71.DLL", "MSVCR80.DLL"};
const int RlsCRTLibraryNameLength = 2;
const char *DbgCRTLibraryName = "MSVCRTD.DLL";

#define IAX86_NEARJMP_OPCODE	  0xe9
#define MakeIAX86Offset(to,from)  ((unsigned)((char*)(to)-(char*)(from)) - 5)

typedef struct
{
  const char *import;		// import name of patch routine
  FARPROC replacement;		// pointer to replacement function
  FARPROC original;		// pointer to original function
  unsigned char codebytes[5];	// 5 bytes of original code storage
} PATCH;


bool PatchMeIn (void);

#define CUSTOM_PREFIX(n) hard_##n
#define DIEHARD_PRE_ACTION { DisableThreadLibraryCalls ((HMODULE)hinstDLL); PatchMeIn();}
#define DIEHARD_POST_ACTION { }  // {HeapAlloc (GetProcessHeap(), 0, 1); }

#define CUSTOM_DLLNAME HardDllMain

#include "libdiehard.cpp"

// Intercept the exit functions.

static const int DIEHARD_MAX_EXIT_FUNCTIONS = 255;
static int exitCount = 0;

extern "C" {

  __declspec(dllexport) int ReferenceDieHardStub;

  typedef void (*exitFunctionType) (void);
  exitFunctionType exitFunctionBuffer[DIEHARD_MAX_EXIT_FUNCTIONS];

  void hard_onexit (void (*function)(void)) {
    if (exitCount < DIEHARD_MAX_EXIT_FUNCTIONS) {
      exitFunctionBuffer[exitCount] = function;
      exitCount++;
    }
  }

  void hard_exit (int code) {
    while (exitCount > 0) {
      exitCount--;
      (exitFunctionBuffer[exitCount])();
    }
  }

  void * hard_expand (void * ptr) {
    return NULL;
  }

}


/* ------------------------------------------------------------------------ */

static PATCH rls_patches[] = 
  {
    // RELEASE CRT library routines supported by this memory manager.

#if 0
    {"_heapchk",	(FARPROC) hard__heapchk,	0},
    {"_heapmin",	(FARPROC) hard__heapmin,	0},
    {"_heapset",	(FARPROC) hard__heapset,	0},
    {"_heapwalk",	(FARPROC) hard__heapwalk,	0},
#endif

    {"_expand",		(FARPROC) hard_expand,	   0},
    {"_onexit",         (FARPROC) hard_onexit,    0},
    {"_exit",           (FARPROC) hard_exit,      0},

    // operator new, new[], delete, delete[].

    {"??2@YAPAXI@Z",    (FARPROC) hard_malloc,    0},
    {"??_U@YAPAXI@Z",   (FARPROC) hard_malloc,    0},
    {"??3@YAXPAX@Z",    (FARPROC) hard_free,      0},
    {"??_V@YAXPAX@Z",   (FARPROC) hard_free,      0},

    // the nothrow variants new, new[].

    {"??2@YAPAXIABUnothrow_t@std@@@Z",  (FARPROC) hard_malloc, 0},
    {"??_U@YAPAXIABUnothrow_t@std@@@Z", (FARPROC) hard_malloc, 0},

    {"_msize",	(FARPROC) hard_malloc_usable_size,		0},
    {"calloc",	(FARPROC) hard_calloc,		0},
    {"malloc",	(FARPROC) hard_malloc,		0},
    {"realloc",	(FARPROC) hard_realloc,		0},
    {"free",	(FARPROC) hard_free,              0}
  };

#if 0 // def _DEBUG
static PATCH dbg_patches[] = 
  {
    // DEBUG CRT library routines supported by this memory manager.

    {"_calloc_dbg",               (FARPROC) hard__calloc_dbg,0},
    {"_CrtCheckMemory",	          (FARPROC) hard__CrtCheckMemory,	0},
    {"_CrtDoForAllClientObjects", (FARPROC) hard__CrtDoForAllClientObjects, 0},
    {"_CrtDumpMemoryLeaks",       (FARPROC) hard__CrtDumpMemoryLeaks, 0},
    {"_CrtIsMemoryBlock",         (FARPROC) hard__CrtIsMemoryBlock, 0},
    {"_CrtIsValidHeapPointer",	  (FARPROC) hard__CrtIsValidHeapPointer, 0},
    {"_CrtMemCheckpoint",         (FARPROC) hard__CrtMemCheckpoint, 0},
    {"_CrtMemDifference",         (FARPROC) hard__CrtMemDifference, 0},
    {"_CrtMemDumpAllObjectsSince",(FARPROC) hard__CrtMemDumpAllObjectsSince, 0},
    {"_CrtMemDumpStatistics",	  (FARPROC) hard__CrtMemDumpStatistics, 0},
    {"_CrtSetAllocHook",	  (FARPROC) hard__CrtSetAllocHook, 0},
    {"_CrtSetBreakAlloc",         (FARPROC) hard__CrtSetBreakAlloc,0},
    {"_CrtSetDbgFlag",	          (FARPROC) hard__CrtSetDbgFlag, 0},
    {"_CrtSetDumpClient",(FARPROC) hard__CrtSetDumpClient, 0},
    {"_expand",		 (FARPROC) hard__expand, 0},
    {"_expand_dbg",      (FARPROC) hard__expand_dbg, 0},
    {"_free_dbg",	 (FARPROC) hard__free_dbg, 0},
    {"_malloc_dbg",      (FARPROC) hard__malloc_dbg, 0},
    {"_msize",		 (FARPROC) hard__msize, 0},
    {"_msize_dbg",	 (FARPROC) hard__msize_dbg, 0},
    {"_realloc_dbg",     (FARPROC) hard__realloc_dbg, 0},
    {"_heapchk",	 (FARPROC) hard__heapchk,	0},
    {"_heapmin",	 (FARPROC) hard__heapmin,	0},
    {"_heapset",	 (FARPROC) hard__heapset,	0},
    {"_heapwalk",	 (FARPROC) hard__heapwalk, 0},
    {"_msize",		 (FARPROC) hard__msize, 0},
    {"calloc",		 (FARPROC) hard_calloc, 0},
    {"malloc",		 (FARPROC) hard_malloc, 0},
    {"realloc",		 (FARPROC) hard_realloc, 0},
    {"free",             (FARPROC) hard_free, 0},

    // operator new, new[], delete, delete[].

    {"??2@YAPAXI@Z",     (FARPROC) hard_malloc, 0},
    {"??_U@YAPAXI@Z",    (FARPROC) hard_malloc, 0},
    {"??3@YAXPAX@Z",     (FARPROC) hard_free,   0},
    {"??_V@YAXPAX@Z",    (FARPROC) hard_free,   0},

    // the nothrow variants new, new[].

    {"??2@YAPAXIABUnothrow_t@std@@@Z",  (FARPROC) hard_new_nothrow, 0},
    {"??_U@YAPAXIABUnothrow_t@std@@@Z", (FARPROC) hard_new_nothrow, 0},

    // The debug versions of operator new & delete.

    {"??2@YAPAXIHPBDH@Z", (FARPROC) hard_debug_operator_new, 0},
    {"??3@YAXPAXHPBDH@Z", (FARPROC) hard_debug_operator_delete, 0},
    // And the nh_malloc_foo.

    {"_nh_malloc_dbg",   (FARPROC)hard_nh_malloc_dbg, 0},
  };
#endif


static void PatchIt (PATCH *patch)
{
  // Change rights on CRT Library module to execute/read/write.

  MEMORY_BASIC_INFORMATION mbi_thunk;
  VirtualQuery((void*)patch->original, &mbi_thunk, 
	       sizeof(MEMORY_BASIC_INFORMATION));
  VirtualProtect(mbi_thunk.BaseAddress, mbi_thunk.RegionSize, 
		 PAGE_EXECUTE_READWRITE, &mbi_thunk.Protect);

  // Patch CRT library original routine:
  // 	save original 5 code bytes for exit restoration
  //		write jmp <patch_routine> (5 bytes long) to original.

  memcpy(patch->codebytes, patch->original, sizeof(patch->codebytes));
  unsigned char *patchloc = (unsigned char*)patch->original;
  *patchloc++ = IAX86_NEARJMP_OPCODE;
  *(unsigned*)patchloc = MakeIAX86Offset(patch->replacement, patch->original);
	
  // Reset CRT library code to original page protection.

  VirtualProtect(mbi_thunk.BaseAddress, mbi_thunk.RegionSize, 
		 mbi_thunk.Protect, &mbi_thunk.Protect);
}


static bool PatchMeIn (void)
{
  // acquire the module handles for the CRT libraries (release and debug)
  for (int i = 0; i < RlsCRTLibraryNameLength; i++) {

    HMODULE RlsCRTLibrary = GetModuleHandle(RlsCRTLibraryName[i]);

#ifdef _DEBUG
    HMODULE DbgCRTLibrary = GetModuleHandle(DbgCRTLibraryName);
#endif
    
    HMODULE DefCRTLibrary = 
#ifdef _DEBUG
      DbgCRTLibrary? DbgCRTLibrary: 
#endif	
      RlsCRTLibrary;

    // assign function pointers for required CRT support functions
    if (DefCRTLibrary) {
      hard_memcpy_ptr = (void(*)(void*,const void*,size_t))
	GetProcAddress(DefCRTLibrary, "memcpy");
      hard_memset_ptr = (void(*)(void*,int,size_t))
	GetProcAddress(DefCRTLibrary, "memset");
    }
    
    // patch all relevant Release CRT Library entry points
    bool patchedRls = false;
    if (RlsCRTLibrary) {
      for (int i = 0; i < sizeof(rls_patches) / sizeof(*rls_patches); i++) {
	if (rls_patches[i].original = GetProcAddress(RlsCRTLibrary, rls_patches[i].import)) {
	  PatchIt(&rls_patches[i]);
	  patchedRls = true;
	}
      }
    }

#ifdef _DEBUG
    // patch all relevant Debug CRT Library entry points
    bool patchedDbg = false;
    if (DbgCRTLibrary) {
      for (int i = 0; i < sizeof(dbg_patches) / sizeof(*dbg_patches); i++) {
	if (dbg_patches[i].original = GetProcAddress(DbgCRTLibrary, dbg_patches[i].import)) {
	  PatchIt(&dbg_patches[i]);
	  patchedDbg = true;
	}
      }
    }
#endif
  }
  return true;
}
