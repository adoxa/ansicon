/*
  Test for a valid process.  This may sometimes detect GUI, even for a console
  process.  I think this is due to a DLL being loaded in the address space
  before the main image.  Ideally I could just use the base address directly,
  but that doesn't seem easy to do for another process - there doesn't seem to
  be a GetModuleHandle for another process.  The CreateRemoteThread trick won't
  work with 64-bit (exit code is DWORD) and setting it up to make it work
  hardly seems worth it.  There's GetModuleInformation, but passing in NULL just
  returns a base of NULL, so that's no help.  Since 64/32 is sufficient, let
  ansicon.exe handle the difference between console/GUI.

  Update: ignore images characterised as DLL.
*/

#include "ansicon.h"


int ProcessType( LPPROCESS_INFORMATION pinfo, BOOL* gui )
{
  char* ptr;
  MEMORY_BASIC_INFORMATION minfo;
  IMAGE_DOS_HEADER dos_header;
  IMAGE_NT_HEADERS nt_header;
  SIZE_T read;

  *gui = FALSE;
  for (ptr = NULL;
       VirtualQueryEx( pinfo->hProcess, ptr, &minfo, sizeof(minfo) );
       ptr += minfo.RegionSize)
  {
    if (minfo.BaseAddress == minfo.AllocationBase &&
	ReadProcessMemory( pinfo->hProcess, minfo.AllocationBase,
			   &dos_header, sizeof(dos_header), &read ))
    {
      if (dos_header.e_magic == IMAGE_DOS_SIGNATURE)
      {
	if (ReadProcessMemory( pinfo->hProcess, (char*)minfo.AllocationBase +
			       dos_header.e_lfanew, &nt_header,
			       sizeof(nt_header), &read ))
	{
	  if (nt_header.Signature == IMAGE_NT_SIGNATURE &&
	      (nt_header.FileHeader.Characteristics &
			 (IMAGE_FILE_EXECUTABLE_IMAGE | IMAGE_FILE_DLL))
			 == IMAGE_FILE_EXECUTABLE_IMAGE)
	  {
	    *gui = (nt_header.OptionalHeader.Subsystem
			      == IMAGE_SUBSYSTEM_WINDOWS_GUI);
	    if (nt_header.OptionalHeader.Subsystem ==
		IMAGE_SUBSYSTEM_WINDOWS_CUI || *gui)
	    {
	      if (nt_header.FileHeader.Machine == IMAGE_FILE_MACHINE_I386)
	      {
		// Microsoft ignores precision on %p.
		DEBUGSTR( 1, L"  32-bit %s (base = %.8X)",
			  (*gui) ? L"GUI" : L"console",
			  (DWORD)(DWORD_PTR)minfo.AllocationBase );
		return 32;
	      }
#ifdef _WIN64
	      if (nt_header.FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64)
	      {
		DEBUGSTR( 1, L"  64-bit %s (base = %p)",
			  (*gui) ? L"GUI" : L"console", minfo.AllocationBase );
		return 64;
	      }
#elif defined(W32ON64)
	      if (nt_header.FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64)
	      {
		DEBUGSTR( 1, L"  64-bit %s",
			  (*gui) ? L"GUI" : L"console" );
		return 64;
	      }
#endif
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
	}
      }
    }
#ifdef _WIN32
    // If a 32-bit process manages to load a 64-bit one, we may miss the base
    // address.  If the pointer overflows, assume 64-bit and abort.
    if (ptr > ptr + minfo.RegionSize)
    {
#ifdef W32ON64
      DEBUGSTR( 1, L"  Pointer overflow: assuming 64-bit console" );
      return 64;
#else
      DEBUGSTR( 1, L"  Ignoring apparent 64-bit process" );
      return 0;
#endif
    }
#endif
  }

  DEBUGSTR( 1, L"  Ignoring non-Windows process" );
  return 0;
}
