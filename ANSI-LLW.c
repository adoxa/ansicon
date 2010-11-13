/*
  ANSI-LLW.c - Output the 32-bit address of LoadLibraryW.

  Jason Hood, 13 November, 2010 (LLA version 5 September, 2010).

  I don't know of a method to retrieve the 32-bit address of a function in
  64-bit code, so this is a simple workaround.
*/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

int main( void )
{
  return (DWORD)LoadLibraryW;
}
