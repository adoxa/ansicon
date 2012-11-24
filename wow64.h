/*
  wow64.h - Definitions for Wow64.

  The 2003 Platform SDK does not include these Wow64 definitions.
*/

#ifndef WOW64_H
#define WOW64_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define WOW64_CONTEXT_i386	0x00010000

#define WOW64_CONTEXT_CONTROL		    (WOW64_CONTEXT_i386 | 0x00000001L)
#define WOW64_CONTEXT_INTEGER		    (WOW64_CONTEXT_i386 | 0x00000002L)
#define WOW64_CONTEXT_SEGMENTS		    (WOW64_CONTEXT_i386 | 0x00000004L)
#define WOW64_CONTEXT_FLOATING_POINT	    (WOW64_CONTEXT_i386 | 0x00000008L)
#define WOW64_CONTEXT_DEBUG_REGISTERS	    (WOW64_CONTEXT_i386 | 0x00000010L)
#define WOW64_CONTEXT_EXTENDED_REGISTERS    (WOW64_CONTEXT_i386 | 0x00000020L)

#define WOW64_CONTEXT_FULL      (WOW64_CONTEXT_CONTROL | WOW64_CONTEXT_INTEGER | WOW64_CONTEXT_SEGMENTS)

#define WOW64_CONTEXT_ALL       (WOW64_CONTEXT_CONTROL | WOW64_CONTEXT_INTEGER | WOW64_CONTEXT_SEGMENTS | \
                                 WOW64_CONTEXT_FLOATING_POINT | WOW64_CONTEXT_DEBUG_REGISTERS | \
                                 WOW64_CONTEXT_EXTENDED_REGISTERS)

#define WOW64_SIZE_OF_80387_REGISTERS      80

#define WOW64_MAXIMUM_SUPPORTED_EXTENSION     512

typedef struct _WOW64_FLOATING_SAVE_AREA {
    DWORD   ControlWord;
    DWORD   StatusWord;
    DWORD   TagWord;
    DWORD   ErrorOffset;
    DWORD   ErrorSelector;
    DWORD   DataOffset;
    DWORD   DataSelector;
    BYTE    RegisterArea[WOW64_SIZE_OF_80387_REGISTERS];
    DWORD   Cr0NpxState;
} WOW64_FLOATING_SAVE_AREA;

typedef WOW64_FLOATING_SAVE_AREA *PWOW64_FLOATING_SAVE_AREA;

typedef struct _WOW64_CONTEXT {

    DWORD ContextFlags;

    DWORD   Dr0;
    DWORD   Dr1;
    DWORD   Dr2;
    DWORD   Dr3;
    DWORD   Dr6;
    DWORD   Dr7;

    WOW64_FLOATING_SAVE_AREA FloatSave;

    DWORD   SegGs;
    DWORD   SegFs;
    DWORD   SegEs;
    DWORD   SegDs;

    DWORD   Edi;
    DWORD   Esi;
    DWORD   Ebx;
    DWORD   Edx;
    DWORD   Ecx;
    DWORD   Eax;

    DWORD   Ebp;
    DWORD   Eip;
    DWORD   SegCs;
    DWORD   EFlags;
    DWORD   Esp;
    DWORD   SegSs;

    BYTE    ExtendedRegisters[WOW64_MAXIMUM_SUPPORTED_EXTENSION];

} WOW64_CONTEXT;

typedef WOW64_CONTEXT *PWOW64_CONTEXT;


typedef BOOL (WINAPI *TWow64GetThreadContext)( HANDLE hThread, PWOW64_CONTEXT lpContext );
typedef BOOL (WINAPI *TWow64SetThreadContext)( HANDLE hThread, CONST WOW64_CONTEXT *lpContext );

#endif
