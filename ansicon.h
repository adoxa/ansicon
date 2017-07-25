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
#define _WIN32_WINNT 0x0501	// at least XP required
#else
#define _WIN32_WINNT 0x0500	// at least Windows 2000 required
#endif
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef LOAD_LIBRARY_AS_IMAGE_RESOURCE
#define LOAD_LIBRARY_AS_IMAGE_RESOURCE 0x20
#endif
#ifndef LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE
#define LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE 0x20
#endif

#define lenof(array) (sizeof(array)/sizeof(*(array)))
#define TSIZE(size)  ((size) * sizeof(TCHAR))
#define PTRSZ	     sizeof(PVOID)

// Macro for adding pointers/DWORDs together without C arithmetic interfering
#define MakeVA( cast, offset ) (cast)((DWORD_PTR)pDosHeader + (DWORD)(offset))

#define DATADIRS  OptionalHeader.NumberOfRvaAndSizes
#define EXPORTDIR OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT]
#define IMPORTDIR OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT]
#define BOUNDDIR  OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT]
#define IATDIR	  OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IAT]
#define COMDIR	  OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR]

// Reduce the verbosity of some functions (assuming variable names).
#define ReadProcVar(a, b)     ReadProcMem( a, b, sizeof(*(b)) )
#define WriteProcVar(a, b)    WriteProcMem( a, b, sizeof(*(b)) )
#define ReadProcMem(a, b, c)  ReadProcessMemory( ppi->hProcess, a, b, c, NULL )
#define WriteProcMem(a, b, c) WriteProcessMemory( ppi->hProcess, a, b, c, NULL )
#define VirtProtVar(a, b)     VirtualProtectEx( ppi->hProcess, a, sizeof(*(a)), b, &pr )


int    ProcessType( LPPROCESS_INFORMATION, PBYTE*, BOOL* );
BOOL   Wow64Process( HANDLE );

void   InjectDLL( LPPROCESS_INFORMATION, PBYTE );
#ifdef _WIN64
void   InjectDLL32( LPPROCESS_INFORMATION, PBYTE );
void   InjectDLL64( LPPROCESS_INFORMATION );
DWORD  GetProcRVA( LPCTSTR, LPCSTR, int );
#else
DWORD  GetProcRVA( LPCTSTR, LPCSTR );
#endif

extern HANDLE hHeap;

extern TCHAR  prog_path[MAX_PATH];
extern LPTSTR prog;
LPTSTR get_program_name( LPTSTR );

extern TCHAR  DllName[MAX_PATH];
extern LPTSTR DllNameType;
extern char   ansi_dll[MAX_PATH];
extern DWORD  ansi_len;
extern char*  ansi_bits;
void   set_ansi_dll( void );

extern int log_level;
void   DEBUGSTR( int level, LPCSTR szFormat, ... );

#endif
