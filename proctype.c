/*
  Test for a valid process.
*/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>


int ProcessType( LPPROCESS_INFORMATION pinfo )
{
  MEMORY_BASIC_INFORMATION minfo;
  char* ptr = 0;
  int	type = 0;

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
	    if (nt_header.OptionalHeader.Subsystem ==
						   IMAGE_SUBSYSTEM_WINDOWS_CUI)
	    {
	      if (nt_header.FileHeader.Machine == IMAGE_FILE_MACHINE_I386)
	      {
		type = 32;
#ifdef _WIN64
	      }
	      else if (nt_header.FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64)
	      {
		type = 64;
#endif
	      }
	      else
	      {
		//DEBUGSTR( L"  Ignoring unsupported machine (%x)",
		//	  nt_header.FileHeader.Machine );
	      }
	    }
	    else
	    {
	      //DEBUGSTR( L"  Ignoring non-console subsystem (%u)",
	      //	nt_header.OptionalHeader.Subsystem );
	    }
	    break;
	  }
	}
      }
    }
    ptr += minfo.RegionSize;
  }

  return type;
}
