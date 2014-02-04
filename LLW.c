/*
  Locate the relative address of LoadLibraryW in kernel32.dll.	This is needed
  to get the 32-bit address from 64-bit code, and it eliminates the possibility
  of it already being hooked.
*/

#include "ansicon.h"

static PIMAGE_DOS_HEADER pDosHeader;

#define MakeVA( cast, offset ) (cast)((DWORD_PTR)pDosHeader + (DWORD)(offset))


static int export_cmp( const void* a, const void* b )
{
  return strcmp( (LPCSTR)a, MakeVA( LPCSTR, *(const PDWORD)b ) );
}


DWORD get_LLW32r( void )
{
  HMODULE kernel32;
  TCHAR   buf[MAX_PATH];
  UINT	  len;
  PIMAGE_NT_HEADERS32	  pNTHeader;
  PIMAGE_EXPORT_DIRECTORY pExportDir;
  PDWORD  fun_table, name_table;
  PWORD   ord_table;
  PDWORD  pLLW;
  DWORD   LLWr;

#ifdef _WIN64
  len = GetSystemWow64Directory( buf, MAX_PATH );
#else
  len = GetSystemDirectory( buf, MAX_PATH );
#endif
  wcscpy( buf + len, L"\\kernel32.dll" );
  kernel32 = LoadLibraryEx( buf, NULL, LOAD_LIBRARY_AS_IMAGE_RESOURCE );
  if (kernel32 == NULL)
  {
    DEBUGSTR( 1, L"Unable to load 32-bit kernel32.dll!" );
    return 0;
  }
  // The handle uses low bits as flags, so strip 'em off.
  pDosHeader = (PIMAGE_DOS_HEADER)((DWORD_PTR)kernel32 & ~0xFFFF);
  pNTHeader  = MakeVA( PIMAGE_NT_HEADERS32, pDosHeader->e_lfanew );
  pExportDir = MakeVA( PIMAGE_EXPORT_DIRECTORY,
		       pNTHeader->EXPORTDIR.VirtualAddress );

  fun_table  = MakeVA( PDWORD, pExportDir->AddressOfFunctions );
  name_table = MakeVA( PDWORD, pExportDir->AddressOfNames );
  ord_table  = MakeVA( PWORD,  pExportDir->AddressOfNameOrdinals );

  pLLW = bsearch( "LoadLibraryW", name_table, pExportDir->NumberOfNames,
		  sizeof(DWORD), export_cmp );
  if (pLLW == NULL)
  {
    DEBUGSTR( 1, L"Could not find 32-bit LoadLibraryW!" );
    LLWr = 0;
  }
  else
  {
    LLWr = fun_table[ord_table[pLLW - name_table]];
  }
  FreeLibrary( kernel32 );
  return LLWr;
}


#ifdef _WIN64
DWORD64 get_LLW64r( void )
{
  HMODULE kernel32;
  TCHAR   buf[MAX_PATH];
  UINT	  len;
  PIMAGE_NT_HEADERS	  pNTHeader;
  PIMAGE_EXPORT_DIRECTORY pExportDir;
  PDWORD  fun_table, name_table;
  PWORD   ord_table;
  PDWORD  pLLW;
  DWORD64 LLWr;

  len = GetSystemDirectory( buf, MAX_PATH );
  wcscpy( buf + len, L"\\kernel32.dll" );
  kernel32 = LoadLibraryEx( buf, NULL, LOAD_LIBRARY_AS_IMAGE_RESOURCE );
  if (kernel32 == NULL)
  {
    DEBUGSTR( 1, L"Unable to load 64-bit kernel32.dll!" );
    return 0;
  }
  // The handle uses low bits as flags, so strip 'em off.
  pDosHeader = (PIMAGE_DOS_HEADER)((DWORD_PTR)kernel32 & ~0xFFFF);
  pNTHeader  = MakeVA( PIMAGE_NT_HEADERS, pDosHeader->e_lfanew );
  pExportDir = MakeVA( PIMAGE_EXPORT_DIRECTORY,
		       pNTHeader->EXPORTDIR.VirtualAddress );

  fun_table  = MakeVA( PDWORD, pExportDir->AddressOfFunctions );
  name_table = MakeVA( PDWORD, pExportDir->AddressOfNames );
  ord_table  = MakeVA( PWORD,  pExportDir->AddressOfNameOrdinals );

  pLLW = bsearch( "LoadLibraryW", name_table, pExportDir->NumberOfNames,
		  sizeof(DWORD), export_cmp );
  if (pLLW == NULL)
  {
    DEBUGSTR( 1, L"Could not find 64-bit LoadLibraryW!" );
    LLWr = 0;
  }
  else
  {
    LLWr = fun_table[ord_table[pLLW - name_table]];
  }
  FreeLibrary( kernel32 );
  return LLWr;
}
#endif
