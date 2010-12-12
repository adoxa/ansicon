/*
  ansicon.h - Header file for common definitions.

  Jason Hood, 12 December, 2010 (originally injdll.h, 20 June, 2009).
*/

#ifndef ANSICON_H
#define ANSICON_H

#ifndef UNICODE
# define UNICODE
#endif

#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0500	// MinGW wants this defined for OpenThread
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#define lenof(array) (sizeof(array)/sizeof(*(array)))


BOOL ProcessType( LPPROCESS_INFORMATION );
void InjectDLL32( LPPROCESS_INFORMATION, LPCTSTR );
void InjectDLL64( LPPROCESS_INFORMATION, LPCTSTR );


// ========== Auxiliary debug function

//#define MYDEBUG 1	// use OutputDebugString
#define MYDEBUG 2	// use %temp%\ansicon.log
#ifndef MYDEBUG
#  define MYDEBUG 0	// no debugging
#endif

#if (MYDEBUG > 0)
    void DEBUGSTR( LPTSTR szFormat, ... );
#else
#   if defined(_MSC_VER) && _MSC_VER <= 1400
      void DEBUGSTR() { }
#   else
#     define DEBUGSTR(...)
#   endif
#endif

#endif
