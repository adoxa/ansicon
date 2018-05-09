/*
  Test for a valid process (i386 for x86; that or AMD64 for x64).  We can get
  that info from the image header, which means getting the process's base
  address (which we need anyway, to modify the imports).  The simplest way to
  do that is to enumerate the pages, looking for an executable image.  A .NET
  AnyCPU process has a 32-bit structure, but will load as 64-bit when possible.
  The 64-bit version (both DLLs) will say this is type 48 (halfway between 32 &
  64); the 32-bit version will ignore it if run on a 64-bit OS.
*/

#include "ansicon.h"


#if !defined(_WIN64) && !defined(W32ON64)
static BOOL ProcessIs64( HANDLE hProcess )
{
  BOOL wow;

  typedef BOOL (WINAPI *LPFN_ISWOW64PROCESS)( HANDLE, PBOOL );
  static LPFN_ISWOW64PROCESS fnIsWow64Process;

  if (fnIsWow64Process == INVALID_HANDLE_VALUE)
    return FALSE;

  if (fnIsWow64Process == NULL)
  {
    fnIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress(
			GetModuleHandle( L"kernel32.dll" ), "IsWow64Process" );
    if (fnIsWow64Process == NULL)
    {
      fnIsWow64Process = INVALID_HANDLE_VALUE;
      return FALSE;
    }
  }

  // If IsWow64Process fails, say it is 64, since injection probably wouldn't
  // work, either.
  return !(fnIsWow64Process( hProcess, &wow ) && wow);
}
#endif


int ProcessType( LPPROCESS_INFORMATION ppi, PBYTE* pBase, BOOL* gui )
{
  PBYTE ptr;
  MEMORY_BASIC_INFORMATION minfo;
  IMAGE_DOS_HEADER dos_header;
  IMAGE_NT_HEADERS nt_header;
  PBYTE dummy_base;
  BOOL	dummy_gui;
  int	log;

  // Don't log if we're only getting one value, as it's already been logged.
  log = 1;
  if (pBase == NULL)
  {
    pBase = &dummy_base;
    log = 128;
  }
  if (gui == NULL)
  {
    gui = &dummy_gui;
    log = 128;
  }

  *pBase = NULL;
  *gui = FALSE;

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
	&& !(nt_header.FileHeader.Characteristics & IMAGE_FILE_DLL))
    {
      // Don't load into ansicon.exe, it's already imported.
      if (nt_header.OptionalHeader.MajorImageVersion == 20033 &&    // 'AN'
	  nt_header.OptionalHeader.MinorImageVersion == 18771)	    // 'SI'
	return -1;

      *pBase = minfo.BaseAddress;
      if (nt_header.OptionalHeader.Subsystem == IMAGE_SUBSYSTEM_WINDOWS_GUI)
	*gui = TRUE;
      if (*gui ||
	  nt_header.OptionalHeader.Subsystem == IMAGE_SUBSYSTEM_WINDOWS_CUI)
      {
	if (nt_header.FileHeader.Machine == IMAGE_FILE_MACHINE_I386)
	{
	  PIMAGE_NT_HEADERS32 pNTHeader = (PIMAGE_NT_HEADERS32)&nt_header;
	  if (pNTHeader->DATADIRS > IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR &&
	      pNTHeader->COMDIR.VirtualAddress != 0)
	  {
	    IMAGE_COR20_HEADER ComHeader, *pComHeader;
	    pComHeader = (PIMAGE_COR20_HEADER)((PBYTE)minfo.BaseAddress
				      + pNTHeader->COMDIR.VirtualAddress);
	    ReadProcVar( pComHeader, &ComHeader );
	    if ((ComHeader.Flags & COMIMAGE_FLAGS_ILONLY) &&
		!(ComHeader.Flags & COMIMAGE_FLAGS_32BITREQUIRED))
	    {
	      DEBUGSTR( log, "  AnyCPU %s (base = %q)",
			     (*gui) ? "GUI" : "console", minfo.BaseAddress );
#if defined(_WIN64) || defined(W32ON64)
	      return 48;
#else
	      if (ProcessIs64( ppi->hProcess ))
	      {
		DEBUGSTR( log, "  Unsupported (use x64\\ansicon)" );
		return 0;
	      }
	      return 32;
#endif
	    }
	  }
	  DEBUGSTR( log, "  32-bit %s (base = %q)",
			 (*gui) ? "GUI" : "console", minfo.BaseAddress );
	  return 32;
	}
	if (nt_header.FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64)
	{
#ifdef _WIN64
	  DEBUGSTR( log, "  64-bit %s (base = %p)",
			 (*gui) ? "GUI" : "console", minfo.BaseAddress );
	  return 64;
#else
	  DEBUGSTR( log, "  64-bit %s (base = %P)",
			 (*gui) ? "GUI" : "console", minfo.BaseAddress );
#if defined(W32ON64)
	  return 64;
#else
	  DEBUGSTR( log, "  Unsupported (use x64\\ansicon)" );
	  return 0;
#endif
#endif
	}
	DEBUGSTR( log, "  Ignoring unsupported machine (0x%X)",
		       nt_header.FileHeader.Machine );
      }
      else
      {
	DEBUGSTR( log, "  Ignoring unsupported subsystem (%u)",
		       nt_header.OptionalHeader.Subsystem );
      }
      return 0;
    }
#ifndef _WIN64
    // If a 32-bit process loads a 64-bit one, we may miss the base
    // address.  If the pointer overflows, assume 64-bit.
    if (((DWORD)ptr >> 12) + ((DWORD)minfo.RegionSize >> 12) >= 0x100000)
    {
#ifdef W32ON64
      DEBUGSTR( log, "  Pointer overflow: assuming 64-bit" );
      return 64;
#else
      DEBUGSTR( log, "  Ignoring apparent 64-bit process (use x64\\ansicon)" );
      return 0;
#endif
    }
#endif
  }

  DEBUGSTR( log, "  Ignoring non-Windows process" );
  return 0;
}
