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

#define MakeVA( cast, offset ) (cast)((DWORD_PTR)pDosHeader + (DWORD)(offset))

extern DWORD LLW32;
LPVOID base;
static PIMAGE_DOS_HEADER pDosHeader;

int export_cmp( const void* a, const void* b )
{
  return strcmp( (LPCSTR)a, MakeVA( LPCSTR, *(const PDWORD)b ) );
}


/*
  Get the relative address of the 32-bit LoadLibraryW function from 64-bit code.
  This was originally done via executing a helper program (ANSI-LLW.exe), but I
  never liked doing that, so now I do it the "hard" way - load the 32-bit
  kernel32.dll directly and search the exports.
*/
BOOL get_LLW32( void )
{
  HMODULE kernel32;
  TCHAR   buf[MAX_PATH];
  UINT	  len;
  PIMAGE_NT_HEADERS32	  pNTHeader;
  PIMAGE_EXPORT_DIRECTORY pExportDir;
  PDWORD  fun_table, name_table;
  PWORD   ord_table;
  PDWORD  pLLW;

  len = GetSystemWow64Directory( buf, MAX_PATH );
  wcscpy( buf + len, L"\\kernel32.dll" );
  // MinGW-w64 had a typo, calling it LINRARY.
  kernel32 = LoadLibraryEx( buf, NULL, 0x20/*LOAD_LIBRARY_AS_IMAGE_RESOURCE*/ );
  if (kernel32 == NULL)
  {
    DEBUGSTR( 1, L"Unable to load 32-bit kernel32.dll!" );
    return FALSE;
  }
  // The handle uses low bits as flags, so strip 'em off.
  pDosHeader = (PIMAGE_DOS_HEADER)((DWORD_PTR)kernel32 & ~0xFFFF);
  pNTHeader  = MakeVA( PIMAGE_NT_HEADERS32, pDosHeader->e_lfanew );
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
    FreeLibrary( kernel32 );
    return FALSE;
  }
  LLW32 = fun_table[ord_table[pLLW - name_table]];

  FreeLibrary( kernel32 );
  return TRUE;
}
#else
DWORD LLW32;
#endif


void InjectDLL32( LPPROCESS_INFORMATION ppi, LPCTSTR dll )
{
  CONTEXT context;
  DWORD   ep;
  DWORD   len;
  LPVOID  mem;
  DWORD   mem32;
  DWORD   pr;
  #define CODESIZE 20
  BYTE	  code[CODESIZE+TSIZE(MAX_PATH)];
  union
  {
    PBYTE  pB;
    PDWORD pL;
  } ip;
#ifdef _WIN64
  BOOL entry = FALSE;
#endif

#ifdef IMPORT_WOW64
  if (Wow64GetThreadContext == 0)
  {
    #define GETPROC( proc ) proc = (T##proc)GetProcAddress( hKernel, #proc )
    HMODULE hKernel = GetModuleHandle( L"kernel32.dll" );
    GETPROC( Wow64GetThreadContext );
    GETPROC( Wow64SetThreadContext );
    // Assume if one is defined, so is the other.
    if (Wow64GetThreadContext == 0)
    {
      DEBUGSTR( 1, L"Failed to get pointer to Wow64GetThreadContext." );
      return;
    }
  }
#endif

  len = TSIZE(lstrlen( dll ) + 1);
  if (len > TSIZE(MAX_PATH))
    return;

  CopyMemory( code + CODESIZE, dll, len );
  len += CODESIZE;

  context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
  GetThreadContext( ppi->hThread, &context );
  mem = VirtualAllocEx( ppi->hProcess, NULL, len, MEM_COMMIT,
			PAGE_READWRITE );
  mem32 = (DWORD)(DWORD_PTR)mem;

  ip.pB = code;

  ep = context.Eip;
  if (LLW32 == 0)
  {
#ifndef _WIN64
    LLW32 = (DWORD)GetProcAddress( GetModuleHandle( L"kernel32.dll" ),
						     "LoadLibraryW" );
#else
    struct unicode_string
    {
      USHORT Length;
      USHORT MaximumLength;
      DWORD  Buffer;
    };
    struct ldr_module		// incomplete definition
    {
      DWORD next, prev;
      DWORD baseAddress;
      DWORD entryPoint;
      DWORD sizeOfImage;
      struct unicode_string fullDllName;
      struct unicode_string baseDllName;
    } ldr;
    WCHAR basename[MAX_PATH];

    if (!get_LLW32())
      return;
    // Determine the base address of the 32-bit kernel32.dll.  If injecting
    // into the parent process, base has already been determined.  Otherwise,
    // use the PEB to walk the loaded modules.
    if (base == 0)
    {
      // When a process is created suspended, EAX has the entry point and EBX
      // points to the PEB.
      if (!ReadProcessMemory( ppi->hProcess, UIntToPtr( context.Ebx + 0x0C ),
			      ip.pL, 4, NULL ))
      {
	DEBUGSTR( 1, L"Failed to read Ldr from PEB." );
	return;
      }
      // In case we're a bit slow (which seems to be unlikely), set up an
      // infinite loop as the entry point.
      WriteProcessMemory( ppi->hProcess, mem, "\xEB\xFE", 2, NULL );
      FlushInstructionCache( ppi->hProcess, mem, 2 );
      ep = context.Eax;
      context.Eax = mem32;
      SetThreadContext( ppi->hThread, &context );
      VirtualProtectEx( ppi->hProcess, mem, len, PAGE_EXECUTE, &pr );
      // Now resume the thread, as the PEB hasn't even been created yet.
      ResumeThread( ppi->hThread );
      while (*ip.pL == 0)
      {
	Sleep( 0 );
	ReadProcessMemory( ppi->hProcess, UIntToPtr( context.Ebx + 0x0C ),
			   ip.pL, 4, NULL );
      }
      // Read PEB_LDR_DATA.InInitializationOrderModuleList.Flink.
      ReadProcessMemory( ppi->hProcess, UIntToPtr( *ip.pL + 0x1c ),
			 &ip.pL[1], 4, NULL );
      // Sometimes we're so quick ntdll.dll is the only one present, so keep
      // looping until kernel32.dll shows up.
      for (;;)
      {
	ldr.next = ip.pL[1];
	do
	{
	  ReadProcessMemory( ppi->hProcess, UIntToPtr( ldr.next ),
			     &ldr, sizeof(ldr), NULL );
	  ReadProcessMemory( ppi->hProcess, UIntToPtr( ldr.baseDllName.Buffer ),
			     basename, ldr.baseDllName.MaximumLength, NULL );
	  if (_wcsicmp( basename, L"kernel32.dll" ) == 0)
	  {
	    LLW32 += ldr.baseAddress;
	    goto gotit;
	  }
	} while (ldr.next != *ip.pL + 0x1c);
      }
    gotit:
      SuspendThread( ppi->hThread );
      VirtualProtectEx( ppi->hProcess, mem, len, pr, &pr );
      entry = TRUE;
    }
    else
      LLW32 += PtrToUint( base );
#endif
  }

  *ip.pB++ = 0x68;			// push  ep
  *ip.pL++ = ep;
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
  VirtualProtectEx( ppi->hProcess, mem, len, PAGE_EXECUTE, &pr );
#ifdef _WIN64
  if (entry)
    return;
#endif
  context.Eip = mem32;
  SetThreadContext( ppi->hThread, &context );
}
