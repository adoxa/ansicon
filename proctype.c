/*
  Test for a valid process (i386 for x86; that or AMD64 for x64).  We can get
  that info from the image header, which means getting the process's base
  address (which we need anyway, to modify the imports).  The simplest way to
  do that is to enumerate the pages, looking for an executable image.
*/

#include "ansicon.h"


int ProcessType( LPPROCESS_INFORMATION ppi, PBYTE* pBase, BOOL* gui )
{
  PBYTE ptr;
  MEMORY_BASIC_INFORMATION minfo;
  IMAGE_DOS_HEADER dos_header;
  IMAGE_NT_HEADERS nt_header;

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
      // Don't load into ansicon.exe, it wants to do that itself.
      if (nt_header.OptionalHeader.MajorImageVersion == 20033 &&
	  nt_header.OptionalHeader.MinorImageVersion == 18771)
	return -1;

      *pBase = minfo.BaseAddress;
      if (nt_header.OptionalHeader.Subsystem == IMAGE_SUBSYSTEM_WINDOWS_GUI)
	*gui = TRUE;
      if (*gui ||
	  nt_header.OptionalHeader.Subsystem == IMAGE_SUBSYSTEM_WINDOWS_CUI)
      {
	if (nt_header.FileHeader.Machine == IMAGE_FILE_MACHINE_I386)
	{
	  DEBUGSTR( 1, L"  32-bit %s (base = %.8X)",
		    (*gui) ? L"GUI" : L"console",
		    PtrToUint( minfo.BaseAddress ) );
	  return 32;
	}
	if (nt_header.FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64)
	{
#ifdef _WIN64
	  DEBUGSTR( 1, L"  64-bit %s (base = %.8X_%.8X)",
		    (*gui) ? L"GUI" : L"console",
		    (DWORD)((DWORD_PTR)minfo.BaseAddress >> 32),
		    PtrToUint( minfo.BaseAddress ) );
	  return 64;
#elif defined(W32ON64)
	  // Console will log due to -P, but GUI may be ignored (if not,
	  // this'll show up twice).
	  if (*gui)
	    DEBUGSTR( 1, L"  64-bit GUI (base = 00000000_%.8X)",
		      PtrToUint( minfo.BaseAddress ) );
	  return 64;
#else
	  DEBUGSTR( 1, L"  64-bit %s (base = 00000000_%.8X)",
		    (*gui) ? L"GUI" : L"console",
		    PtrToUint( minfo.BaseAddress ) );
	  DEBUGSTR( 1, L"  Unsupported (use x64\\ansicon)" );
	  return 0;
#endif
	}
	DEBUGSTR( 1, L"  Ignoring unsupported machine (0x%X)",
		  nt_header.FileHeader.Machine );
      }
      else
      {
	DEBUGSTR( 1, L"  Ignoring unsupported subsystem (%u)",
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
      DEBUGSTR( 1, L"  Pointer overflow: assuming 64-bit" );
      return 64;
#else
      DEBUGSTR( 1, L"  Ignoring apparent 64-bit process (use x64\\ansicon)" );
      return 0;
#endif
    }
#endif
  }

  DEBUGSTR( 1, L"  Ignoring non-Windows process" );
  return 0;
}
