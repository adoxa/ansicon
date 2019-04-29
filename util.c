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
  name = ac_wcsrchr( program, '\\' );
  if (name != NULL)
    ++name;
  else
    name = program;
  ext = ac_wcsrchr( name, '.' );
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


// GetVersion and GetVersionEx use Win32VersionValue from the header, which
// could be anything.  Retrieve the OS version from NTDLL's header.
DWORD get_os_version( void )
{
  PIMAGE_DOS_HEADER pDosHeader;
  PIMAGE_NT_HEADERS pNTHeader;

  pDosHeader = (PIMAGE_DOS_HEADER)GetModuleHandle( L"ntdll.dll" );
  pNTHeader = MakeVA( PIMAGE_NT_HEADERS, pDosHeader->e_lfanew );
  return pNTHeader->OptionalHeader.MajorOperatingSystemVersion << 8 |
	 pNTHeader->OptionalHeader.MinorOperatingSystemVersion;
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
    len = (DWORD)(wide ? lstrlen( src.w ) : strlen( src.a ));

  if (pos + len * 6 + 8 >= buf_len)
  {
    LPVOID tmp = HeapReAlloc( hHeap, 0, buf, buf_len + len * 6 + 8 );
    if (tmp == NULL)
      return pos;
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
	    pos += ac_sprintf( buf + pos, "x%2X", ch );
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
	pos += ac_sprintf( buf + pos, "\\x%2X", ch );
      else
	buf[pos++] = ch;
    }
    else
    {
      int mb = WideCharToMultiByte( cp, flags, src.w - 1, 1, buf + pos, 12,
				    NULL, pDef );
      if (def)
      {
	buf[pos++] = (quote) ? '\\' : '^';
	mb = ac_sprintf( buf + pos, ch < 0x100 ? "x%2X" : "u%4X", ch );
      }
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
      log_level = 0;
      return;
    }
    buf = HeapAlloc( hHeap, 0, 2048 );
    buf_len = (DWORD)HeapSize( hHeap, 0, buf );
    prefix_len = str_format( 0, TRUE, (DWORD_PTR)prog, 0 );
    prefix_len += ac_sprintf(buf+prefix_len, " (%u): ", GetCurrentProcessId());
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
      RtlFillMemory( buf + 2, 72, '=' );
      buf[0] = buf[74] = buf[76] = '\r';
      buf[1] = buf[75] = buf[77] = '\n';
      WriteFile( file, buf, 78, &written, NULL );
    }

    GetLocalTime( &now );
    len = ac_sprintf( buf, "ANSICON (" BITSA "-bit) v" PVERSA " log (%d)"
			   " started %d-%2d-%2d %d:%2d:%2d\r\n",
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
	case 'u': len += ac_sprintf( buf + len, "%u", (DWORD)num ); break;
	case 'X': len += ac_sprintf( buf + len, "%X", (DWORD)num ); break;
	case 'p':
#ifdef _WIN64
	  len += ac_sprintf( buf + len, "%8X_%8X",
					(DWORD)(num >> 32), (DWORD)num );
	  break;
#endif
	case 'q': len += ac_sprintf( buf + len, "%8X", (DWORD)num ); break;
	case 'P': len += ac_sprintf( buf + len, "00000000_%8X", (DWORD)num ); break;
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


// Provide custom versions of used C runtime functions, to remove dependence on
// the runtime library.

// For my purposes (palette index and colors):
// * no leading space;
// * base is 10 or 16;
// * number doesn't overflow.
unsigned long ac_wcstoul( const wchar_t* str, wchar_t** end, int base )
{
  unsigned c, n;
  unsigned long num = 0;

  for (;;)
  {
    n = -1;
    c = *str;
    if (c >= '0' && c <= '9')
      n = c - '0';
    else if (base == 16)
    {
      c |= 0x20;
      if (c >= 'a' && c <= 'f')
	n = c - 'a' + 10;
    }
    if (n == -1)
      break;

    num = num * base + n;
    ++str;
  }

  if (end != NULL)
    *end = (wchar_t*)str;
  return num;
}


// For my purposes (log level):
// * same as ac_wcstoul.
int ac_wtoi( const wchar_t* str )
{
  return (int)ac_wcstoul( str, NULL, 10 );
}


// For my purposes (default attribute):
// * same as ac_wcstoul.
long ac_wcstol( const wchar_t* str, wchar_t** end, int base )
{
  int neg = (*str == '-');
  long num = ac_wcstoul( str + neg, end, base );
  return neg ? -num : num;
}


// For my purposes (program separator):
// * set is only one or two characters.
wchar_t* ac_wcspbrk( const wchar_t* str, const wchar_t* set )
{
  while (*str != '\0')
  {
    if (*str == set[0] || *str == set[1])
      return (wchar_t*)str;
    ++str;
  }
  return NULL;
}


// For my purposes (path components):
// * c is not null.
wchar_t* ac_wcsrchr( const wchar_t* str, wchar_t c )
{
  wchar_t* last = NULL;

  while (*str != '\0')
  {
    if (*str == c)
      last = (wchar_t*)str;
    ++str;
  }

  return last;
}


// For my purposes (import module matching):
// * A-Z becomes a-z;
// * s2 is lower case;
// * both strings are at least LEN long;
// * returns 0 for match, 1 for no match.
int ac_strnicmp( const char* s1, const char* s2, size_t len )
{
  while (len--)
  {
    if (*s1 != *s2)
    {
      if (*s2 < 'a' || *s2 > 'z' || (*s1 | 0x20) != *s2)
	return 1;
    }
    ++s1;
    ++s2;
  }
  return 0;
}


static const char hex[16] = { '0','1','2','3','4','5','6','7',
			      '8','9','A','B','C','D','E','F' };

// For my purposes:
// * BUF is big enough;
// * FMT is valid;
// * width implies zero fill and the number is not bigger than the width;
// * only types d, u & X are supported, all as 32-bit unsigned;
// * BUF is NOT NUL-terminated.
int ac_sprintf( char* buf, const char* fmt, ... )
{
  va_list args;
  DWORD   num;
  int	  t, width;
  char*   beg = buf;

  va_start( args, fmt );

  while (*fmt)
  {
    t = *fmt++;
    if (t != '%')
      *buf++ = t;
    else
    {
      num = va_arg( args, DWORD );
      t = *fmt++;
      width = 0;
      if (t == '2' || t == '4' || t == '8')
      {
	width = t - '0';
	t = *fmt++;
      }
      if (t == 'X')
      {
	int bits;
	if (width == 0)
	{
	  if (num & 0xF0000000)
	    bits = 32;
	  else
	  {
	    bits = 4;
	    while (num >> bits)
	      bits += 4;
	  }
	}
	else
	  bits = width * 4;
	do
	{
	  bits -= 4;
	  *buf++ = hex[num >> bits & 0xF];
	} while (bits);
      }
      else // (t == 'd' || t == 'u')
      {
	if (width == 2)
	{
	  *buf++ = (int)(num / 10) + '0';
	  *buf++ = (int)(num % 10) + '0';
	}
	else
	{
	  unsigned power;
	  if (num >= 1000000000)
	    power = 1000000000;
	  else
	  {
	    power = 1;
	    while (num / (power * 10))
	      power *= 10;
	  }
	  do
	  {
	    *buf++ = num / power % 10 + '0';
	    power /= 10;
	  } while (power);
	}
      }
    }
  }

  return (int)(buf - beg);
}


// For my purposes:
// * BUF is big enough;
// * FMT is valid;
// * width is only for X, is only 2 and the number is not bigger than that;
// * X, d & u are 32-bit unsigned decimal;
// * c is not output if NUL;
// * no other type is used;
// * return value is not used.
int ac_wprintf( wchar_t* buf, const char* fmt, ... )
{
  va_list args;
  DWORD   num;
  int	  t;

  va_start( args, fmt );

  while (*fmt)
  {
    t = *fmt++;
    if (t != '%')
      *buf++ = t;
    else
    {
      num = va_arg( args, DWORD );
      t = *fmt++;
      if (t == '2')
      {
	++fmt;
	*buf++ = hex[num >> 4];
	*buf++ = hex[num & 0xF];
      }
      else if (t == 'X')
      {
	int bits;
	if (num & 0xF0000000)
	  bits = 32;
	else
	{
	  bits = 4;
	  while (num >> bits)
	    bits += 4;
	}
	do
	{
	  bits -= 4;
	  *buf++ = hex[num >> bits & 0xF];
	} while (bits);
      }
      else if (t == 'c')
      {
	if (num)
	  *buf++ = (wchar_t)num;
      }
      else // (t == 'd' || t == 'u')
      {
	unsigned power;
	if (num >= 1000000000)
	  power = 1000000000;
	else
	{
	  power = 1;
	  while (num / (power * 10))
	    power *= 10;
	}
	do
	{
	  *buf++ = num / power % 10 + '0';
	  power /= 10;
	} while (power);
      }
    }
  }
  *buf = '\0';

  return 0;
}
