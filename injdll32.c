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

#include "ansicon.h"

#ifdef _WIN64
#ifndef WOW64_CONTEXT_ALL
#include "wow64.h"

TWow64GetThreadContext Wow64GetThreadContext;
TWow64SetThreadContext Wow64SetThreadContext;
#define IMPORT_WOW64
#endif

#define CONTEXT 	 WOW64_CONTEXT
#undef	CONTEXT_CONTROL
#define CONTEXT_CONTROL  WOW64_CONTEXT_CONTROL
#define GetThreadContext Wow64GetThreadContext
#define SetThreadContext Wow64SetThreadContext

#define MakeVA( cast, offset ) (cast)((DWORD_PTR)base + (DWORD)(offset))

extern DWORD LLW32;
LPVOID base;

int export_cmp( const void* a, const void* b )
{
  return strcmp( (LPCSTR)a, MakeVA( LPCSTR, *(const PDWORD)b ) );
}


/*
  Get the address of the 32-bit LoadLibraryW function from 64-bit code.  This
  was originally done via executing a helper program (ANSI-LLW.exe), but for
  some reason, virus scanners really didn't like it (even when I rewrote it in
  assembly so it was 1024 bytes and literally two instructions, virustotal
  still had three scanners complaining).  Now I do it the "hard" way - load the
  32-bit kernel32.dll directly and search the exports.	Even worse, it seems
  Wow64 loads kernel32.dll at a different base address each boot (at least, I
  hope it only changes each boot).  Fortunately, loading the DLL as an image in
  64-bit code seems to use the 32-bit address.
*/
BOOL get_LLW32( void )
{
  HMODULE kernel32;
  TCHAR   buf[MAX_PATH];
  UINT	  len;
  PIMAGE_DOS_HEADER	  pDosHeader;
  PIMAGE_NT_HEADERS32	  pNTHeader;
  PIMAGE_EXPORT_DIRECTORY pExportDir;
  PDWORD  fun_table, name_table;
  PWORD   ord_table;
  PDWORD  pLLW;

  len = GetSystemWow64Directory( buf, MAX_PATH );
  wcscpy( buf + len, L"\\kernel32.dll" );
  // MinGW-w64 has a typo, calling it LINRARY.
  kernel32 = LoadLibraryEx( buf, NULL, 0x20/*LOAD_LIBRARY_AS_IMAGE_RESOURCE*/ );
  if (kernel32 == NULL)
  {
    DEBUGSTR( 1, L"Unable to load 32-bit kernel32.dll!" );
    return FALSE;
  }
  // The handle uses low bits as flags, so strip 'em off.
  base = (LPVOID)((DWORD_PTR)kernel32 & 0xFFFF0000);

  // Tests to make sure we're looking at a module image (the 'MZ' header)
  pDosHeader = (PIMAGE_DOS_HEADER)base;
  if (pDosHeader->e_magic != IMAGE_DOS_SIGNATURE)
  {
    DEBUGSTR( 1, L"Image has no DOS header!" );
    return FALSE;
  }

  // The MZ header has a pointer to the PE header
  pNTHeader = MakeVA( PIMAGE_NT_HEADERS32, pDosHeader->e_lfanew );

  // One more test to make sure we're looking at a "PE" image
  if (pNTHeader->Signature != IMAGE_NT_SIGNATURE)
  {
    DEBUGSTR( 1, L"Image has no NT header!" );
    return FALSE;
  }

  // We now have a valid pointer to the module's PE header.
  // Get a pointer to its exports section.
  pExportDir = MakeVA( PIMAGE_EXPORT_DIRECTORY,
			pNTHeader->OptionalHeader.
			 DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].
			  VirtualAddress );

  fun_table  = MakeVA( PDWORD, pExportDir->AddressOfFunctions );
  name_table = MakeVA( PDWORD, pExportDir->AddressOfNames );
  ord_table  = MakeVA( PWORD,  pExportDir->AddressOfNameOrdinals );

  pLLW = bsearch( "LoadLibraryW", name_table, pExportDir->NumberOfNames,
		  sizeof(DWORD), export_cmp );
  if (pLLW == NULL)
  {
    DEBUGSTR( 1, L"Could not find LoadLibraryW!" );
    return FALSE;
  }
  LLW32 = MakeVA( DWORD, fun_table[ord_table[pLLW - name_table]] );

  FreeLibrary( kernel32 );
  return TRUE;
}
#else
DWORD LLW32;
#endif


void InjectDLL32( LPPROCESS_INFORMATION ppi, LPCTSTR dll )
{
  CONTEXT context;
  DWORD   len;
  LPVOID  mem;
  DWORD   mem32;
  #define CODESIZE 20
  BYTE	  code[CODESIZE+TSIZE(MAX_PATH)];
  union
  {
    PBYTE  pB;
    PDWORD pL;
  } ip;

  len = TSIZE(lstrlen( dll ) + 1);
  if (len > TSIZE(MAX_PATH))
    return;

  if (LLW32 == 0)
  {
#ifdef _WIN64
    if (!get_LLW32())
      return;
#else
    LLW32 = (DWORD)GetProcAddress( GetModuleHandleA( "kernel32.dll" ),
						     "LoadLibraryW" );
#endif
  }
#ifdef IMPORT_WOW64
  if (Wow64GetThreadContext == 0)
  {
    #define GETPROC( proc ) proc = (T##proc)GetProcAddress( hKernel, #proc )
    HMODULE hKernel = GetModuleHandleA( "kernel32.dll" );
    GETPROC( Wow64GetThreadContext );
    GETPROC( Wow64SetThreadContext );
    // Assume if one is defined, so is the other.
    if (Wow64GetThreadContext == 0)
    {
      DEBUGSTR( 1, L"Failed to get pointer to Wow64GetThreadContext.\n" );
      return;
    }
  }
#endif

  CopyMemory( code + CODESIZE, dll, len );
  len += CODESIZE;

  context.ContextFlags = CONTEXT_CONTROL;
  GetThreadContext( ppi->hThread, &context );
  mem = VirtualAllocEx( ppi->hProcess, NULL, len, MEM_COMMIT,
			PAGE_EXECUTE_READWRITE );
  mem32 = (DWORD)(DWORD_PTR)mem;

  ip.pB = code;

  *ip.pB++ = 0x68;			// push  eip
  *ip.pL++ = context.Eip;
  *ip.pB++ = 0x9c;			// pushf
  *ip.pB++ = 0x60;			// pusha
  *ip.pB++ = 0x68;			// push  L"path\to\ANSI32.dll"
  *ip.pL++ = mem32 + CODESIZE;
  *ip.pB++ = 0xe8;			// call  LoadLibraryW
  *ip.pL++ = LLW32 - (mem32 + (DWORD)(ip.pB+4 - code));
  *ip.pB++ = 0x61;			// popa
  *ip.pB++ = 0x9d;			// popf
  *ip.pB++ = 0xc3;			// ret

  WriteProcessMemory( ppi->hProcess, mem, code, len, NULL );
  FlushInstructionCache( ppi->hProcess, mem, len );
  context.Eip = mem32;
  SetThreadContext( ppi->hThread, &context );
}
