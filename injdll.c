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
      if ((PBYTE)minfo.BaseAddress - base > 0xFFFFffff - len)
	return NULL;
#endif
      // Round up to the allocation granularity, presumed to be 64Ki.
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

  // Windows 8 and later require the IDT to be part of a section when there's
  // no IAT.  This means we can't move the imports, so remote load instead.
  if (NTHeader.DATADIRS <= IMAGE_DIRECTORY_ENTRY_IAT &&
      get_os_version() >= 0x602)
  {
#ifdef _WIN64
    RemoteLoad64( ppi );
#else
    RemoteLoad32( ppi );
#endif
    return;
  }

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
  RtlMoveMemory( ip.pB, ansi_dll, ansi_len );
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

  if (NTHeader.DATADIRS <= IMAGE_DIRECTORY_ENTRY_IAT &&
      get_os_version() >= 0x602)
  {
    RemoteLoad32( ppi );
    return;
  }

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
  RtlMoveMemory( ip.pB, ansi_dll, ansi_len );
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
#endif


/*
  Locate the base address of ntdll.dll.  This is supposedly really at the same
  address for every process, but let's find it anyway.  A newly-created
  suspended process has two images in memory: the process itself and ntdll.dll.
  Thus the one that is a DLL must be ntdll.dll.  However, a WOW64 process also
  has the 64-bit version, so test the machine.
*/
#ifdef _WIN64
static PBYTE get_ntdll( LPPROCESS_INFORMATION ppi, WORD machine )
#else
static PBYTE get_ntdll( LPPROCESS_INFORMATION ppi )
#endif
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
	&& (nt_header.FileHeader.Characteristics & IMAGE_FILE_DLL)
#ifdef _WIN64
	&& nt_header.FileHeader.Machine == machine
#endif
       )
    {
      return minfo.BaseAddress;
    }
  }

  DEBUGSTR( 1, "  Failed to find ntdll.dll!" );
  return NULL;
}


#ifdef _WIN64
void RemoteLoad64( LPPROCESS_INFORMATION ppi )
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

  ntdll = get_ntdll( ppi, IMAGE_FILE_MACHINE_AMD64 );
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

  len = (DWORD)TSIZE(lstrlen( DllName ) + 1);
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
  *(PDWORD)DllNameType = 0x340036/*L'46'*/;
  WriteProcMem( pMem + (ip.pB - code), DllName, len );
  thread = CreateRemoteThread( ppi->hProcess, NULL, 4096,
			   (LPTHREAD_START_ROUTINE)(pMem + 8), NULL, 0, NULL );
  WaitForSingleObject( thread, INFINITE );
  CloseHandle( thread );
  VirtualFreeEx( ppi->hProcess, pMem, 0, MEM_RELEASE );
}
#endif


void RemoteLoad32( LPPROCESS_INFORMATION ppi )
{
  PBYTE  ntdll;
  DWORD  rLdrLoadDll;
  PBYTE  pMem;
  DWORD  bMem;
  DWORD  len;
  HANDLE thread;
  BYTE	 code[64];
  union
  {
    PBYTE   pB;
    PUSHORT pS;
    PDWORD  pD;
  } ip;

#ifdef _WIN64
  ntdll = get_ntdll( ppi, IMAGE_FILE_MACHINE_I386 );
#else
  ntdll = get_ntdll( ppi );
#endif
  if (ntdll == NULL)
    return;

#ifdef _WIN64
  rLdrLoadDll = GetProcRVA( L"ntdll.dll", "LdrLoadDll", 32 );
#else
  rLdrLoadDll = GetProcRVA( L"ntdll.dll", "LdrLoadDll" );
#endif
  if (rLdrLoadDll == 0)
    return;

  pMem = VirtualAllocEx( ppi->hProcess, NULL, 4096, MEM_COMMIT,
			 PAGE_EXECUTE_READ );
  if (pMem == NULL)
  {
    DEBUGSTR(1, "  Failed to allocate virtual memory (%u)", GetLastError());
    return;
  }
  bMem = PtrToUint( pMem );

  len = (DWORD)TSIZE(lstrlen( DllName ) + 1);
  ip.pB = code;

  *ip.pS++ = 0x5451;			// push  ecx esp
  *ip.pB++ = 0x68;			// push
  *ip.pD++ = bMem + 20; 		//	 L"path\to\ANSI32.dll"
  *ip.pD++ = 0x006A006A;		// push  0 0
  *ip.pB++ = 0xe8;			// call  LdrLoadDll
  *ip.pD++ = PtrToUint( ntdll ) + rLdrLoadDll - (bMem + 16);
  *ip.pD++ = 0xc359;			// pop	 ecx / ret and padding
  *ip.pS++ = (USHORT)(len - TSIZE(1));	// UNICODE_STRING.Length
  *ip.pS++ = (USHORT)len;		// UNICODE_STRING.MaximumLength
  *ip.pD++ = bMem + 28; 		// UNICODE_STRING.Buffer
  WriteProcMem( pMem, code, ip.pB - code );
#ifdef _WIN64
  *(PDWORD)DllNameType = 0x320033/*L'23'*/;
#endif
  WriteProcMem( pMem + (ip.pB - code), DllName, len );
  thread = CreateRemoteThread( ppi->hProcess, NULL, 4096,
			       (LPTHREAD_START_ROUTINE)pMem, NULL, 0, NULL );
  WaitForSingleObject( thread, INFINITE );
  CloseHandle( thread );
  VirtualFreeEx( ppi->hProcess, pMem, 0, MEM_RELEASE );
}
