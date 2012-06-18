/**
 * @file  diehard-system.cpp
 * @brief  The implementation of the DieHard system for Windows.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 */

#if !defined(_WIN32)
#error "This file is for Windows only."
#endif

#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0500

#pragma init_seg(compiler)
#pragma inline_depth(255)

#pragma optimize("sy",on)

#pragma comment(linker, "/disallowlib:libc.lib")
#pragma comment(linker, "/disallowlib:libcd.lib")
#pragma comment(linker, "/disallowlib:libcmtd.lib")
#pragma comment(linker, "/disallowlib:msvcrtd.lib")
#pragma comment(linker, "/disallowlib:msvcrt.lib")

#pragma warning(disable: 4820) // Padding
#pragma warning(disable: 4127) // Constant conditional
#pragma warning(disable: 4625) // Inaccessible copy constructors
#pragma warning(disable: 4626) // Inaccessible assignment op
#pragma warning(disable: 4512) 
#pragma warning(disable: 4996) // sprintf warning
#if 0
#pragma warning(disable: 4101) // Unreferenced local variable
#pragma warning(disable: 4800) // Performance warning (size_t to bool)
#pragma warning(disable: 4668) // preprocessor warning
#endif

#define NO_WIN32_DLL
#define DIEHARD_REPLICATED 0
#ifndef DIEHARD_DIEFAST
#define DIEHARD_DIEFAST 0
#endif

#define PRINT_STATS 0


#include "diehard.h"

#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <malloc.h>
#include <limits.h>
#include <stdarg.h>
#include <ctype.h>

static const char * whiteList[] = 
  {
    "mozilla.exe",
    "firefox.exe",
  };

#define CUSTOM_PREFIX(n) diehard##n

#include "libdiehard.cpp"
#include "madCHook.h"

static const char * dllName[] = 
  { "msvcr80.dll",
    "msvcrt.dll",
    "msvcr70.dll",
    "msvcr71.dll"
  };

enum { NUMBER_OF_LIBRARIES = sizeof(dllName) / sizeof(const char *) };

const int MAX_EXIT_FUNCTIONS = 1024;
static _onexit_t maxFunctions[MAX_EXIT_FUNCTIONS];
static int functionCount = 0;

extern "C" _onexit_t __cdecl local_onexit (_onexit_t func) {
  if (func == NULL) {
    return NULL;
  }
  if (functionCount < MAX_EXIT_FUNCTIONS) {
    maxFunctions[functionCount] = func;
    functionCount++;
  }
  return func;
}

extern "C" int __cdecl local_atexit (void (__cdecl *func )( void )) {
  if (local_onexit ((_onexit_t) func)) {
    return 0;
  } else {
    return NULL;
  }
}


extern "C" void __cdecl local_exit (int) {
#if 0
  while (exitCount > 0) {
    exitCount--;
    (exitFunctionBuffer[exitCount])();
  }
#endif
}

extern "C" void __cdecl local_cexit (void) {
#if 0
  // Disabled -- this causes problems...
  while (functionCount > 0) {
    (maxFunctions[functionCount-1])();
    functionCount--;
  }
#endif
}

extern "C" void __cdecl local_dllonexit (void (*)(),
					 void (***)(), 
					 void (***)()) 
{
  // Disabled for now.
}

#if PRINT_STATS
int mallocs = 0;
float mallocSize = 0.0;
#endif

extern "C" void * __cdecl local_malloc (size_t sz) {
#if PRINT_STATS
  mallocs++;
  mallocSize += sz;
#endif
  return CUSTOM_PREFIX(malloc)(sz);
}

extern "C" void * __cdecl local_nh_malloc (size_t sz,
					   int) {
  return local_malloc(sz);
}

extern "C" void * __cdecl local_calloc (size_t sz,
					size_t n) {
  void * ptr = local_malloc(sz * n + 1);
  if (ptr)
    memset (ptr, 0, sz * n + 1);
  return ptr;
}

extern "C" void * __cdecl local_calloc_impl (size_t sz,
					     size_t n,
					     int * /* et */) {
  return local_calloc(sz, n);
}

extern "C" size_t __cdecl local_usable_size (void * ptr) {
  size_t objSize = CUSTOM_PREFIX(malloc_usable_size)(ptr);
  return objSize;
}

extern "C" void __cdecl local_free (void * ptr) {
  CUSTOM_PREFIX(free)(ptr);
}

extern "C" void * __cdecl local_expand (void * memblock,
					size_t size)
{
  // If we have enough room, just return the object.
  if (local_usable_size(memblock) >= size) {
    return memblock;
  }
  return NULL;
}

extern "C" void * __cdecl local_realloc (void * ptr,
					 size_t sz) {
  if (ptr == NULL) {
    return local_malloc (sz);
  }
  if (sz == 0) {
    local_free (ptr);
    return NULL;
  }

  size_t objSize = local_usable_size (ptr);
  void * buf = local_malloc (sz);

  if (buf != NULL) {
    // Copy the contents of the original object
    // up to the size of the new block.
    size_t minSize = (objSize < sz) ? objSize : sz;
    memcpy (buf, ptr, minSize);
  }

  // Free the old block.
  local_free (ptr);

  // Return a pointer to the new one.
  return buf;
}

extern "C" void * __cdecl local_recalloc (void * memblock,
					  size_t num,
					  size_t size)
{
  void * ptr = local_realloc (memblock, num * size);
  if (ptr)
    memset (ptr, 0, num * size);
  return ptr;
}

extern "C" void * __cdecl local_heap_alloc (size_t sz) {
  return local_malloc (sz);
}

extern "C" void local_aligned_free (void * ptr) {
  local_free (ptr);
}

extern "C" void * local_aligned_malloc (size_t sz,
					size_t align) {
  return CUSTOM_PREFIX(memalign)(align, sz);
}

// Disabled for now.

extern "C" void * local_aligned_realloc (void * ptr, size_t sz, size_t align) {
  abort();
  return 0;
}

extern "C" void * local_aligned_offset_malloc (size_t sz, size_t align, int offset) {
  abort();
  return 0;
}

extern "C" void * local_aligned_offset_realloc (void * ptr, size_t sz, size_t align) {
  abort();
  return 0;
}



/* ---------- HeapShield ------------ */

#if 0
#if 1
#include "heapshield.cpp"
#else

extern "C" void * local_strdup (char * s) {
  int len = strlen(s) + 1;
  char * p = (char *) local_malloc (len);
  if (p) {
    strncpy (p, s, len);
  }
  return p;
}

extern "C" void * _local_strdup (char * s) {
  return local_strdup (s);
}

#endif
#endif

/* --------------------------------------------------------------------------- */


#pragma warning(disable:4273)

void AttachMallocHooks (void)
{
  const DWORD HookFlags = 0;
  PVOID funcptr[(NUMBER_OF_LIBRARIES + 1) * 64];
  // increase if more than this many HookAPI calls.

  CollectHooks();

  int f = 0;
  for (int i = 0; i < NUMBER_OF_LIBRARIES; i++) {

#if 1
    // Exit and friends
    HookAPI(dllName[i], "_onexit", local_onexit, &funcptr[f++], HookFlags);
    HookAPI(dllName[i], "atexit", local_atexit, &funcptr[f++], HookFlags);
    HookAPI(dllName[i], "_cexit", local_cexit, &funcptr[f++], HookFlags);
    HookAPI(dllName[i], "_c_exit", local_cexit, &funcptr[f++], HookFlags);
    HookAPI(dllName[i], "_exit", local_exit, &funcptr[f++], HookFlags);
    HookAPI(dllName[i], "__dllonexit", local_dllonexit, &funcptr[f++], HookFlags);
#endif

#if 1
    // Malloc
    HookAPI(dllName[i], "??2@YAPAXI@Z",  local_malloc, &funcptr[f++], HookFlags);
    HookAPI(dllName[i], "??_U@YAPAXI@Z", local_malloc, &funcptr[f++], HookFlags);
    HookAPI(dllName[i], "_malloc_crt", local_malloc, &funcptr[f++], HookFlags);
    HookAPI(dllName[i], "_malloc_base", local_malloc, &funcptr[f++], HookFlags);
    HookAPI(dllName[i], "malloc", local_malloc, &funcptr[f++], HookFlags);
    HookAPI(dllName[i], "__sbh_alloc_block", local_malloc, &funcptr[f++],  HookFlags);
    HookAPI(dllName[i], "_nh_malloc", local_nh_malloc, &funcptr[f++],  HookFlags);

    // Calloc
    HookAPI(dllName[i], "calloc", local_calloc,  &funcptr[f++],  HookFlags);
    HookAPI(dllName[i], "_calloc_crt", local_calloc,  &funcptr[f++],  HookFlags);
    HookAPI(dllName[i], "_calloc_base", local_calloc,  &funcptr[f++],  HookFlags);
    HookAPI(dllName[i], "_calloc_impl", local_calloc_impl,  &funcptr[f++],  HookFlags);

    // Realloc
    HookAPI(dllName[i], "realloc", local_realloc, &funcptr[f++],  HookFlags);
    HookAPI(dllName[i], "_realloc_crt", local_realloc, &funcptr[f++],  HookFlags);
    HookAPI(dllName[i], "_realloc_base", local_realloc, &funcptr[f++],  HookFlags);

    // Free
    HookAPI(dllName[i], "free", local_free, &funcptr[f++], HookFlags);
    HookAPI(dllName[i], "??3@YAXPAX@Z",  local_free, &funcptr[f++], HookFlags);
    HookAPI(dllName[i], "??_V@YAXPAX@Z", local_free, &funcptr[f++], HookFlags);
    HookAPI(dllName[i], "_free_crt", local_free, &funcptr[f++], HookFlags);

    // Size
    HookAPI(dllName[i], "_msize", local_usable_size, &funcptr[f++], HookFlags);

#if 0
    // Aligned functions
    HookAPI(dllName[i], "_aligned_free", local_free, &funcptr[f++], HookFlags);
    HookAPI(dllName[i], "_aligned_malloc", local_aligned_malloc, &funcptr[f++], HookFlags);
    HookAPI(dllName[i], "_aligned_realloc", local_aligned_realloc, &funcptr[f++], HookFlags);
    HookAPI(dllName[i], "_aligned_offset_malloc", local_aligned_offset_malloc, &funcptr[f++], HookFlags);
    HookAPI(dllName[i], "_aligned_offset_realloc", local_aligned_offset_realloc, &funcptr[f++], HookFlags);
#endif

    // Others
    HookAPI(dllName[i], "_heap_alloc", local_malloc, &funcptr[f++], HookFlags);
    HookAPI(dllName[i], "_expand", local_expand, &funcptr[f++], HookFlags);

    HookAPI(dllName[i], "_recalloc", local_recalloc, &funcptr[f++], HookFlags);
    HookAPI(dllName[i], "_recalloc_crt", local_recalloc, &funcptr[f++], HookFlags);

#if 0
    // String functions
    HookAPI(dllName[i], "strdup", local_strdup, &funcptr[f++], HookFlags);
    HookAPI(dllName[i], "_strdup", local_strdup, &funcptr[f++], HookFlags);
    HookAPI(dllName[i], "strcpy", local_strcpy, &funcptr[f++], HookFlags);
    HookAPI(dllName[i], "strncpy", local_strncpy, &funcptr[f++], HookFlags);
    HookAPI(dllName[i], "strlen", local_strlen, &funcptr[f++], HookFlags);
#endif

#endif

  }

#if 0
  // This code does not work.

  HookAPI ("kernel32.dll", "HeapCreate", local_HeapCreate, &funcptr[f], HookFlags);
  real_HeapCreate = (HANDLE (WINAPI *) (DWORD, SIZE_T, SIZE_T)) funcptr[f];
  f++;
  HookAPI ("kernel32.dll", "GetProcessHeap", local_GetProcessHeap, &funcptr[f], HookFlags);
  real_GetProcessHeap = (HANDLE (WINAPI *)()) funcptr[f];
  f++;
  HookAPI ("kernel32.dll", "HeapDestroy", local_HeapDestroy, &funcptr[f], HookFlags);
  real_HeapDestroy = (BOOL (WINAPI *)(HANDLE)) funcptr[f];
  f++;
  HookAPI ("kernel32.dll", "HeapAlloc", local_HeapAlloc, &funcptr[f], HookFlags);
  real_HeapAlloc = (LPVOID (WINAPI *) (HANDLE, DWORD, SIZE_T)) funcptr[f];
  f++;
  HookAPI ("kernel32.dll", "HeapFree", local_HeapFree, &funcptr[f], HookFlags);
  real_HeapFree = (BOOL (WINAPI *) (HANDLE, DWORD, LPVOID)) funcptr[f];
  f++;
#if 1
  HookAPI ("kernel32.dll", "HeapSize", local_HeapSize, &funcptr[f], HookFlags);
  real_HeapSize = (SIZE_T (WINAPI *) (HANDLE, ULONG, LPCVOID)) funcptr[f];
  f++;
  HookAPI ("kernel32.dll", "HeapReAlloc", local_HeapReAlloc, &funcptr[f], HookFlags);
  real_HeapReAlloc = (LPVOID (WINAPI *) (HANDLE, DWORD, LPVOID, SIZE_T)) funcptr[f];
  f++;
  HookAPI ("kernel32.dll", "HeapValidate", local_HeapValidate, &funcptr[f++], HookFlags);
  HookAPI ("kernel32.dll", "HeapWalk", local_HeapWalk, &funcptr[f++], HookFlags);
  HookAPI ("kernel32.dll", "HeapCompact", local_HeapCompact, &funcptr[f++], HookFlags);
  HookAPI ("kernel32.dll", "HeapLock", local_HeapLock, &funcptr[f++], HookFlags);
  HookAPI ("kernel32.dll", "HeapUnlock", local_HeapUnlock, &funcptr[f++], HookFlags);
  HookAPI ("kernel32.dll", "HeapSetInformation", local_HeapSetInformation, &funcptr[f++], HookFlags);
  HookAPI ("kernel32.dll", "HeapQueryInformation", local_HeapSetInformation, &funcptr[f++], HookFlags);
#endif
#endif


  FlushHooks();

}


/////////
/////////

const int whiteListEntries = sizeof(whiteList) / sizeof(const char *);

bool onWhiteList (char * cmd) {
  int len = (int) strlen (cmd);
  for (int i = 0; i < len; i++) {
    cmd[i] = (char) tolower (cmd[i]);
  }
  for (int i = 0; i < whiteListEntries; i++) {
    if (strstr (cmd, whiteList[i]) != NULL) {
      return true;
    }
  }
  return false;
}

WNDPROC winProc = 0;
HHOOK windowsHook = 0;

void updateText (HWND hWnd)
{
  // Append "[DieHard]" to the title bar.
  char buf[255];
  if (GetWindowText (hWnd, buf, 255)) {
    if (!strstr(buf, "[DieHard]")) {
      char buf2[300];
      sprintf (buf2, "%s [DieHard]", buf);
      SetWindowText (hWnd, buf2);
    }
  }
}


extern "C" LRESULT CALLBACK HookMessage (HWND hWnd,
					 UINT uMsg,
					 WPARAM wParam,
					 LPARAM lParam)
{
  LRESULT rc = CallWindowProc (winProc, hWnd, uMsg, wParam, lParam);
#if 1
  switch (uMsg) {
  case WM_CREATE:
  case WM_ACTIVATE:
  case WM_NCPAINT:
  case WM_NCACTIVATE:
  case WM_SYSCOMMAND:
  case WM_GETTEXT:
  case WM_SETTEXT:
  case WM_CHILDACTIVATE:
    updateText (hWnd);
    break;
  default:
    break;
  }
#endif
  return rc;
}


extern "C" LRESULT CALLBACK EstablishHook (int code,
					   WPARAM wParam,
					   LPARAM lParam)
{
  // A helper function for setting the title bar text.
  if (code == HC_ACTION) {
    CWPSTRUCT * cwp = (CWPSTRUCT *) lParam;
    if (cwp->message == WM_ACTIVATE) {
      if (!winProc)
	winProc = (WNDPROC)
	  SetWindowLongPtr (cwp->hwnd, GWLP_WNDPROC, (LONG_PTR) HookMessage);
    }
  }
  return CallNextHookEx (windowsHook, code, wParam, lParam);
}


void AttachWindowHooks (void)
{
  // Establish the hook to update title bars.
  windowsHook = SetWindowsHookEx (WH_CALLWNDPROC,
				  &EstablishHook,
				  0,
				  GetCurrentThreadId());
}

static bool wasPatched = false;

static void diehardMessage (const char * errString);


BOOL APIENTRY DllMain (HINSTANCE /* hModule */,
		       DWORD dwReason,
		       PVOID)
{
  if (dwReason == DLL_PROCESS_ATTACH) {

    InitializeMadCHook();

    // Check to see if the file has been created by DieHard -- if
    // not, this is an already-running process, and we leave it
    // alone. Otherwise, we check the whitelist for this process, and
    // if it's on the whitelist, we hook it.

    HANDLE f = CreateFile (TEXT(DIEHARD_FILENAME),
			   GENERIC_READ,
			   FILE_SHARE_READ,
			   NULL,
			   OPEN_EXISTING,
			   FILE_ATTRIBUTE_HIDDEN,
			   NULL);


    if (f != INVALID_HANDLE_VALUE) {

      // Now we check to see that the file was created before the
      // current executable started. If not, we do not patch it.

      WIN32_FILE_ATTRIBUTE_DATA fileInfo;
      // Find out when the file was created.
      GetFileAttributesEx(TEXT(DIEHARD_FILENAME),
			  GetFileExInfoStandard,
			  (void *) &fileInfo);

      // Now find out when this process was created.
      FILETIME creationTime, exitTime, kernelTime, userTime;
      GetProcessTimes(GetCurrentProcess(), &creationTime, &exitTime, &kernelTime, &userTime);

      // If the process was created AFTER the file was created, then
      // patch away; otherwise, do nothing.

      LARGE_INTEGER ft, pt;
      ft.LowPart  = fileInfo.ftCreationTime.dwLowDateTime;
      ft.HighPart = fileInfo.ftCreationTime.dwHighDateTime;
      pt.LowPart  = creationTime.dwLowDateTime;
      pt.HighPart = creationTime.dwHighDateTime;
      bool createdAfter = (pt.QuadPart > ft.QuadPart);

      if (createdAfter) {
	// Parse out the command line. If the first argument matches
	// our 'white list', patch it.
	int nArgs;
	LPWSTR * argv = CommandLineToArgvW (GetCommandLineW(), &nArgs);
	if (nArgs > 0) {
	  char cmd[MAX_PATH];
	  memset (cmd, '\0', MAX_PATH);
	  WideCharToMultiByte (CP_ACP, NULL, argv[0], wcslen(argv[0]), cmd, MAX_PATH, NULL, NULL);
	  if (onWhiteList (cmd)) {
	    AttachMallocHooks();
	    //	    MessageBox (0, "This application is being protected by DieHard.", "DieHard", MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
	    //	    AttachWindowHooks();
	    wasPatched = true;
	  }
	}
      }

      CloseHandle (f);
    }

  } else if (dwReason == DLL_THREAD_ATTACH) {
    anyThreadCreated = true;
  } else if (dwReason == DLL_PROCESS_DETACH) {
    if (wasPatched) {
#if PRINT_STATS // this is just for informational purposes
      char buf[255];
      if (mallocs > 0) {
	sprintf (buf, "mallocs = %d, avg size = %d\n", mallocs, (int) mallocSize / mallocs);
      } else {
	sprintf (buf, "no mallocs.\n");
      }
      diehardMessage (buf);
#endif
    }
  }

  FinalizeMadCHook();

  return TRUE;
}


static void diehardMessage (const char * errString)
{
  // Output a message (once only), either to the screen or to the console.
  static bool alertedAlready = false;
  if (!alertedAlready) {
    if(!GetConsoleTitle(NULL, 0) && GetLastError() == ERROR_SUCCESS) {
      // CUI
      fprintf (stderr, errString);
    } else {
      // GUI
      MessageBox (0, errString, "DieHard", MB_ICONINFORMATION);
    }
    alertedAlready = true;
  }
}


extern "C" void reportOverflowError (void)
{
  const char errString[] = "This application has performed an illegal action (an overflow).";
  diehardMessage (errString);
}

extern "C" void reportDoubleFreeError (void)
{
  const char errString[] = "This application has performed an illegal action (a double free),\nbut DieHard has prevented it from doing any damage.";
  diehardMessage (errString);
}

extern "C" void reportInvalidFreeError (void) {
  const char errString[] = "This application has performed an illegal action (an invalid free),\nbut DieHard has prevented it from doing any damage.";
  diehardMessage (errString);
}


