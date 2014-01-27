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

extern DWORD LLW64r;
extern LPVOID kernel32_base;
extern PIMAGE_DOS_HEADER pDosHeader;

#define MakeVA( cast, offset ) (cast)((DWORD_PTR)pDosHeader + (DWORD)(offset))

extern int export_cmp( const void* a, const void* b );


/*
  Get the relative address of LoadLibraryW direct from kernel32.dll.
*/
BOOL get_LLW64r( void )
{
  HMODULE kernel32;
  TCHAR   buf[MAX_PATH];
  UINT	  len;
  PIMAGE_NT_HEADERS	  pNTHeader;
  PIMAGE_EXPORT_DIRECTORY pExportDir;
  PDWORD  fun_table, name_table;
  PWORD   ord_table;
  PDWORD  pLLW;

  len = GetSystemDirectory( buf, MAX_PATH );
  wcscpy( buf + len, L"\\kernel32.dll" );
  kernel32 = LoadLibraryEx( buf, NULL, LOAD_LIBRARY_AS_IMAGE_RESOURCE );
  if (kernel32 == NULL)
  {
    DEBUGSTR( 1, L"Unable to load 64-bit kernel32.dll!" );
    return FALSE;
  }
  // The handle uses low bits as flags, so strip 'em off.
  pDosHeader = (PIMAGE_DOS_HEADER)((DWORD_PTR)kernel32 & ~0xFFFF);
  pNTHeader  = MakeVA( PIMAGE_NT_HEADERS, pDosHeader->e_lfanew );
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
  LLW64r = fun_table[ord_table[pLLW - name_table]];

  FreeLibrary( kernel32 );
  return TRUE;
}


void InjectDLL64( LPPROCESS_INFORMATION ppi, LPCTSTR dll )
{
  CONTEXT context;
  DWORD64 ep;
  BOOL	  rip;
  LPVOID  mem;
  DWORD   pr;
  DWORD64 LLW;

  union
  {
    PBYTE    pB;
    PDWORD64 pL;
  } ip;

  struct unicode_string
  {
    USHORT  Length;
    USHORT  MaximumLength;
    DWORD64 Buffer;
  };
  struct ldr_module		// incomplete definition
  {
    DWORD64 next, prev;
    DWORD64 baseAddress;
    DWORD64 entryPoint;
    DWORD64 sizeOfImage;
    struct unicode_string fullDllName;
    struct unicode_string baseDllName;
  } ldr;
  WCHAR basename[MAX_PATH];

  DWORD   len;
  #define CODESIZE 92
  static BYTE code[CODESIZE+TSIZE(MAX_PATH)] = {
	0,0,0,0,0,0,0,0,	   // original rip
	0,0,0,0,0,0,0,0,	   // LoadLibraryW
	0x9C,			   // pushfq
	0x50,			   // push  rax
	0x51,			   // push  rcx
	0x52,			   // push  rdx
	0x53,			   // push  rbx
	0x55,			   // push  rbp
	0x56,			   // push  rsi
	0x57,			   // push  rdi
	0x41,0x50,		   // push  r8
	0x41,0x51,		   // push  r9
	0x41,0x52,		   // push  r10
	0x41,0x53,		   // push  r11
	0x41,0x54,		   // push  r12
	0x41,0x55,		   // push  r13
	0x41,0x56,		   // push  r14
	0x41,0x57,		   // push  r15
	0x48,0x83,0xEC,0x28,	   // sub   rsp, 40
	0x48,0x8D,0x0D,41,0,0,0,   // lea   ecx, L"path\to\ANSI64.dll"
	0xFF,0x15,-49,-1,-1,-1,    // call  LoadLibraryW
	0x48,0x83,0xC4,0x28,	   // add   rsp, 40
	0x41,0x5F,		   // pop   r15
	0x41,0x5E,		   // pop   r14
	0x41,0x5D,		   // pop   r13
	0x41,0x5C,		   // pop   r12
	0x41,0x5B,		   // pop   r11
	0x41,0x5A,		   // pop   r10
	0x41,0x59,		   // pop   r9
	0x41,0x58,		   // pop   r8
	0x5F,			   // pop   rdi
	0x5E,			   // pop   rsi
	0x5D,			   // pop   rbp
	0x5B,			   // pop   rbx
	0x5A,			   // pop   rdx
	0x59,			   // pop   rcx
	0x58,			   // pop   rax
	0x9D,			   // popfq
	0xFF,0x25,-91,-1,-1,-1,    // jmp   original Rip
	0,			   // dword alignment for LLW, fwiw
  };

  len = TSIZE(lstrlen( dll ) + 1);
  if (len > TSIZE(MAX_PATH))
    return;
  CopyMemory( code + CODESIZE, dll, len );
  len += CODESIZE;

  context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
  GetThreadContext( ppi->hThread, &context );
  mem = VirtualAllocEx( ppi->hProcess, NULL, len, MEM_COMMIT,
			PAGE_READWRITE );

  ip.pB = code;

  // Determine the base address of kernel32.dll.  If injecting into the parent
  // process, the base has already been determined.  Otherwise, use the PEB to
  // walk the loaded modules.
  if (kernel32_base != 0)
  {
    ep = context.Rip;
    rip = TRUE;
  }
  else
  {
    // When a process is created suspended, RCX has the entry point and RDX
    // points to the PEB.
    if (!ReadProcessMemory( ppi->hProcess, (LPVOID)(context.Rdx + 0x18),
			    ip.pL, 8, NULL ))
    {
      DEBUGSTR( 1, L"Failed to read Ldr from PEB." );
      return;
    }
    ep = context.Rcx;
    rip = FALSE;
    // In case we're a bit slow (which seems to be unlikely), set up an
    // infinite loop as the entry point.
    WriteProcessMemory( ppi->hProcess, (PBYTE)mem + 16, "\xEB\xFE", 2, NULL );
    FlushInstructionCache( ppi->hProcess, (PBYTE)mem + 16, 2 );
    context.Rcx = (DWORD64)mem + 16;
    SetThreadContext( ppi->hThread, &context );
    VirtualProtectEx( ppi->hProcess, mem, len, PAGE_EXECUTE, &pr );
    // Now resume the thread, as the PEB hasn't even been created yet.
    ResumeThread( ppi->hThread );
    while (*ip.pL == 0)
    {
      Sleep( 0 );
      ReadProcessMemory( ppi->hProcess, (LPVOID)(context.Rdx + 0x18),
			 ip.pL, 8, NULL );
    }
    // Read PEB_LDR_DATA.InInitializationOrderModuleList.Flink.
    ReadProcessMemory( ppi->hProcess, (LPVOID)(*ip.pL + 0x30),
		       &ip.pL[1], 8, NULL );
    // Sometimes we're so quick ntdll.dll is the only one present, so keep
    // looping until kernel32.dll shows up.
    for (;;)
    {
      ldr.next = ip.pL[1];
      do
      {
	ReadProcessMemory( ppi->hProcess, (LPVOID)ldr.next,
			   &ldr, sizeof(ldr), NULL );
	ReadProcessMemory( ppi->hProcess, (LPVOID)ldr.baseDllName.Buffer,
			   basename, ldr.baseDllName.MaximumLength, NULL );
	if (_wcsicmp( basename, L"kernel32.dll" ) == 0)
	{
	  kernel32_base = (LPVOID)ldr.baseAddress;
	  goto gotit;
	}
      } while (ldr.next != *ip.pL + 0x30);
    }
  gotit:
    SuspendThread( ppi->hThread );
    VirtualProtectEx( ppi->hProcess, mem, len, pr, &pr );
  }
  LLW = (DWORD64)kernel32_base + LLW64r;
  kernel32_base = 0;

  *ip.pL++ = ep;
  *ip.pL++ = LLW;

  WriteProcessMemory( ppi->hProcess, mem, code, len, NULL );
  FlushInstructionCache( ppi->hProcess, mem, len );
  VirtualProtectEx( ppi->hProcess, mem, len, PAGE_EXECUTE, &pr );

  if (rip)
  {
    context.Rip = (DWORD64)mem + 16;
    SetThreadContext( ppi->hThread, &context );
  }
}
