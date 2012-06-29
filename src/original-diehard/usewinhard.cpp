/**
 * @file   usewinhard.cpp
 * @brief  For Windows: link with your executable to use DieHard.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 * @note   Copyright (C) 2005 by Emery Berger, University of Massachusetts Amherst.
 *
 **/

#include <windows.h>
#pragma comment(linker, "/include:_ReferenceDieHard")

extern "C" {

  __declspec(dllimport) int ReferenceDieHardStub;
  
  void ReferenceDieHard (void)
  {
    LoadLibraryA ("windiehard.dll");
    ReferenceDieHardStub = 1; 
  }

}
