// ***************************************************************
//  madCHook - dynamic.h      version:  2.1e  ·  date: 2005-11-28
//  -------------------------------------------------------------
//  API hooking, code hooking
//  -------------------------------------------------------------
//  Copyright (C) 1999 - 2005 www.madshi.net, All Rights Reserved
// ***************************************************************

// 2005-11-28 2.1e Initialize/FinalizeMadCHook definition cleaned up
// 2005-06-13 2.1d (1) automatic unhooking added when used in a single process
//                 (2) new functions (Un)InstallMadCHook added
// 2005-04-05 2.1c (1) flag "DONT_COUNT" renamed to "NO_SAFE_UNHOOKING"
//                 (2) "NO_IMPROVED_SAFE_UNHOOKING" flag added
//                 (3) "SAFE_HOOKING" -> thread safe hook installation (winNT)
// 2005-01-30 2.1b GetCallingModule() returns HMODULE instead of DWORD now
// 2004-04-27 2.1a IsHookInUse added
// 2004-03-07 2.1  (1) seperate header file for dynamic and static lib
//                 (2) NO_MIXTURE_MODE flag added
//                 (3) CreateIpcQueueEx added with additional parameters
// 2003-10-05 2.0a changed initialization/finalization logic to fix memory leak
// 2003-08-10 2.0  (1) HookCode parameters changed -> only one flags parameter
//                 (2) (Un)InjectLibrary: user/session/system wide injection!
//                 (3) InjectLibrary2 replaced by InjectLibrary (auto detect)
//                 (4) static lib for Microsoft C++ added
//                 (5) CreateIpcQueue + SendIpcMessage + DestroyIpcQueue added
//                 (6) AmSystemProcess + AmUsingInputDesktop added
//                 (7) GetCurrentSessionId + GetInputSessionId added
//                 (8) GetCallingModule function added
//                 (9) ProcessIdToFileName added
//                 (a) Create/OpenGlobalMutex + Event + FileMapping added
//                 (b) WideToAnsi + AnsiToWide functions added
//                 (c) RenewHook function added
//                 (d) madCodeHook.dll -> madCHook.dll (8.3 dos name logic)
//                 (e) UnhookAPI added (= UnhookCode, added just for the look)
//                 (f) AddAccessForEveryone added
// 2002-10-03 1.3e (1) InjectLibraryW added
//                 (2) InjectLibrary2(W) added for use in CreateProcess(W) hooks
// 2002-09-22 1.3d CreateProcessExW added
// 2002-03-24 1.3c CollectHooks/FlushHooks speed up mixture initialization
// 2002-02-24 1.3b LPSTARTUPINFO -> LPSTARTUPINFOA
// 2002-01-21 1.3a ProcessHandleToId exported
// 2001-07-08 1.3  new functions (1) AllocMemEx & FreeMemEx
//                               (2) CopyFunction
//                               (3) CreateRemoteThread and
//                               (4) InjectLibrary added
// 2001-04-20 1.2a you can now force HookCode/API to use the mixture mode
// 2001-04-16 1.2  new function CreateProcessEx -> dll injecting

// ***************************************************************

#if !defined(MADCHOOK_H)
#define MADCHOOK_H

#ifdef __cplusplus
  extern "C" {
#endif

#define madCHookApi(rt) __declspec(dllimport) rt WINAPI

// ***************************************************************

// you don't need to install madCodeHook, but you can
// what purpose does this have?
// (1) in the NT family madCodeHook uses a little kernel mode injection driver
//     to inject your hook dlls into all newly created processes
//     if you install madCodeHook, this driver is officially added to the OS
//     list of installed drivers with the description "madCodeHook DLL
//     injection driver"
//     if you don't install madCodeHook, the driver is temporarily extracted
//     to harddisk, which might look "suspicious"
// (2) some protection software (e.g. DeepFreeze) doesn't allow dynamic loading
//     of drivers, so we have to officially and permanently install our driver
//     to make madCodeHook work together with such protection software
// each (successful) call to "InstallMadCHook" needs its own call to
// "UninstallMadCHook", since the installation number is reference counted
madCHookApi(BOOL)   InstallMadCHook ();
madCHookApi(BOOL) UninstallMadCHook ();

// ***************************************************************

// by default madCodeHook counts how many times any thread is currently
// running inside of your callback function
// this way unhooking can be safely synchronized to that counter
// sometimes you don't need/want this counting to happen, e.g.
// (1) if you don't plan to ever unhook, anyway
// (2) if the counting performance drop is too high for your taste
// (3) if you want to unhook from inside the hook callback function
// in those cases you can set the flag "NO_SAFE_UNHOOKING"
#define DONT_COUNT (DWORD) 0x00000001         // old name - kept for compatability
#define NO_SAFE_UNHOOKING (DWORD) 0x00000001  // new name

// with 2.1f the "safe unhooking" functionality (see above) was improved
// most probably there's no problem with the improvement
// but to be sure you can disable the improvement
// the improved safe unhooking is currently only available in the NT family
#define NO_IMPROVED_SAFE_UNHOOKING (DWORD) 0x00000040

// optionally madCodeHook can use a special technique to make sure that
// hooking in multi threaded situations won't result in crashing threads
// this technique is not tested too well right now, so it's optional for now
// you can turn this feature on by setting the flag "SAFE_HOOKING"
// without this technique crashes can happen, if a thread is calling the API
// which we want to hook in exactly the moment when the hook is installed
// safe hooking is currently only available in the NT family
#define SAFE_HOOKING (DWORD) 0x00000020

// madCodeHook implements two different API hooking methods
// the mixture mode is the second best method, it's only used if the main
// hooking method doesn't work for whatever reason (e.g. API code structure)
// normally madCodeHook chooses automatically which mode to use
// you can force madCodeHook to use the mixture mode by specifying the flag:
#define MIXTURE_MODE (DWORD) 0x00000002

// if you don't want madCodeHook to use the mixture mode, you can say so
// however, if the main hooking mode can't be used, hooking then simply fails
#define NO_MIXTURE_MODE (DWORD) 0x00000010

// under win9x you can hook code system wide, if it begins > $80000000
// or if the code section of the to-be-hooked dll is shared
// the callback function is in this case automatically copied to shared memory
// use only kernel32 APIs in such a system wide hook callback function (!!)
// if you want an easier way and/or a NT family compatible way to hook code
// system wide, please use InjectLibrary(ALL_SESSIONS) instead of these flags:
#define SYSTEM_WIDE_9X            (DWORD) 0x00000004
#define ACCEPT_UNKNOWN_TARGETS_9X (DWORD) 0x00000008

// hook any code or a specific API
madCHookApi(BOOL) HookCode(
  PVOID  pCode,
  PVOID  pCallbackFunc,
  PVOID  *pNextHook,
  #ifdef __cplusplus
    DWORD  dwFlags = 0
  #else
    DWORD  dwFlags
  #endif
);
madCHookApi(BOOL) HookAPI(
  LPCSTR pszModule,
  LPCSTR pszFuncName,
  PVOID  pCallbackFunc,
  PVOID  *pNextHook,
  #ifdef __cplusplus
    DWORD  dwFlags = 0
  #else
    DWORD  dwFlags
  #endif
);

// some firewall/antivirus programs kill our hooks, so we need to renew them
madCHookApi(BOOL) RenewHook(
  PVOID  *pNextHook
);

// is the hook callback function of the specified hook currently in use?
// 0: the hook callback function is not in use
// x: the hook callback function is in use x times
madCHookApi(DWORD) IsHookInUse(
  PVOID  *pNextHook
);

// unhook again
madCHookApi(BOOL) UnhookCode( PVOID  *pNextHook );
madCHookApi(BOOL) UnhookAPI ( PVOID  *pNextHook );

// putting all your "HookCode/API" calls into a "CollectHooks".."FlushHooks"
// frame can eventually speed up the installation of the hooks
madCHookApi(VOID) CollectHooks ();
madCHookApi(VOID)   FlushHooks ();

// ***************************************************************
// same as CreateProcess
// additionally the dll "loadLibrary" is injected into the newly created process
// the dll is loaded right before the entry point of the exe module is called

madCHookApi(BOOL) CreateProcessExA(
  LPCSTR                lpApplicationName,
  LPSTR                 lpCommandLine,
  LPSECURITY_ATTRIBUTES lpProcessAttributes,
  LPSECURITY_ATTRIBUTES lpThreadAttributes,
  BOOL                  bInheritHandles,
  DWORD                 dwCreationFlags,
  LPVOID                lpEnvironment,
  LPCSTR                lpCurrentDirectory,
  LPSTARTUPINFOA        lpStartupInfo,
  LPPROCESS_INFORMATION lpProcessInformation,
  LPCSTR                lpLoadLibrary
);
madCHookApi(BOOL) CreateProcessExW(
  LPCWSTR               lpApplicationName,
  LPWSTR                lpCommandLine,
  LPSECURITY_ATTRIBUTES lpProcessAttributes,
  LPSECURITY_ATTRIBUTES lpThreadAttributes,
  BOOL                  bInheritHandles,
  DWORD                 dwCreationFlags,
  LPVOID                lpEnvironment,
  LPCWSTR               lpCurrentDirectory,
  LPSTARTUPINFOW        lpStartupInfo,
  LPPROCESS_INFORMATION lpProcessInformation,
  LPCWSTR               lpLoadLibrary
);

// ***************************************************************
// memory allocation in the specified processes (shared memory in win9x)
// if the processHandle is 0, the memory is allocated or freed in the shared
// area (in win9x) or in our own process (in winNT)

madCHookApi(PVOID) AllocMemEx(
  DWORD  dwSize,
  #ifdef __cplusplus
    HANDLE hProcess = 0
  #else
    HANDLE hProcess
  #endif
);
madCHookApi(BOOL) FreeMemEx(
  PVOID  pMem,
  #ifdef __cplusplus
    HANDLE hProcess = 0
  #else
    HANDLE hProcess
  #endif
);

// ***************************************************************
// copy (and relocate) any function to a new location in any process
// if the processHandle is 0, the function is copied to shared area (in win9x)
// or to another memory location in our own process (in winNT)
// don't forget to free the function with FreeMemEx, if you don't it anymore

madCHookApi(PVOID) CopyFunction(
  PVOID  pFunction,
  #ifdef __cplusplus
    HANDLE hProcess              = 0,
    BOOL   bAcceptUnknownTargets = FALSE,
    PVOID  *pBuffer              = NULL
  #else
    HANDLE hProcess,
    BOOL   bAcceptUnknownTargets,
    PVOID  *pBuffer
  #endif
);

// ***************************************************************
// like CreateRemoteThread, but 3 changes:
// (1) this one also works perfectly in win9x!!
// (2) this one also works on other sessions in winNt
// (3) the DACL of the current thread is copied in winNt (if threadAttr = nil)

#if 0 // EDB - not used by DieHard, causes errors in VS10
madCHookApi(HANDLE) CreateRemoteThreadEx(
  HANDLE                 hProcess,
  LPSECURITY_ATTRIBUTES  lpThreadAttributes,
  DWORD                  dwStackSize,
  LPTHREAD_START_ROUTINE lpStartAddress,
  LPVOID                 lpParameter,
  DWORD                  dwCreationFlags,
  LPDWORD                lpThreadId
);
#endif

// ***************************************************************

// this is how your remote function must look like
typedef DWORD (WINAPI *PREMOTE_EXECUTE_ROUTINE)( LPVOID pParams );

// executes the specified function in the context of another process
// this works only if the function follows some specific rules
// e.g. it must not use global variables, nor C++ private functions
// only win32 APIs are allowed
// if "dwSize" > 0, the "pParams" block will be copied to the other process
// after the remote function is finished, the "pParams" block is copied back
// so you can use the "pParams" block for both "in" and "out" parameters
// if "dwSize" = 0, the "pParams" value is just given into the remote function
madCHookApi(BOOL) RemoteExecute(
  HANDLE                  hProcess,
  PREMOTE_EXECUTE_ROUTINE pFunc,
  DWORD                   *dwFuncResult,
  #ifdef __cplusplus
    PVOID                   pParams = NULL,
    DWORD                   dwSize  = 0
  #else
    PVOID                   pParams,
    DWORD                   dwSize        
  #endif
);

// ***************************************************************

// (un)inject the specified dll into (from) all current and future processes
// these flags can be used for both UninjectLibrary + InjectLibrary
#define ALL_SESSIONS    0xFFFFFFED  // apps of all sessions
#define CURRENT_SESSION 0xFFFFFFEC  // apps of current session
#define CURRENT_USER    0xFFFFFFEB  // apps of current user

// the following flags may only be used in combination with the first 3 flags
#define SYSTEM_PROCESSES 0x10  // include this flag to include system processes + services
#define CURRENT_PROCESS  0x08  // exclude this flag to exclude injection into yourself

// same as LoadLibrary, but is able to load the library in any process
madCHookApi(BOOL) InjectLibraryA(
  DWORD   dwProcessHandleOrSpecialFlags,
  LPCSTR  pLibFileName,
  #ifdef __cplusplus
    DWORD   dwTimeOut = 7000
  #else
    DWORD   dwTimeOut
  #endif
);
madCHookApi(BOOL) InjectLibraryW(
  DWORD   dwProcessHandleOrSpecialFlags,
  LPCWSTR pLibFileName,
  #ifdef __cplusplus
    DWORD   dwTimeOut = 7000
  #else
    DWORD   dwTimeOut
  #endif
);

// same as InjectLibrary(CURRENT_SESSION, ...), but you can choose the session
madCHookApi(BOOL) InjectLibrarySessionA(
  DWORD   dwSession,
  BOOL    bSystemProcesses,
  LPCSTR  pLibFileName,
  #ifdef __cplusplus
    DWORD   dwTimeOut = 7000
  #else
    DWORD   dwTimeOut
  #endif
);
madCHookApi(BOOL) InjectLibrarySessionW(
  DWORD   dwSession,
  BOOL    bSystemProcesses,
  LPCWSTR pLibFileName,
  #ifdef __cplusplus
    DWORD   dwTimeOut = 7000
  #else
    DWORD   dwTimeOut
  #endif
);

// stop the "virus" injection effect (can be used for UninjectLibrary only)
#define STOP_VIRUS (HANDLE) 0xFFFFFFFA

// same as FreeLibrary, but is able to free the library from any process
madCHookApi(BOOL) UninjectLibraryA(
  DWORD   dwProcessHandleOrSpecialFlags,
  LPCSTR  pLibFileName,
  #ifdef __cplusplus
    DWORD   dwTimeOut = 7000
  #else
    DWORD   dwTimeOut
  #endif
);
madCHookApi(BOOL) UninjectLibraryW(
  DWORD   dwProcessHandleOrSpecialFlags,
  LPCWSTR pLibFileName,
  #ifdef __cplusplus
    DWORD   dwTimeOut = 7000
  #else
    DWORD   dwTimeOut
  #endif
);

// same as UninjectLibrary(CURRENT_SESSION, ...), but you can choose the session
madCHookApi(BOOL) UninjectLibrarySessionA(
  DWORD   dwSession,
  BOOL    bSystemProcesses,
  LPCSTR  pLibFileName,
  #ifdef __cplusplus
    DWORD   dwTimeOut = 7000
  #else
    DWORD   dwTimeOut
  #endif
);
madCHookApi(BOOL) UninjectLibrarySessionW(
  DWORD   dwSession,
  BOOL    bSystemProcesses,
  LPCWSTR pLibFileName,
  #ifdef __cplusplus
    DWORD   dwTimeOut = 7000
  #else
    DWORD   dwTimeOut
  #endif
);

// ***************************************************************

// which processId belongs to the specified process handle?
// undocumented function, works in all windows 32 bit systems
madCHookApi(DWORD) ProcessHandleToId(
  HANDLE dwProcessHandle
);

// find out what file the specified process was executed from
// the file name buffer must have a size of MAX_PATH characters (or more)
madCHookApi(BOOL) ProcessIdToFileName(
  DWORD  dwProcessId,
  LPSTR  pFileName
);

// ***************************************************************

// is the current process a service/system process?  (win9x -> always false)
madCHookApi(BOOL) AmSystemProcess (VOID);

// is the current thread's desktop the input desktop?  (win9x -> always true)
// only in that case you should show messages boxes or other GUI stuff
// but please note that in XP fast user switching AmUsingInputDesktop may
// return true, although the current session is currently not visible
// XP fast user switching is implemented by using terminal server logic
// so each fast user session has its own window station and input desktop
madCHookApi(BOOL) AmUsingInputDesktop (VOID);

// the following two functions can be used to get the session id of the
// current session and of the input session
// each terminal server (or XP fast user switching) session has its own id
// the "input session" is the one currently shown on the physical screen
madCHookApi(DWORD) GetCurrentSessionId (VOID);
madCHookApi(DWORD) GetInputSessionId   (VOID);

// ***************************************************************

// which module called me? works only if your function has a stack frame
// MSVC++ users can turn forced stackframe on/off by using:
// #pragma optimize( "y", off/on )
madCHookApi(HMODULE) GetCallingModule (VOID);

// ***************************************************************
// global  =  normal  +  "access for everyone"  +  "non session specific"

madCHookApi(HANDLE) CreateGlobalMutex(
  LPCSTR  pName
);
madCHookApi(HANDLE) OpenGlobalMutex(
  LPCSTR  pName
);
madCHookApi(HANDLE) CreateGlobalEvent(
  LPCSTR  pName,
  BOOL    bManual,
  BOOL    bInitialState
);
madCHookApi(HANDLE) OpenGlobalEvent(
  LPCSTR  pName
);
madCHookApi(HANDLE) CreateGlobalFileMapping(
  LPCSTR  pName,
  DWORD   dwSize
);
madCHookApi(HANDLE) OpenGlobalFileMapping(
  LPCSTR  pName,
  BOOL    bWrite
);

// ***************************************************************

// convert strings ansi <-> wide
// the result buffer must have a size of MAX_PATH characters (or more)
// please use these functions in nt wide API hook callback functions
// because the OS' own functions seem to confuse nt in hook callback functions
madCHookApi(VOID) AnsiToWide(
  LPCSTR  pAnsi,
  LPWSTR  pWide
);
madCHookApi(VOID) WideToAnsi(
  LPCWSTR pWide,
  LPSTR   pAnsi
);

// ***************************************************************
// ipc (inter process communication) message services
// in contrast to SendMessage the following functions don't crash NT services

// this is how you get notified about incoming ipc messages
// you have to write a function which fits to this type definition
// and then you give it into "CreateIpcQueue"
// your callback function will then be called for each incoming message
// CAUTION: each ipc message is handled by a seperate thread, as a result
//          your callback will be called by a different thread each time
typedef VOID (WINAPI *PIPC_CALLBACK_ROUTINE)(
  LPCSTR  pIpc,
  PVOID   pMessageBuf,
  DWORD   dwMessageLen,
  PVOID   pAnswerBuf,
  DWORD   dwAnswerLen
);

// create an ipc queue
// please choose a unique ipc name to avoid conflicts with other programs
// only one ipc queue with the same name can be open at the same time
// so if 2 programs try to create the same ipc queue, the second call will fail
// you can specify how many threads may be created to handle incoming messages
// if the order of the messages is crucial for you, set "maxThreadCount" to "1"
// in its current implementation "maxThreadCount" only supports "1" or unlimited
// the parameter "maxQueueLen" is not yet implemented at all
madCHookApi(BOOL) CreateIpcQueueEx(
  LPCSTR                pIpc,
  PIPC_CALLBACK_ROUTINE pCallback,
  #ifdef __cplusplus
    DWORD                 dwMaxThreadCount = 16,
    DWORD                 dwMaxQueueLen    = 0x1000
  #else
    DWORD                 dwMaxThreadCount,
    DWORD                 dwMaxQueueLen
  #endif
);
madCHookApi(BOOL) CreateIpcQueue(
  LPCSTR                pIpc,
  PIPC_CALLBACK_ROUTINE pCallback
);

// send an ipc message to whomever has created the ipc queue (doesn't matter)
// if you only fill the first 3 parameters, SendIpcMessage returns at once
// if you fill the next two parameters, too, SendIpcMessage will
// wait for an answer of the ipc queue owner
// you can further specify how long you're willing to wait for the answer
// and whether you want SendIpcMessage to handle messages while waiting
madCHookApi(BOOL) SendIpcMessage(
  LPCSTR  pIpc,
  PVOID   pMessageBuf,
  DWORD   dwMessageLen,
  #ifdef __cplusplus
    PVOID   pAnswerBuf      = NULL,
    DWORD   dwAnswerLen     = 0,
    DWORD   dwAnswerTimeOut = INFINITE,
    BOOL    bHandleMessage  = TRUE
  #else
    PVOID   pAnswerBuf,
    DWORD   dwAnswerLen,
    DWORD   dwAnswerTimeOut,
    BOOL    bHandleMessage
  #endif
);

// destroy the ipc queue again
// when the queue owning process quits, the ipc queue is automatically deleted
// only the queue owning process can destroy the queue
madCHookApi(BOOL) DestroyIpcQueue(
  LPCSTR  pIpc
);

// ***************************************************************
// this function adds some access rights to the specified target
// the target can either be a process handle or a service handle

madCHookApi(BOOL) AddAccessForEveryone(
  HANDLE  hProcessOrService,
  DWORD   dwAccess
);

// ***************************************************************
// please in your initialization call "InitializeMadCHook"
// and in your finalization please call "FinalizeMadCHook"

#define InitializeMadCHook() ;
#define FinalizeMadCHook() AutoUnhook((HINSTANCE) -1);

// ***************************************************************

#ifdef UNICODE
  #define CreateProcessEx CreateProcessExW
  #define   InjectLibrary   InjectLibraryW
  #define UninjectLibrary UninjectLibraryW
  #define   InjectLibrarySession   InjectLibrarySessionW
  #define UninjectLibrarySession UninjectLibrarySessionW
#else
  #define CreateProcessEx CreateProcessExA
  #define   InjectLibrary   InjectLibraryA
  #define UninjectLibrary UninjectLibraryA
  #define   InjectLibrarySession   InjectLibrarySessionA
  #define UninjectLibrarySession UninjectLibrarySessionA
#endif

// ***************************************************************
// internal stuff, please ignore

madCHookApi(VOID) AutoUnhook (HINSTANCE hinstDLL);

#ifdef __cplusplus
}
#endif

#endif
