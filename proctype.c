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
*/

#include "ansicon.h"


int ProcessType( LPPROCESS_INFORMATION pinfo )
{
  MEMORY_BASIC_INFORMATION minfo;
  char* ptr = 0;

  while (VirtualQueryEx( pinfo->hProcess, ptr, &minfo, sizeof(minfo) ))
  {
    IMAGE_DOS_HEADER dos_header;
    SIZE_T read;
    if (minfo.BaseAddress == minfo.AllocationBase &&
	ReadProcessMemory( pinfo->hProcess, minfo.AllocationBase,
			   &dos_header, sizeof(dos_header), &read ))
    {
      if (dos_header.e_magic == IMAGE_DOS_SIGNATURE)
      {
	IMAGE_NT_HEADERS nt_header;
	if (ReadProcessMemory( pinfo->hProcess, (char*)minfo.AllocationBase +
			       dos_header.e_lfanew, &nt_header,
			       sizeof(nt_header), &read ))
	{
	  if (nt_header.Signature == IMAGE_NT_SIGNATURE)
	  {
	    BOOL gui = (nt_header.OptionalHeader.Subsystem == IMAGE_SUBSYSTEM_WINDOWS_GUI);
	    if (nt_header.OptionalHeader.Subsystem == IMAGE_SUBSYSTEM_WINDOWS_CUI ||
		gui )
	    {
	      if (nt_header.FileHeader.Machine == IMAGE_FILE_MACHINE_I386)
	      {
		DEBUGSTR( L"  %p: 32-bit %s",
			  minfo.AllocationBase, (gui) ? L"GUI" : L"console" );
		return 32;
	      }
#ifdef _WIN64
	      if (nt_header.FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64)
	      {
		DEBUGSTR( L"  %p: 64-bit %s",
			  minfo.AllocationBase, (gui) ? L"GUI" : L"console" );
		return 64;
	      }
#endif
	      DEBUGSTR( L"  Ignoring unsupported machine (0x%X)",
			nt_header.FileHeader.Machine );
	    }
	    else
	    {
	      DEBUGSTR( L"  Ignoring non-Windows subsystem (%u)",
			nt_header.OptionalHeader.Subsystem );
	    }
	  }
	}
	return 0;
      }
    }
    ptr += minfo.RegionSize;
  }

  DEBUGSTR( L"  Ignoring non-Windows process" );
  return 0;
}
