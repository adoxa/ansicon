/*
  Inject the DLL into the target process by modifying its import descriptor
  table.  The target process must have been created suspended.
*/

#include "ansicon.h"


// Search for a suitable free area after the main image.  (32-bit code could
// really go anywhere, but let's keep it relatively local.)
PVOID FindMem( HANDLE hProcess, PBYTE base, DWORD len )
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


void InjectDLL( LPPROCESS_INFORMATION ppi, PBYTE pBase )
{
  DWORD rva;
  PVOID pMem;
  DWORD len;
  DWORD pr;
  IMAGE_DOS_HEADER	   DosHeader;
  IMAGE_NT_HEADERS	   NTHeader, *pNTHeader;
  PIMAGE_IMPORT_DESCRIPTOR pImports, pANSI_ImportDesc;
  IMAGE_COR20_HEADER	   ComHeader, *pComHeader;
  union
  {
    PBYTE     pB;
    PLONG_PTR pL;
  } ip;

  ReadProcVar( pBase, &DosHeader );
  pNTHeader = (PIMAGE_NT_HEADERS)(pBase + DosHeader.e_lfanew);
  ReadProcVar( pNTHeader, &NTHeader );

  len = 4 * PTRSZ + ansi_len
	+ sizeof(*pANSI_ImportDesc) + NTHeader.IMPORTDIR.Size;
  pImports = malloc( len );
  if (pImports == NULL)
  {
    DEBUGSTR( 1, L"  Failed to allocate memory." );
    return;
  }
  pMem = FindMem( ppi->hProcess, pBase, len );
  if (pMem == NULL)
  {
    DEBUGSTR( 1, L"  Failed to allocate virtual memory." );
    free( pImports );
    return;
  }
  rva = (DWORD)((PBYTE)pMem - pBase);

  ip.pL = (PLONG_PTR)pImports;
  *ip.pL++ = IMAGE_ORDINAL_FLAG + 1;
  *ip.pL++ = 0;
  *ip.pL++ = IMAGE_ORDINAL_FLAG + 1;
  *ip.pL++ = 0;
  memcpy( ip.pB, ansi_dll, ansi_len );
  ip.pB += ansi_len;
  pANSI_ImportDesc = (PIMAGE_IMPORT_DESCRIPTOR)ip.pB;
  pANSI_ImportDesc->OriginalFirstThunk = rva + 2 * PTRSZ;
  pANSI_ImportDesc->TimeDateStamp = 0;
  pANSI_ImportDesc->ForwarderChain = 0;
  pANSI_ImportDesc->Name = rva + 4 * PTRSZ;
  pANSI_ImportDesc->FirstThunk = rva;
  ReadProcMem( pBase + NTHeader.IMPORTDIR.VirtualAddress,
	       pANSI_ImportDesc + 1, NTHeader.IMPORTDIR.Size );
  WriteProcMem( pMem, pImports, len );
  free( pImports );

  NTHeader.IMPORTDIR.VirtualAddress = rva + 4 * PTRSZ + ansi_len;
  NTHeader.IMPORTDIR.Size += sizeof(*pANSI_ImportDesc);

  // If there's no IAT, copy the IDT.
  if (NTHeader.IATDIR.VirtualAddress == 0)
    NTHeader.IATDIR = NTHeader.IMPORTDIR;

  // Remove bound imports, so the updated import table is used.
  NTHeader.BOUNDDIR.VirtualAddress = 0;
  NTHeader.BOUNDDIR.Size = 0;

  VirtProtVar( pNTHeader, PAGE_READWRITE );
  WriteProcVar( pNTHeader, &NTHeader );
  VirtProtVar( pNTHeader, pr );

  // Remove the IL-only flag on a managed process.
  if (NTHeader.COMDIR.VirtualAddress != 0 && NTHeader.COMDIR.Size != 0)
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
  DWORD len;
  DWORD pr;
  IMAGE_DOS_HEADER	   DosHeader;
  IMAGE_NT_HEADERS32	   NTHeader, *pNTHeader;
  PIMAGE_IMPORT_DESCRIPTOR pImports, pANSI_ImportDesc;
  IMAGE_COR20_HEADER	   ComHeader, *pComHeader;
  union
  {
    PBYTE pB;
    PLONG pL;
  } ip;

  ReadProcVar( pBase, &DosHeader );
  pNTHeader = (PIMAGE_NT_HEADERS32)(pBase + DosHeader.e_lfanew);
  ReadProcVar( pNTHeader, &NTHeader );

  len = 16 + ansi_len + sizeof(*pANSI_ImportDesc) + NTHeader.IMPORTDIR.Size;
  pImports = malloc( len );
  if (pImports == NULL)
  {
    DEBUGSTR( 1, L"  Failed to allocate memory." );
    return;
  }
  pMem = FindMem( ppi->hProcess, pBase, len );
  if (pMem == NULL)
  {
    DEBUGSTR( 1, L"  Failed to allocate virtual memory." );
    free( pImports );
    return;
  }
  rva = (DWORD)((PBYTE)pMem - pBase);

  ip.pL = (PLONG)pImports;
  *ip.pL++ = IMAGE_ORDINAL_FLAG32 + 1;
  *ip.pL++ = 0;
  *ip.pL++ = IMAGE_ORDINAL_FLAG32 + 1;
  *ip.pL++ = 0;
  memcpy( ip.pB, ansi_dll, ansi_len );
  ip.pB += ansi_len;
  pANSI_ImportDesc = (PIMAGE_IMPORT_DESCRIPTOR)ip.pB;
  pANSI_ImportDesc->OriginalFirstThunk = rva + 8;
  pANSI_ImportDesc->TimeDateStamp = 0;
  pANSI_ImportDesc->ForwarderChain = 0;
  pANSI_ImportDesc->Name = rva + 16;
  pANSI_ImportDesc->FirstThunk = rva;
  ReadProcMem( pBase + NTHeader.IMPORTDIR.VirtualAddress,
	       pANSI_ImportDesc + 1, NTHeader.IMPORTDIR.Size );
  WriteProcMem( pMem, pImports, len );
  free( pImports );

  NTHeader.IMPORTDIR.VirtualAddress = rva + 16 + ansi_len;
  NTHeader.IMPORTDIR.Size += sizeof(*pANSI_ImportDesc);
  if (NTHeader.IATDIR.VirtualAddress == 0)
    NTHeader.IATDIR = NTHeader.IMPORTDIR;
  NTHeader.BOUNDDIR.VirtualAddress = 0;
  NTHeader.BOUNDDIR.Size = 0;
  VirtProtVar( pNTHeader, PAGE_READWRITE );
  WriteProcVar( pNTHeader, &NTHeader );
  VirtProtVar( pNTHeader, pr );

  if (NTHeader.COMDIR.VirtualAddress != 0 && NTHeader.COMDIR.Size != 0)
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
