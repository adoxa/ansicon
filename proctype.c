/*
  Test for a valid process.
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
    if (ReadProcessMemory( pinfo->hProcess, minfo.AllocationBase,
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
	    if (nt_header.OptionalHeader.Subsystem == IMAGE_SUBSYSTEM_WINDOWS_CUI ||
		nt_header.OptionalHeader.Subsystem == IMAGE_SUBSYSTEM_WINDOWS_GUI)
	    {
	      if (nt_header.FileHeader.Machine == IMAGE_FILE_MACHINE_I386)
		return 32;
#ifdef _WIN64
	      if (nt_header.FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64)
		return 64;
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
