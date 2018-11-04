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
#define WINVER _WIN32_WINNT
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef INVALID_FILE_ATTRIBUTES
#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
#endif
#ifndef LOAD_LIBRARY_AS_IMAGE_RESOURCE
#define LOAD_LIBRARY_AS_IMAGE_RESOURCE 0x20
#endif
#ifndef LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE
#define LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE 0x20
#endif
#ifndef TH32CS_SNAPMODULE32
#define TH32CS_SNAPMODULE32 0x10
#endif
#if !defined(HandleToULong) && !defined(_WIN64)
#define HandleToULong HandleToUlong
#endif

#ifndef __IMAGE_COR20_HEADER_DEFINED__
#define COMIMAGE_FLAGS_ILONLY	     1
#define COMIMAGE_FLAGS_32BITREQUIRED 2

// CLR 2.0 header structure.
typedef struct IMAGE_COR20_HEADER
{
    DWORD                   cb;
    WORD                    MajorRuntimeVersion;
    WORD                    MinorRuntimeVersion;
    IMAGE_DATA_DIRECTORY    MetaData;
    DWORD                   Flags;
    union {
        DWORD               EntryPointToken;
        DWORD               EntryPointRVA;
    } DUMMYUNIONNAME;
    IMAGE_DATA_DIRECTORY    Resources;
    IMAGE_DATA_DIRECTORY    StrongNameSignature;
    IMAGE_DATA_DIRECTORY    CodeManagerTable;
    IMAGE_DATA_DIRECTORY    VTableFixups;
    IMAGE_DATA_DIRECTORY    ExportAddressTableJumps;
    IMAGE_DATA_DIRECTORY    ManagedNativeHeader;
} IMAGE_COR20_HEADER, *PIMAGE_COR20_HEADER;
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


#ifdef PDATE	       // i.e. from ansicon.c
#define EXTERN __declspec(dllimport) extern
#else
#define EXTERN __declspec(dllexport) extern
#endif

EXTERN BOOL IsConsoleHandle( HANDLE );
EXTERN int ProcessType( LPPROCESS_INFORMATION, PBYTE*, BOOL* );
BOOL   Wow64Process( HANDLE );

#ifdef _WIN64
EXTERN
#endif
void   InjectDLL( LPPROCESS_INFORMATION, PBYTE );
void   RemoteLoad32( LPPROCESS_INFORMATION );
#ifdef _WIN64
void   InjectDLL32( LPPROCESS_INFORMATION, PBYTE );
EXTERN void  RemoteLoad64( LPPROCESS_INFORMATION );
EXTERN DWORD GetProcRVA( LPCTSTR, LPCSTR, int );
#else
EXTERN DWORD GetProcRVA( LPCTSTR, LPCSTR );
#endif

extern HANDLE hHeap;

EXTERN TCHAR  prog_path[MAX_PATH];
extern LPTSTR prog;
LPTSTR get_program_name( LPTSTR );

EXTERN TCHAR  DllName[MAX_PATH];
EXTERN LPTSTR DllNameType;
extern char   ansi_dll[MAX_PATH];
extern DWORD  ansi_len;
extern char*  ansi_bits;
void   set_ansi_dll( void );
DWORD  get_os_version( void );

EXTERN int  log_level;
EXTERN void DEBUGSTR( int level, LPCSTR szFormat, ... );

// Replacements for C runtime functions.
#ifdef _MSC_VER
#undef RtlFillMemory
#undef RtlMoveMemory
#undef RtlZeroMemory
void WINAPI RtlFillMemory( PVOID, SIZE_T, BYTE );
void WINAPI RtlMoveMemory( PVOID, const VOID*, SIZE_T );
void WINAPI RtlZeroMemory( PVOID, SIZE_T );
#endif

#define arrcpy( dst, src ) RtlMoveMemory( dst, src, sizeof(dst) )

unsigned long ac_wcstoul( const wchar_t*, wchar_t**, int );
int	      ac_wtoi( const wchar_t* );
long	      ac_wcstol( const wchar_t*, wchar_t**, int );
wchar_t*      ac_wcspbrk( const wchar_t*, const wchar_t* );
wchar_t*      ac_wcsrchr( const wchar_t*, wchar_t );
int	      ac_strnicmp( const char*, const char*, size_t );
int	      ac_sprintf( char*, const char*, ... );
int	      ac_wprintf( wchar_t*, const char*, ... );

#endif
