/*
  Inject code into the target process to load our DLL.	The target thread
  should be suspended on entry; it remains suspended on exit.

  Initially I used the "stack" method of injection.  However, this fails
  when DEP is active, since that doesn't allow code to execute in the stack.
  To overcome this I used the "CreateRemoteThread" method.  However, this
  would fail with Wselect, a program to assist batch files.  Wselect runs,
  but it has no output.  As it turns out, removing the suspended flag would
  make Wselect work, but it caused problems with everything else.  So now I
  allocate a section of memory and change the context to run from there.  At
  first I had an event to signal when the library was loaded, then the memory
  was released.  However, that wouldn't work with -p and CMD.EXE (4NT v8
  worked fine).  Since it's possible the DLL might start a process suspended,
  I've decided to simply keep the memory.
*/

#include "injdll.h"

#ifdef _WIN64
#include "wow64.h"

TWow64GetThreadContext Wow64GetThreadContext;
TWow64SetThreadContext Wow64SetThreadContext;

#define CONTEXT 	 WOW64_CONTEXT
#undef	CONTEXT_CONTROL
#define CONTEXT_CONTROL  WOW64_CONTEXT_CONTROL
#define GetThreadContext Wow64GetThreadContext
#define SetThreadContext Wow64SetThreadContext
#endif


DWORD LLW;


void InjectDLL32( LPPROCESS_INFORMATION ppi, LPCWSTR dll )
{
  CONTEXT context;
  DWORD   len;
  LPVOID  mem;
  DWORD   mem32;
  #define CODESIZE 20
  BYTE	  code[CODESIZE+MAX_PATH*sizeof(WCHAR)];

  len = lstrlenW( dll ) + 1;
  if (len > MAX_PATH)
    return;
  len *= sizeof(WCHAR);

  if (LLW == 0)
  {
#ifdef _WIN64
    extern HMODULE hKernel;
    #define GETPROC( proc ) proc = (T##proc)GetProcAddress( hKernel, #proc )
    GETPROC( Wow64GetThreadContext );
    GETPROC( Wow64SetThreadContext );
    // Assume if one is defined, so is the other.
    if (Wow64GetThreadContext == 0)
      return;

    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    ZeroMemory( &si, sizeof(si) );
    si.cb = sizeof(si);
    // ...ANSI32.dll\0
    CopyMemory( code, dll, len - 7*sizeof(WCHAR) );
    // ...ANSI-LLA.exe\0
    CopyMemory( code + len - 7*sizeof(WCHAR), L"-LLA.exe", 9*sizeof(WCHAR) );
    if (!CreateProcess( (char*)code, NULL, NULL, NULL, FALSE, 0, NULL, NULL,
			&si, &pi ))
      return;
    WaitForSingleObject( pi.hProcess, INFINITE );
    GetExitCodeProcess( pi.hProcess, &LLW );
    CloseHandle( pi.hProcess );
    CloseHandle( pi.hThread );
#else
    LLW = (DWORD)LoadLibraryW;
#endif
  }

  CopyMemory( code + CODESIZE, dll, len );
  len += CODESIZE;

  context.ContextFlags = CONTEXT_CONTROL;
  GetThreadContext( ppi->hThread, &context );
  mem = VirtualAllocEx( ppi->hProcess, NULL, len, MEM_COMMIT,
			PAGE_EXECUTE_READWRITE );
  mem32 = (DWORD)(DWORD_PTR)mem;

  union
  {
    PBYTE  pB;
    PDWORD pL;
  } ip;
  ip.pB = code;

  *ip.pB++ = 0x68;			// push  eip
  *ip.pL++ = context.Eip;
  *ip.pB++ = 0x9c;			// pushf
  *ip.pB++ = 0x60;			// pusha
  *ip.pB++ = 0x68;			// push  L"path\to\ANSI32.dll"
  *ip.pL++ = mem32 + CODESIZE;
  *ip.pB++ = 0xe8;			// call  LoadLibraryW
  *ip.pL++ = LLW - (mem32 + (ip.pB+4 - code));
  *ip.pB++ = 0x61;			// popa
  *ip.pB++ = 0x9d;			// popf
  *ip.pB++ = 0xc3;			// ret

  WriteProcessMemory( ppi->hProcess, mem, code, len, NULL );
  FlushInstructionCache( ppi->hProcess, mem, len );
  context.Eip = mem32;
  SetThreadContext( ppi->hThread, &context );
}
