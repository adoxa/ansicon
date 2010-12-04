/*
  injdll.h - Simple header file for injecting the DLL.

  Jason Hood, 20 June, 2009.
*/

#ifndef INJDLL_H
#define INJDLL_H

#ifndef UNICODE
# define UNICODE
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

BOOL ProcessType( LPPROCESS_INFORMATION );
void InjectDLL32( LPPROCESS_INFORMATION, LPCTSTR );
void InjectDLL64( LPPROCESS_INFORMATION, LPCTSTR );

#endif
