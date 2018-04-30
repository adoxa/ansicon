/*
  util.c - Utility functions.
*/

#include "ansicon.h"
#include "version.h"


TCHAR	prog_path[MAX_PATH];
LPTSTR	prog;

int	log_level;

TCHAR	DllName[MAX_PATH];	// Dll file name
char	ansi_dll[MAX_PATH];
DWORD	ansi_len;
#ifdef _WIN64
char*	ansi_bits;
#endif


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


// Get the ANSI path of the DLL for the import.  If it can't be converted,
// just use the name and hope it's on the PATH.
void set_ansi_dll( void )
{
  BOOL bad;

  ansi_len = WideCharToMultiByte( CP_ACP, WC_NO_BEST_FIT_CHARS, DllName, -1,
				  NULL, 0, NULL, &bad );
  if (bad || ansi_len > MAX_PATH)
  {
#ifdef _WIN64
    ansi_bits = ansi_dll + 4;
    if (*DllNameType == '6')
      memcpy( ansi_dll, "ANSI64.dll\0", 12 );
    else
#endif
    memcpy( ansi_dll, "ANSI32.dll\0", 12 );
    ansi_len = 12;
  }
  else
  {
    WideCharToMultiByte( CP_ACP, WC_NO_BEST_FIT_CHARS, DllName, -1,
			 ansi_dll, MAX_PATH, NULL, NULL );
#ifdef _WIN64
    ansi_bits = ansi_dll + ansi_len - 7;
#endif
    ansi_len = (ansi_len + 3) & ~3;
  }
}


static LPSTR buf;
static DWORD buf_len;
static BOOL  quote, alt;

static DWORD str_format( DWORD pos, BOOL wide, DWORD_PTR str, DWORD len )
{
  static UINT  cp;
  static DWORD flags;
  static BOOL  def, *pDef, start_trail;
  union
  {
    LPSTR  a;
    LPWSTR w;
  } src;
  int  ch;
  BOOL trail;

  src.a = (LPSTR)str;
  if (len == 0 && str != 0)
    len = (DWORD)(wide ? wcslen( src.w ) : strlen( src.a ));

  if (pos + len * 6 + 8 >= buf_len)
  {
    LPVOID tmp = HeapReAlloc( hHeap, 0, buf, buf_len + len * 6 + 8 );
    if (tmp == NULL)
      return 0;
    buf = tmp;
    buf_len = (DWORD)HeapSize( hHeap, 0, buf );
  }

  if (len == 0)
  {
    if (str == 0)
    {
      memcpy( buf + pos, "<null>", 6 );
      pos += 6;
    }
    else if (quote)
    {
      buf[pos++] = '"';
      buf[pos++] = '"';
    }
    else if (alt)
    {
      memcpy( buf + pos, "<empty>", 7 );
      pos += 7;
    }
    return pos;
  }

  if (cp != GetConsoleOutputCP())
  {
    cp = GetConsoleOutputCP();
    if (wide)
    {
      wchar_t und = L'\xFFFF';
      flags = WC_NO_BEST_FIT_CHARS;
      pDef = &def;
      // Some code pages don't support the default character.
      if (!WideCharToMultiByte( cp, flags, &und, 1, buf + pos, 12, NULL, pDef ))
      {
	flags = 0;
	pDef = NULL;
	def = FALSE;
      }
    }
  }

  if (quote)
    buf[pos++] = '"';

  trail = FALSE;
  while (len-- != 0)
  {
    if (wide)
      ch = *src.w++;
    else
      ch = (BYTE)*src.a++;

    if (ch < 32 || (quote && start_trail))
    {
      start_trail = FALSE;
      if (quote)
      {
	buf[pos++] = '\\';
	switch (ch)
	{
	  case '\0': buf[pos++] = '0'; break;
	  case '\a': buf[pos++] = 'a'; break;
	  case '\b': buf[pos++] = 'b'; break;
	  case '\t': buf[pos++] = 't'; break;
	  case '\n': buf[pos++] = 'n'; break;
	  case '\v': buf[pos++] = 'v'; break;
	  case '\f': buf[pos++] = 'f'; break;
	  case '\r': buf[pos++] = 'r'; break;
	  case	27 : buf[pos++] = 'e'; break;
	  default:
	    pos += sprintf( buf + pos, "x%.2X", ch );
	}
      }
      else
      {
	buf[pos++] = '^';
	buf[pos++] = ch + '@';
      }
    }
    else if (quote && ch == '"')
    {
      buf[pos++] = '\\';
      buf[pos++] = ch;
    }
    else if (!wide)
    {
      if (quote && (cp == 932 || cp == 936 || cp == 949 || cp == 950))
      {
	if (trail)
	  trail = FALSE;
	else if (IsDBCSLeadByteEx( cp, (char)ch ))
	{
	  if (len == 0)
	    start_trail = TRUE;
	  else
	    trail = TRUE;
	}
      }
      if (quote && start_trail)
	pos += sprintf( buf + pos, "\\x%.2X", ch );
      else
	buf[pos++] = ch;
    }
    else
    {
      int mb = WideCharToMultiByte( cp, flags, src.w - 1, 1, buf + pos, 12,
				    NULL, pDef );
      if (def)
	mb = sprintf( buf + pos, ch < 0x100 ? "%cx%.2X" : "%cu%.4X",
		      (quote) ? '\\' : '^', ch );
      pos += mb;
    }
  }

  if (quote)
    buf[pos++] = '"';

  return pos;
}

void DEBUGSTR( int level, LPCSTR szFormat, ... )
{
  static int	prefix_len;
  static HANDLE mutex;
  static DWORD	size;

  WCHAR     temp[MAX_PATH];
  HANDLE    file;
  va_list   pArgList;
  DWORD     len, slen, written;
  DWORD_PTR num;

  if ((log_level & 3) < level && !(level & 4 & log_level))
    return;

  if (mutex == NULL)
  {
    mutex = CreateMutex( NULL, FALSE, L"ANSICON_debug_file" );
    if (mutex == NULL)
    {
      file = INVALID_HANDLE_VALUE;
      return;
    }
    buf = HeapAlloc( hHeap, 0, 2048 );
    buf_len = (DWORD)HeapSize( hHeap, 0, buf );
    prefix_len = sprintf( buf, "%S (%lu): ", prog, GetCurrentProcessId() );
  }
  if (WaitForSingleObject( mutex, 500 ) == WAIT_TIMEOUT)
    return;

  ExpandEnvironmentStrings( L"%TEMP%\\ansicon.log", temp, lenof(temp) );
  file = CreateFile( temp, GENERIC_WRITE, FILE_SHARE_READ, NULL,
		     (szFormat != NULL || (log_level & 8)) ? OPEN_ALWAYS
							   : CREATE_ALWAYS,
		     0, NULL );
  if (file == INVALID_HANDLE_VALUE)
  {
    ReleaseMutex( mutex );
    CloseHandle( mutex );
    return;
  }

  len = SetFilePointer( file, 0, NULL, FILE_END );
  if (len == 0 || szFormat == NULL)
  {
    char buf[128];
    SYSTEMTIME now;

    size = 0;

    if (len != 0)
    {
      memset( buf + 2, '=', 72 );
      buf[0] = buf[74] = buf[76] = '\r';
      buf[1] = buf[75] = buf[77] = '\n';
      WriteFile( file, buf, 78, &written, NULL );
    }

    GetLocalTime( &now );
    len = sprintf( buf, "ANSICON (" BITSA "-bit) v" PVERSA " log (%d)"
			" started %d-%.2d-%.2d %d:%.2d:%.2d\r\n",
			log_level,
			now.wYear, now.wMonth, now.wDay,
			now.wHour, now.wMinute, now.wSecond );
    WriteFile( file, buf, len, &written, NULL );
    if (szFormat == NULL)
    {
      CloseHandle( file );
      ReleaseMutex( mutex );
      return;
    }
  }
  if (len != size)
    WriteFile( file, "\r\n", 2, &written, NULL );

  va_start( pArgList, szFormat );

  // Customized printf, mainly to handle wide-character strings the way I want.
  // It only supports:
  //   %u   unsigned 32-bit decimal
  //   %X   unsigned 32-bit upper case hexadecimal
  //   %p   native pointer
  //   %q   native pointer, display as 32 bits
  //   %P   32-bit pointer, display as 64 bits
  //   %s   null-terminated byte characters
  //   %S   null-terminated wide characters
  //
  // s & S may be prefixed with (in this order):
  //   "    quote the string, using C-style escapes and <null> for NULL
  //   #    use <null> for NULL and <empty> for ""
  //   <    length of the string is the previous %u
  //   *    length of the string is the parameter before the string
  //
  // Wide strings are converted according to the current code page; if a
  // character could not be translated, hex is used.
  //
  // C-style escapes are the standard backslash sequences, plus '\e' for ESC,
  // with '\x' used for two hex digits and '\u' for four.  Otherwise, caret
  // notation is used to represent controls, with '^x'/'^u' for hex.

  num = 0;
  len = prefix_len;
  while (*szFormat != '\0')
  {
    if (*szFormat != '%')
      buf[len++] = *szFormat++;
    else
    {
      quote = alt = FALSE;
      ++szFormat;
      if (*szFormat == '"')
      {
	quote = TRUE;
	++szFormat;
      }
      if (*szFormat == '#')
      {
	alt = TRUE;
	++szFormat;
      }
      slen = 0;
      if (*szFormat == '<')
      {
	slen = (DWORD)num;
	++szFormat;
      }
      if (*szFormat == '*')
      {
	slen = va_arg( pArgList, DWORD );
	++szFormat;
      }
      num = va_arg( pArgList, DWORD_PTR );
      switch (*szFormat++)
      {
	case 'u': len += sprintf( buf + len, "%u", (DWORD)num ); break;
	case 'X': len += sprintf( buf + len, "%X", (DWORD)num ); break;
	case 'p':
#ifdef _WIN64
	  len += sprintf( buf + len, "%.8X_%.8X",
				     (DWORD)(num >> 32), (DWORD)num );
	  break;
#endif
	case 'q': len += sprintf( buf + len, "%.8X", (DWORD)num ); break;
	case 'P': len += sprintf( buf + len, "00000000_%.8X", (DWORD)num ); break;
	case 's': len = str_format( len, FALSE, num, slen ); break;
	case 'S': len = str_format( len, TRUE, num, slen ); break;
	default:
	  buf[len++] = '%';
	  if (szFormat[-1] == '\0')
	    --szFormat;
	  else
	    buf[len++] = szFormat[-1];
      }
      if (len >= buf_len - 20)
      {
	LPVOID tmp = HeapReAlloc( hHeap, 0, buf, buf_len + 128 );
	if (tmp == NULL)
	  break;
	buf = tmp;
	buf_len = (DWORD)HeapSize( hHeap, 0, buf );
      }
    }
  }
  buf[len++] = '\r';
  buf[len++] = '\n';

  WriteFile( file, buf, len, &written, NULL );

  size = GetFileSize( file, NULL );
  CloseHandle( file );
  ReleaseMutex( mutex );
}
