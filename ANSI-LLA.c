/*
  ANSI-LLA.c - Output the 32-bit address of LoadLibraryA.

  Jason Hood, 5 September, 2010.

  I don't know of a method to retrieve the 32-bit address of a function in
  64-bit code, so this is a simple workaround.
*/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

int main( void )
{
  return (DWORD)GetProcAddress( GetModuleHandleA( "kernel32.dll" ),
						  "LoadLibraryA" );
}
