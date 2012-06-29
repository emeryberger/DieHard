/**
 * @file   diehard.cpp
 * @brief  Protects certain Windows apps.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 *
 * Copyright (C) 2006-7 Emery Berger, University of Massachusetts Amherst
 */

#include <windows.h>
#include <stdio.h>

#include "madCHook.h"
#include "diehard.h"

char questionStr[]	= "DieHard Application Hardener, version "VERSION_STRING " (" __DATE__ ", " __TIME__ ")\nCopyright (C) 2006-2007 Emery Berger, University of Massachusetts Amherst\nhttp://www.diehard-software.org\n\nThis software is free for non-commercial use only.\n\nDieHard is currently not activated.\n\nProtect your applications from memory bugs?";

char activeQuestionStr[]	= "DieHard Application Hardener, version "VERSION_STRING " (" __DATE__ ", " __TIME__ ")\nCopyright (C) 2006-2007 Emery Berger, University of Massachusetts Amherst\nhttp://www.diehard-software.org\n\nThis software is free for non-commercial use only.\n\nDieHard is currently ACTIVE.\n\nProtect your applications from memory bugs?";

char installedStr[]	= "DieHard protection is now active.\n";

char uninstalledStr[]	= "DieHard protection has been disabled.\n\nThanks for using DieHard.\nPlease send any feedback to emery@cs.umass.edu.";

const char dllName[] 		= DIEHARD_DLL_NAME;
const char madCHookdllName[]	= MADCHOOK_DLL_NAME;

const DWORD code =
  (DWORD) CURRENT_SESSION & ~SYSTEM_PROCESSES & ~CURRENT_PROCESS;

int WINAPI WinMain (HINSTANCE hInstance,
		    HINSTANCE hPrevInstance,
		    LPSTR lpCmdLine,
		    int nCmdShow)
{
  // First, check to see if DieHard is already active or not.
  HANDLE f = CreateFile (TEXT(DIEHARD_FILENAME),
			 GENERIC_READ,
			 FILE_SHARE_READ,
			 NULL,
			 OPEN_EXISTING,
			 FILE_ATTRIBUTE_HIDDEN,
			 NULL);

  bool alreadyActive = (f != INVALID_HANDLE_VALUE);
  CloseHandle (f);

  // Ask if the user wants to activate DieHard.
  int r;
  if (!alreadyActive) {
    r = MessageBox (0, questionStr, "DieHard", 
		    MB_YESNO | MB_ICONINFORMATION | MB_SYSTEMMODAL |
		    MB_TOPMOST | MB_SETFOREGROUND);
  } else {
    r = MessageBox (0, activeQuestionStr, "DieHard", 
		    MB_YESNO | MB_ICONINFORMATION | MB_SYSTEMMODAL |
		    MB_TOPMOST | MB_SETFOREGROUND);
  }
  
  if (r == IDYES) {
    // Activate DieHard protection.

    // We have to show the message box before we actually install the
    // DieHard DLL, or Explorer crashes (!).
    int r = MessageBox (0, installedStr, "DieHard",
			MB_ICONINFORMATION | MB_SYSTEMMODAL | MB_TOPMOST |
			MB_SETFOREGROUND);

    // InitializeMadCHook is needed only if you're using the static madCHook.lib
    InitializeMadCHook();

    // Turn on DieHard.  First, inject in all running processes, but
    // do nothing.
    InjectLibrary (code, dllName, 30000);

    // Now, create the file.
    HANDLE f = CreateFile (TEXT(DIEHARD_FILENAME),
			   GENERIC_WRITE,
			   0,
			   NULL,
			   CREATE_ALWAYS,
			   FILE_ATTRIBUTE_HIDDEN,
			   NULL);

  } else {

    // Disable DieHard, and alert the user.
    int r = MessageBox (0, uninstalledStr, "DieHard",
			MB_ICONEXCLAMATION | MB_SYSTEMMODAL |
			MB_TOPMOST | MB_SETFOREGROUND);

    
    DeleteFile (TEXT(DIEHARD_FILENAME));

#if defined(DIEHARD_RELEASE)
    // Leave installed in already-patched processes.
    UninjectLibrary ((DWORD) STOP_VIRUS, dllName, 1000);
    UninjectLibrary ((DWORD) STOP_VIRUS, madCHookdllName, 1000);
#else
#pragma message("Warning: this mode is for development only.")
    UninjectLibrary (code, dllName, 1000);
    UninjectLibrary (code, madCHookdllName, 1000);
#endif

    // FinalizeMadCHook is needed only if you're using the static madCHook.lib
    FinalizeMadCHook();

  }

  return true;
}
