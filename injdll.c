/*
  Inject the DLL into the target process by modifying its import descriptor
  table.  The target process must have been created suspended.	However, for a
  64-bit system with a .NET AnyCPU process, inject via LdrLoadDll in ntdll.dll
  and CreateRemoteThread (since AnyCPU is stored as i386, but loads as AMD64,
  preventing imports from working).
*/

#include "ansicon.h"


// Search for a suitable free area after the main image (32-bit code could
// really go anywhere, but let's keep it relatively local.)
static PVOID FindMem( HANDLE hProcess, PBYTE base, DWORD len )
{
  MEMORY_BASIC_INFORMATION minfo;
  PBYTE ptr;
  PVOID mem;

  for (ptr = base;
       VirtualQueryEx( hProcess, ptr, &minfo, sizeof(minfo) );
       ptr += minfo.RegionSize)
  {
    if ((minfo.State & MEM_FREE) && minfo.RegionSize >= len)
    {
#ifdef _WIN64
      if ((PBYTE)minfo.BaseAddress - base > 0xFfFfFfFf - len)
	return NULL;
#endif
      mem = VirtualAllocEx( hProcess, (PVOID)
			    (((DWORD_PTR)minfo.BaseAddress + 0xFFFF) & ~0xFFFF),
			    len, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE );
      if (mem != NULL)
	return mem;
    }
  }
  return NULL;
}


// Count the imports directly (including the terminator), since the size of the
// import directory is not necessarily correct (Windows doesn't use it at all).
static DWORD sizeof_imports( LPPROCESS_INFORMATION ppi,
			     PBYTE pBase, DWORD rImports )
{
  IMAGE_IMPORT_DESCRIPTOR  import;
  PIMAGE_IMPORT_DESCRIPTOR pImports;
  DWORD cnt;

  if (rImports == 0)
    return 0;

  pImports = (PIMAGE_IMPORT_DESCRIPTOR)(pBase + rImports);
  cnt = 0;
  do
  {
    ++cnt;
    ReadProcVar( pImports++, &import );
  } while (import.Name != 0);

  return cnt * sizeof(import);
}


void InjectDLL( LPPROCESS_INFORMATION ppi, PBYTE pBase )
{
  DWORD rva;
  PVOID pMem;
  DWORD len, import_size;
  DWORD pr;
  IMAGE_DOS_HEADER	   DosHeader;
  IMAGE_NT_HEADERS	   NTHeader, *pNTHeader;
  PIMAGE_IMPORT_DESCRIPTOR pImports;
  IMAGE_COR20_HEADER	   ComHeader, *pComHeader;
  union
  {
    PBYTE     pB;
    PLONG_PTR pL;
    PIMAGE_IMPORT_DESCRIPTOR pI;
  } ip;

  ReadProcVar( pBase, &DosHeader );
  pNTHeader = (PIMAGE_NT_HEADERS)(pBase + DosHeader.e_lfanew);
  ReadProcVar( pNTHeader, &NTHeader );

  import_size = sizeof_imports( ppi, pBase, NTHeader.IMPORTDIR.VirtualAddress );
  len = 2 * PTRSZ + ansi_len + sizeof(*pImports) + import_size;
  pImports = HeapAlloc( hHeap, 0, len );
  if (pImports == NULL)
  {
    DEBUGSTR( 1, "  Failed to allocate memory" );
    return;
  }
  pMem = FindMem( ppi->hProcess, pBase, len );
  if (pMem == NULL)
  {
    DEBUGSTR( 1, "  Failed to allocate virtual memory (%u)", GetLastError() );
    HeapFree( hHeap, 0, pImports );
    return;
  }
  rva = (DWORD)((PBYTE)pMem - pBase);

  ip.pL = (PLONG_PTR)pImports;
  *ip.pL++ = IMAGE_ORDINAL_FLAG + 1;
  *ip.pL++ = 0;
  memcpy( ip.pB, ansi_dll, ansi_len );
  ip.pB += ansi_len;
  ip.pI->OriginalFirstThunk = 0;
  ip.pI->TimeDateStamp = 0;
  ip.pI->ForwarderChain = 0;
  ip.pI->Name = rva + 2 * PTRSZ;
  ip.pI->FirstThunk = rva;
  ReadProcMem( pBase+NTHeader.IMPORTDIR.VirtualAddress, ip.pI+1, import_size );
  WriteProcMem( pMem, pImports, len );
  HeapFree( hHeap, 0, pImports );

  // If there's no IAT, copy the original IDT (to allow writable ".idata").
  if (NTHeader.DATADIRS > IMAGE_DIRECTORY_ENTRY_IAT &&
      NTHeader.IATDIR.VirtualAddress == 0)
    NTHeader.IATDIR = NTHeader.IMPORTDIR;

  NTHeader.IMPORTDIR.VirtualAddress = rva + 2 * PTRSZ + ansi_len;
  //NTHeader.IMPORTDIR.Size += sizeof(*pImports);

  // Remove bound imports, so the updated import table is used.
  if (NTHeader.DATADIRS > IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT)
  {
    NTHeader.BOUNDDIR.VirtualAddress = 0;
    //NTHeader.BOUNDDIR.Size = 0;
  }

  VirtProtVar( pNTHeader, PAGE_READWRITE );
  WriteProcVar( pNTHeader, &NTHeader );
  VirtProtVar( pNTHeader, pr );

  // Remove the IL-only flag on a managed process.
  if (NTHeader.DATADIRS > IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR &&
      NTHeader.COMDIR.VirtualAddress != 0)
  {
    pComHeader = (PIMAGE_COR20_HEADER)(pBase + NTHeader.COMDIR.VirtualAddress);
    ReadProcVar( pComHeader, &ComHeader );
    if (ComHeader.Flags & COMIMAGE_FLAGS_ILONLY)
    {
      ComHeader.Flags &= ~COMIMAGE_FLAGS_ILONLY;
      VirtProtVar( pComHeader, PAGE_READWRITE );
      WriteProcVar( pComHeader, &ComHeader );
      VirtProtVar( pComHeader, pr );
    }
  }
}


#ifdef _WIN64
void InjectDLL32( LPPROCESS_INFORMATION ppi, PBYTE pBase )
{
  DWORD rva;
  PVOID pMem;
  DWORD len, import_size;
  DWORD pr;
  IMAGE_DOS_HEADER	   DosHeader;
  IMAGE_NT_HEADERS32	   NTHeader, *pNTHeader;
  PIMAGE_IMPORT_DESCRIPTOR pImports;
  IMAGE_COR20_HEADER	   ComHeader, *pComHeader;
  union
  {
    PBYTE pB;
    PLONG pL;
    PIMAGE_IMPORT_DESCRIPTOR pI;
  } ip;

  ReadProcVar( pBase, &DosHeader );
  pNTHeader = (PIMAGE_NT_HEADERS32)(pBase + DosHeader.e_lfanew);
  ReadProcVar( pNTHeader, &NTHeader );

  import_size = sizeof_imports( ppi, pBase, NTHeader.IMPORTDIR.VirtualAddress );
  len = 8 + ansi_len + sizeof(*pImports) + import_size;
  pImports = HeapAlloc( hHeap, 0, len );
  if (pImports == NULL)
  {
    DEBUGSTR( 1, "  Failed to allocate memory" );
    return;
  }
  pMem = FindMem( ppi->hProcess, pBase, len );
  if (pMem == NULL)
  {
    DEBUGSTR( 1, "  Failed to allocate virtual memory" );
    HeapFree( hHeap, 0, pImports );
    return;
  }
  rva = (DWORD)((PBYTE)pMem - pBase);

  ip.pL = (PLONG)pImports;
  *ip.pL++ = IMAGE_ORDINAL_FLAG32 + 1;
  *ip.pL++ = 0;
  memcpy( ip.pB, ansi_dll, ansi_len );
  ip.pB += ansi_len;
  ip.pI->OriginalFirstThunk = 0;
  ip.pI->TimeDateStamp = 0;
  ip.pI->ForwarderChain = 0;
  ip.pI->Name = rva + 8;
  ip.pI->FirstThunk = rva;
  ReadProcMem( pBase+NTHeader.IMPORTDIR.VirtualAddress, ip.pI+1, import_size );
  WriteProcMem( pMem, pImports, len );
  HeapFree( hHeap, 0, pImports );

  if (NTHeader.DATADIRS > IMAGE_DIRECTORY_ENTRY_IAT &&
      NTHeader.IATDIR.VirtualAddress == 0)
    NTHeader.IATDIR = NTHeader.IMPORTDIR;
  NTHeader.IMPORTDIR.VirtualAddress = rva + 8 + ansi_len;
  //NTHeader.IMPORTDIR.Size += sizeof(*pImports);
  if (NTHeader.DATADIRS > IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT)
  {
    NTHeader.BOUNDDIR.VirtualAddress = 0;
    //NTHeader.BOUNDDIR.Size = 0;
  }
  VirtProtVar( pNTHeader, PAGE_READWRITE );
  WriteProcVar( pNTHeader, &NTHeader );
  VirtProtVar( pNTHeader, pr );

  if (NTHeader.DATADIRS > IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR &&
      NTHeader.COMDIR.VirtualAddress != 0)
  {
    pComHeader = (PIMAGE_COR20_HEADER)(pBase + NTHeader.COMDIR.VirtualAddress);
    ReadProcVar( pComHeader, &ComHeader );
    if (ComHeader.Flags & COMIMAGE_FLAGS_ILONLY)
    {
      ComHeader.Flags &= ~COMIMAGE_FLAGS_ILONLY;
      VirtProtVar( pComHeader, PAGE_READWRITE );
      WriteProcVar( pComHeader, &ComHeader );
      VirtProtVar( pComHeader, pr );
    }
  }
}


/*
  Locate the base address of 64-bit ntdll.dll.	This is supposedly really at
  the same address for every process, but let's find it anyway.  A newly-
  created suspended 64-bit process has two images in memory: the process itself
  and ntdll.dll - the one that is a DLL must be ntdll.dll.  (A 32-bit WOW64
  process has three images - the process and both 64- & 32-bit ntdll.dll).
*/
static PBYTE get_ntdll( LPPROCESS_INFORMATION ppi )
{
  PBYTE  ptr;
  MEMORY_BASIC_INFORMATION minfo;
  IMAGE_DOS_HEADER dos_header;
  IMAGE_NT_HEADERS nt_header;

  for (ptr = NULL;
       VirtualQueryEx( ppi->hProcess, ptr, &minfo, sizeof(minfo) );
       ptr += minfo.RegionSize)
  {
    if (minfo.BaseAddress == minfo.AllocationBase
	&& ReadProcVar( minfo.BaseAddress, &dos_header )
	&& dos_header.e_magic == IMAGE_DOS_SIGNATURE
	&& ReadProcVar( (PBYTE)minfo.BaseAddress + dos_header.e_lfanew,
			&nt_header )
	&& nt_header.Signature == IMAGE_NT_SIGNATURE
	&& (nt_header.FileHeader.Characteristics & IMAGE_FILE_DLL))
    {
      return minfo.BaseAddress;
    }
  }

  DEBUGSTR( 1, "  Failed to find ntdll.dll!" );
  return NULL;
}


void InjectDLL64( LPPROCESS_INFORMATION ppi )
{
  PBYTE  ntdll;
  DWORD  rLdrLoadDll;
  PBYTE  pMem;
  DWORD  len;
  HANDLE thread;
  BYTE	 code[64];
  union
  {
    PBYTE   pB;
    PUSHORT pS;
    PDWORD  pD;
    PBYTE*  pL;
  } ip;

  ntdll = get_ntdll( ppi );
  if (ntdll == NULL)
    return;

  rLdrLoadDll = GetProcRVA( L"ntdll.dll", "LdrLoadDll", 64 );
  if (rLdrLoadDll == 0)
    return;

  pMem = VirtualAllocEx( ppi->hProcess, NULL, 4096, MEM_COMMIT,
			 PAGE_EXECUTE_READ );
  if (pMem == NULL)
  {
    DEBUGSTR(1, "  Failed to allocate virtual memory (%u)", GetLastError());
    return;
  }

  len = (DWORD)TSIZE(wcslen( DllName ) + 1);
  ip.pB = code;

  *ip.pL++ = ntdll + rLdrLoadDll;	// address of LdrLoadDll
  *ip.pD++ = 0x38ec8348;		// sub	 rsp, 0x38
  *ip.pD++ = 0x244c8d4c;		// lea	 r9, [rsp+0x20]
  *ip.pD++ = 0x058d4c20;		// lea	 r8, L"path\to\ANSI64.dll"
  *ip.pD++ = 16;			// xor	 edx, edx
  *ip.pD++ = 0xc933d233;		// xor	 ecx, ecx
  *ip.pS++ = 0x15ff;			// call  LdrLoadDll
  *ip.pD++ = -34;			// add	 rsp, 0x38
  *ip.pD++ = 0x38c48348;		// ret
  *ip.pS++ = 0x00c3;			// alignment for the name
  *ip.pS++ = (USHORT)(len - TSIZE(1));	// UNICODE_STRING.Length
  *ip.pS++ = (USHORT)len;		// UNICODE_STRING.MaximumLength
  *ip.pD++ = 0; 			// padding
  *ip.pL++ = pMem + 56; 		// UNICODE_STRING.Buffer
  WriteProcMem( pMem, code, ip.pB - code );
  WriteProcMem( pMem + (ip.pB - code), DllName, len );
  thread = CreateRemoteThread( ppi->hProcess, NULL, 4096,
			   (LPTHREAD_START_ROUTINE)(pMem + 8), NULL, 0, NULL );
  WaitForSingleObject( thread, INFINITE );
  CloseHandle( thread );
  VirtualFreeEx( ppi->hProcess, pMem, 0, MEM_RELEASE );
}
#endif
