/*
  ANSI-LLW.c - Output the 32-bit address of LoadLibraryW.

  Jason Hood, 13 November, 2010 (LLA version 5 September, 2010).

  I don't know of a method to retrieve the 32-bit address of a function in
  64-bit code, so this is a simple workaround.

  18 December, 2010: Initially I used GetProcAddress, but then I thought that
  was silly, why don't I just return LoadLibraryW directly?  That worked fine
  for TDM64 and VC, but MinGW32 would return the address of the jump to the
  function, not the function itself.  Not so silly after all.
*/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

int main( void )
{
  return (DWORD)GetProcAddress( GetModuleHandle( "kernel32.dll" ),
				"LoadLibraryW" );
}
