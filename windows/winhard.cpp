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

const char *RlsCRTLibraryName[] = {"MSVCR71.DLL", "MSVCR80.DLL", "MSVCR90.DLL", "MSVCR100.DLL", "MSVCP100.DLL", "MSVCRT.DLL" };
static const int RlsCRTLibraryNameLength = 6;

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
// #define DIEHARD_PRE_ACTION { DisableThreadLibraryCalls ((HMODULE)hinstDLL); PatchMeIn();}
#define DIEHARD_PRE_ACTION { PatchMeIn();}
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

    {"_expand",		(FARPROC) hard_expand,	   0},
    {"_onexit",         (FARPROC) hard_onexit,    0},
    {"_exit",           (FARPROC) hard_exit,      0},

    // operator new, new[], delete, delete[].

#ifdef _WIN64

    {"??2@YAPEAX_K@Z",  (FARPROC) hoard_malloc,    0},
    {"??_U@YAPEAX_K@Z", (FARPROC) hoard_malloc,    0},
    {"??3@YAXPEAX@Z",   (FARPROC) hoard_free,      0},
    {"??_V@YAXPEAX@Z",  (FARPROC) hoard_free,      0},

#else

    {"??2@YAPAXI@Z",    (FARPROC) hard_malloc,    0},
    {"??_U@YAPAXI@Z",   (FARPROC) hard_malloc,    0},
    {"??3@YAXPAX@Z",    (FARPROC) hard_free,      0},
    {"??_V@YAXPAX@Z",   (FARPROC) hard_free,      0},

#endif

    // the nothrow variants new, new[].

    {"??2@YAPAXIABUnothrow_t@std@@@Z",  (FARPROC) hard_malloc, 0},
    {"??_U@YAPAXIABUnothrow_t@std@@@Z", (FARPROC) hard_malloc, 0},

    {"_msize",	(FARPROC) hard_malloc_usable_size,		0},
    {"calloc",	(FARPROC) hard_calloc,		0},
    {"malloc",	(FARPROC) hard_malloc,		0},
    {"realloc",	(FARPROC) hard_realloc,		0},
    {"free",	(FARPROC) hard_free,            0}
    {"_recalloc", (FARPROC) hard_recalloc,     0},
    {"_putenv",	(FARPROC) hard__putenv,	0,
    {"getenv",	(FARPROC) hard_getenv,		0},
  };


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
    HMODULE DefCRTLibrary = 
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
  }
  return patchedRls;
}


extern "C" void reportOverflowError (void)
{
  static bool alertedAlready = false;
  if (!alertedAlready) {
    alertedAlready = true;
    MessageBox (0, "This application has performed an illegal action (an overflow).", "DieHard", MB_ICONINFORMATION);
  }
}

extern "C" void reportDoubleFreeError (void)
{
  static bool alertedAlready = false;
  if (!alertedAlready) {
    alertedAlready = true;
    MessageBox (0, "This application has performed an illegal action (a double free), but DieHard has prevented it from doing any harm.", "DieHard", MB_ICONINFORMATION);
  }
}

extern "C" void reportInvalidFreeError (void) {
  static bool alertedAlready = false;
  if (!alertedAlready) {
    alertedAlready = true;
    MessageBox (0, "DieHard", "This application has performed an illegal action (an invalid free), but DieHard has prevented it from doing any damage.", MB_ICONINFORMATION);
  }
}
