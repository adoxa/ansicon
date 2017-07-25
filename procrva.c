/*
  Get the RVA of a function directly from a module.  This allows 64-bit code to
  work with 32-bit DLLs, and eliminates (or at least reduces) the possibility
  of the function already being hooked.
*/

#include "ansicon.h"

static PIMAGE_DOS_HEADER pDosHeader;


static int export_cmp( const void* a, const void* b )
{
  return strcmp( (LPCSTR)a, MakeVA( LPCSTR, *(const PDWORD)b ) );
}


#ifdef _WIN64
DWORD GetProcRVA( LPCTSTR module, LPCSTR func, int bits )
#else
DWORD GetProcRVA( LPCTSTR module, LPCSTR func )
#endif
{
  HMODULE hMod;
  TCHAR   buf[MAX_PATH];
  UINT	  len;
  PIMAGE_NT_HEADERS	  pNTHeader;
  PIMAGE_EXPORT_DIRECTORY pExportDir;
  PDWORD  fun_table, name_table;
  PWORD   ord_table;
  PDWORD  pFunc;
  DWORD   rva;

#ifdef _WIN64
  if (bits == 32)
    len = GetSystemWow64Directory( buf, MAX_PATH );
  else
#endif
  len = GetSystemDirectory( buf, MAX_PATH );
  buf[len++] = '\\';
  wcscpy( buf + len, module );
  hMod = LoadLibraryEx( buf, NULL, LOAD_LIBRARY_AS_IMAGE_RESOURCE );
  if (hMod == NULL)
  {
#ifdef _WIN64
    DEBUGSTR( 1, "Unable to load %u-bit %S (%u)!",
		 bits, module, GetLastError() );
#else
    DEBUGSTR( 1, "Unable to load %S (%u)!", module, GetLastError() );
#endif
    return 0;
  }
  // The handle uses low bits as flags, so strip 'em off.
  pDosHeader = (PIMAGE_DOS_HEADER)((DWORD_PTR)hMod & ~0xFFFF);
  pNTHeader  = MakeVA( PIMAGE_NT_HEADERS, pDosHeader->e_lfanew );
#ifdef _WIN64
  if (bits == 32)
    pExportDir	= MakeVA( PIMAGE_EXPORT_DIRECTORY,
		  ((PIMAGE_NT_HEADERS32)pNTHeader)->EXPORTDIR.VirtualAddress );
  else
#endif
  pExportDir = MakeVA( PIMAGE_EXPORT_DIRECTORY,
		       pNTHeader->EXPORTDIR.VirtualAddress );
  fun_table  = MakeVA( PDWORD, pExportDir->AddressOfFunctions );
  name_table = MakeVA( PDWORD, pExportDir->AddressOfNames );
  ord_table  = MakeVA( PWORD,  pExportDir->AddressOfNameOrdinals );

  pFunc = bsearch( func, name_table, pExportDir->NumberOfNames,
		   sizeof(DWORD), export_cmp );
  if (pFunc == NULL)
  {
#ifdef _WIN64
    DEBUGSTR( 1, "Could not find %u-bit %s!", bits, func );
#else
    DEBUGSTR( 1, "Could not find %s!", func );
#endif
    rva = 0;
  }
  else
  {
    rva = fun_table[ord_table[pFunc - name_table]];
  }
  FreeLibrary( hMod );
  return rva;
}
