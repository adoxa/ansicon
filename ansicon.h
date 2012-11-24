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
#ifdef _WIN64
#define _WIN32_WINNT 0x0600	// MinGW-w64 wants this defined for Wow64 stuff
#else
#define _WIN32_WINNT 0x0500	// MinGW wants this defined for OpenThread
#endif
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#define lenof(array) (sizeof(array)/sizeof(*(array)))
#define TSIZE(size)  ((size) * sizeof(TCHAR))


typedef struct
{
  BYTE foreground;	// ANSI base color (0 to 7; add 30)
  BYTE background;	// ANSI base color (0 to 7; add 40)
  BYTE bold;		// console FOREGROUND_INTENSITY bit
  BYTE underline;	// console BACKGROUND_INTENSITY bit
  BYTE rvideo;		// swap foreground/bold & background/underline
  BYTE concealed;	// set foreground/bold to background/underline
  BYTE reverse; 	// swap console foreground & background attributes
} GRM, *PGRM;		// Graphic Rendition Mode


int  ProcessType( LPPROCESS_INFORMATION, BOOL* );
void InjectDLL32( LPPROCESS_INFORMATION, LPCTSTR );
void InjectDLL64( LPPROCESS_INFORMATION, LPCTSTR );

extern TCHAR  prog_path[MAX_PATH];
extern LPTSTR prog;
LPTSTR get_program_name( LPTSTR );

extern int log_level;
void DEBUGSTR( int level, LPTSTR szFormat, ... );

#endif
