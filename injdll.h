/*
  injdll.h - Simple header file for injecting the DLL.

  Jason Hood, 20 June, 2009.
*/

#ifndef INJDLL_H
#define INJDLL_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

void InjectDLL32( LPPROCESS_INFORMATION, LPCSTR );
void InjectDLL64( LPPROCESS_INFORMATION, LPCSTR );

#endif
