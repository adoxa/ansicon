/*
  util.c - Utility functions.
*/

#include "ansicon.h"
#include "version.h"


TCHAR	prog_path[MAX_PATH];
LPTSTR	prog;
int	log_level;
char	tempfile[MAX_PATH];
DWORD	pid;


// Get just the name of the program: "C:\path\program.exe" -> "program".
// Returns a pointer within program; it is modified to remove the extension.
LPTSTR get_program_name( LPTSTR program )
{
  LPTSTR name, ext;

  if (program == NULL)
  {
    GetModuleFileName( NULL, prog_path, lenof(prog_path) );
    program = prog_path;
  }
  name = wcsrchr( program, '\\' );
  if (name != NULL)
    ++name;
  else
    name = program;
  ext = wcsrchr( name, '.' );
  if (ext != NULL && ext != name)
    *ext = '\0';

  return name;
}


void DEBUGSTR( int level, LPTSTR szFormat, ... )
{
  TCHAR szBuffer[1024], szEscape[1024];
  va_list pArgList;
  HANDLE mutex;
  DWORD wait;
  FILE* file;

  if ((log_level & 3) < level && !(level & 4 & log_level))
    return;

  if (*tempfile == '\0')
  {
    _snprintf( tempfile, MAX_PATH, "%s\\ansicon.log", getenv( "TEMP" ) );
    pid = GetCurrentProcessId();
  }
  if (szFormat == NULL)
  {
    file = fopen( tempfile, (log_level & 8) ? "at" : "wt" );
    if (file != NULL)
    {
      SYSTEMTIME now;
      GetLocalTime( &now );
      fprintf( file, "ANSICON v" PVERSA " log (%d) started "
		      "%d-%.2d-%.2d %d:%.2d:%.2d\n",
		     log_level,
		     now.wYear, now.wMonth, now.wDay,
		     now.wHour, now.wMinute, now.wSecond );
      fclose( file );
    }
    return;
  }

  va_start( pArgList, szFormat );
  _vsnwprintf( szBuffer, lenof(szBuffer), szFormat, pArgList );
  va_end( pArgList );

  szFormat = szBuffer;
  if (*szFormat == '\33')
  {
    BOOL first = TRUE;
    LPTSTR pos = szEscape;
    while (*++szFormat != '\0' && pos < szEscape + lenof(szEscape) - 4)
    {
      if (*szFormat < 32)
      {
	*pos++ = '\\';
	switch (*szFormat)
	{
	  case '\a': *pos++ = 'a'; break;
	  case '\b': *pos++ = 'b'; break;
	  case '\t': *pos++ = 't'; break;
	  case '\r': *pos++ = 'r'; break;
	  case '\n': *pos++ = 'n'; break;
	  case	27 : *pos++ = 'e'; break;
	  default:
	    pos += _snwprintf( pos, 32, L"%.*o",
			     (szFormat[1] >= '0' && szFormat[1] <= '7') ? 3 : 1,
			     *szFormat );
	}
      }
      else
      {
	if (*szFormat == '"')
	{
	  if (first)
	    first = FALSE;
	  else if (szFormat[1] != '\0')
	    *pos++ = '\\';
	}
	*pos++ = *szFormat;
      }
    }
    *pos = '\0';
    szFormat = szEscape;
  }

  mutex = CreateMutex( NULL, FALSE, L"ANSICON_debug_file" );
  wait	= WaitForSingleObject( mutex, 500 );
  file	= fopen( tempfile, "at" ); // _fmode might be binary
  if (file != NULL)
  {
    fwprintf( file, L"%s (%lu): %s\n", prog, pid, szFormat );
    fclose( file );
  }
  if (wait == WAIT_OBJECT_0)
    ReleaseMutex( mutex );
  CloseHandle( mutex );
}
