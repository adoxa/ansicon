/*
  ANSICON.c - ANSI escape sequence console driver.

  Jason Hood, 21 to 23 October, 2005.

  Original injection code was derived from Console Manager by Sergey Oblomov
  (hoopoepg).  Use of FlushInstructionCache came from www.catch22.net.
  Additional information came from "Process-wide API spying - an ultimate hack",
  Anton Bassov's article in "The Code Project" (use of OpenThread).

  v1.01, 11 & 12 March, 2006:
    -m option to set "monochrome" (grey on black);
    restore original color on exit.

  v1.10, 22 February, 2009:
    ignore Ctrl+C/Ctrl+Break.

  v1.13, 21 & 27 March, 2009:
    alternate injection method, to work with DEP;
    use Unicode.

  v1.20, 17 to 21 June, 2009:
    use a combination of the two injection methods;
    test if ANSICON is already installed;
    added -e (and -E) option to echo the command line (without newline);
    added -t (and -T) option to type (display) files (with file name).

  v1.21, 23 September, 2009:
    added -i (and -u) to add (remove) ANSICON to AutoRun.

  v1.24, 6 & 7 January, 2010:
    no arguments to -t, or using "-" for the name, will read from stdin;
    fix -t and -e when ANSICON was already loaded.

  v1.25, 22 July, 2010:
    added -IU for HKLM.

  v1.30, 3 August to 7 September, 2010:
    x64 support.

  v1.31, 13 & 15 November, 2010:
    use LLW to fix potential Unicode path problems;
    VC compatibility (2008 Express for 32-bit, PSDK 2003 R2 for 64-bit);
    explicitly use wide characters (stick with TCHAR, but not <tchar.h>).

  v1.32, 4 to 22 December, 2010:
    make -p more robust;
    inject into GUI processes;
    -i implies -p.

  v1.50, 7 to 14 December, 2011:
    -u does not imply -p;
    add the PID to the debugging output;
    use ANSICON_VER to test if already installed;
    always place first in AutoRun;
    logging is always available, controlled by ANSICON_LOG environment variable;
    only restore the original color after program/echo/type;
    return program's exit code.

  7 January, 2012:
    fixed installing into a piped CMD.EXE;
    added a log message indicating all imports have been processed.

  v1.52, 10 April, 2012:
    fixed running "cmd" if "ComSpec" is not defined;
    pass process & thread identifiers on the command line (for x86->x64).

  v1.60, 22 & 24 November, 2012:
    set the code page to convert strings correctly;
    expand wildcards for -t;
    write the date if appending to the log.
*/

#define PDATE L"24 November, 2012"

#include "ansicon.h"
#include "version.h"
#include <tlhelp32.h>
#include <ctype.h>
#include <io.h>
#include <locale.h>

#ifdef __MINGW32__
int _CRT_glob = 0;
#endif


#ifdef _WIN64
# define BITS L"64"
#else
# define BITS L"32"
#endif


#define CMDKEY	L"Software\\Microsoft\\Command Processor"
#define AUTORUN L"AutoRun"


void   help( void );

void   display( LPCTSTR, BOOL );
void   print_error( LPCTSTR, ... );
LPTSTR skip_spaces( LPTSTR );
void   get_arg( LPTSTR, LPTSTR*, LPTSTR* );
void   get_file( LPTSTR, LPTSTR*, LPTSTR* );

void   process_autorun( TCHAR );

BOOL   find_proc_id( HANDLE snap, DWORD id, LPPROCESSENTRY32 ppe );
BOOL   GetParentProcessInfo( LPPROCESS_INFORMATION ppi, LPTSTR );


// The DLL shares this variable, so injection requires it here.
#ifdef _WIN64
DWORD  LLW32;
#endif


// Find the name of the DLL and inject it.
BOOL Inject( LPPROCESS_INFORMATION ppi, BOOL* gui, LPCTSTR app )
{
  DWORD len;
  WCHAR dll[MAX_PATH];
  int	type;

  DEBUGSTR( 1, L"%s (%lu)", app, ppi->dwProcessId );
  type = ProcessType( ppi, gui );
  if (type == 0)
  {
    fwprintf( stderr, L"ANSICON: %s: unsupported process.\n", app );
    return FALSE;
  }

  len = (DWORD)(prog - prog_path);
  memcpy( dll, prog_path, TSIZE(len) );
#ifdef _WIN64
  wsprintf( dll + len, L"ANSI%d.dll", type );
  if (type == 32)
    InjectDLL32( ppi, dll );
  else
    InjectDLL64( ppi, dll );
#else
  wcscpy( dll + len, L"ANSI32.dll" );
  InjectDLL32( ppi, dll );
#endif
  return TRUE;
}


static HANDLE hConOut;
static CONSOLE_SCREEN_BUFFER_INFO csbi;

void get_original_attr( void )
{
  hConOut = CreateFile( L"CONOUT$", GENERIC_READ | GENERIC_WRITE,
				    FILE_SHARE_READ | FILE_SHARE_WRITE,
				    NULL, OPEN_EXISTING, 0, 0 );
  GetConsoleScreenBufferInfo( hConOut, &csbi );
}


void set_original_attr( void )
{
  SetConsoleTextAttribute( hConOut, csbi.wAttributes );
  CloseHandle( hConOut );
}


DWORD CtrlHandler( DWORD event )
{
  return (event == CTRL_C_EVENT || event == CTRL_BREAK_EVENT);
}


int main( void )
{
  STARTUPINFO si;
  PROCESS_INFORMATION pi;
  LPTSTR  argv, arg, cmd;
  TCHAR   logstr[4];
  BOOL	  installed;
  BOOL	  shell, run, gui;
  HMODULE ansi;
  DWORD   len;
  int	  rc = 0;

  argv = GetCommandLine();
  len = (DWORD)wcslen( argv ) + 1;
  if (len < MAX_PATH)
    len = MAX_PATH;
  arg = malloc( TSIZE(len) );
  get_arg( arg, &argv, &cmd );	// skip the program name
  get_arg( arg, &argv, &cmd );

  if (*arg)
  {
    if (wcscmp( arg, L"/?" ) == 0 ||
	wcscmp( arg, L"--help" ) == 0)
    {
      help();
      return rc;
    }
    if (wcscmp( arg, L"--version" ) == 0)
    {
      _putws( L"ANSICON (" BITS L"-bit) version " PVERS L" (" PDATE L")." );
      return rc;
    }
  }

  prog = get_program_name( NULL );
  *logstr = '\0';
  GetEnvironmentVariable( L"ANSICON_LOG", logstr, lenof(logstr) );
  log_level = _wtoi( logstr );

  // Using "" for setlocale uses the system ANSI code page.
  sprintf( (LPSTR)logstr, ".%u", GetConsoleOutputCP() );
  setlocale( LC_CTYPE, (LPSTR)logstr );

#ifdef _WIN64
  if (*arg == '-' && arg[1] == 'P')
  {
    swscanf( arg + 2, L"%u:%u", &pi.dwProcessId, &pi.dwThreadId );
    pi.hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pi.dwProcessId);
    pi.hThread	= OpenThread( THREAD_ALL_ACCESS, FALSE, pi.dwThreadId );
    Inject( &pi, &gui, arg );
    CloseHandle( pi.hThread );
    CloseHandle( pi.hProcess );
    return 0;
  }
#endif

  if (log_level)
    DEBUGSTR( 1, NULL );	// start a new session

  installed = (GetEnvironmentVariable( L"ANSICON_VER", NULL, 0 ) != 0);
  // If it's already installed, remove it.  This serves two purposes: preserves
  // the parent's GRM; and unconditionally injects into GUI, without having to
  // worry about ANSICON_GUI.
  if (installed)
  {
    fputws( L"\33[m", stdout );
    FreeLibrary( GetModuleHandle( L"ANSI" BITS L".dll" ) );
  }

  shell = run = TRUE;
  get_original_attr();

  while (*arg == '-')
  {
    switch (arg[1])
    {
      case 'l':
	SetEnvironmentVariable( L"ANSICON_LOG", arg + 2 );
	log_level = _wtoi( arg + 2 );
	DEBUGSTR( 1, NULL );		// create a session
	break;

      case 'i':
      case 'I':
      case 'u':
      case 'U':
	shell = FALSE;
	process_autorun( arg[1] );
	if (arg[1] == 'u' || arg[1] == 'U')
	  break;
	// else fall through

      case 'p':
	shell = FALSE;
	// If it's already installed, there's no need to do anything.
	if (installed)
	{
	  DEBUGSTR( 1, L"Already installed" );
	}
	else if (GetParentProcessInfo( &pi, arg ))
	{
	  pi.hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pi.dwProcessId);
	  pi.hThread  = OpenThread( THREAD_ALL_ACCESS,	FALSE, pi.dwThreadId );
	  SuspendThread( pi.hThread );
	  if (!Inject( &pi, &gui, arg ))
	    rc = 1;
	  ResumeThread( pi.hThread );
	  CloseHandle( pi.hThread );
	  CloseHandle( pi.hProcess );
	}
	else
	{
	  fputws( L"ANSICON: could not obtain the parent process.\n", stderr );
	  rc = 1;
	}
	break;

      case 'm':
      {
	int a = wcstol( arg + 2, NULL, 16 );
	if (a == 0)
	  a = (arg[2] == '-') ? -7 : 7;
	if (a < 0)
	{
	  SetEnvironmentVariable( L"ANSICON_REVERSE", L"1" );
	  a = -a;
	  a = ((a >> 4) & 15) | ((a & 15) << 4);
	}
	SetConsoleTextAttribute( hConOut, (WORD)a );
	SetEnvironmentVariable( L"ANSICON_DEF", NULL );
	break;
      }

      case 'e':
      case 'E':
      case 't':
      case 'T':
	run = FALSE;
	++arg;
	goto arg_out;
    }
    get_arg( arg, &argv, &cmd );
  }
arg_out:
  if (run && *cmd == '\0')
  {
    if (!shell)
      run = FALSE;
    else if (!_isatty( 0 ))
    {
      *arg = 't';
      run = FALSE;
    }
  }

  if (run)
  {
    if (*cmd == '\0')
    {
      if (GetEnvironmentVariable( L"ComSpec", arg, MAX_PATH ))
	cmd = arg;
      else
      {
	// CreateProcessW writes to the string, so can't simply point to "cmd".
	static TCHAR cmdstr[] = L"cmd";
	cmd = cmdstr;
      }
    }

    ZeroMemory( &si, sizeof(si) );
    si.cb = sizeof(si);
    if (CreateProcess( NULL, cmd, NULL, NULL, TRUE, CREATE_SUSPENDED,
		       NULL, NULL, &si, &pi ))
    {
      Inject( &pi, &gui, arg );
      ResumeThread( pi.hThread );
      if (!gui)
      {
	SetConsoleCtrlHandler( (PHANDLER_ROUTINE)CtrlHandler, TRUE );
	WaitForSingleObject( pi.hProcess, INFINITE );
	GetExitCodeProcess( pi.hProcess, (LPDWORD)(LPVOID)&rc );
      }
      CloseHandle( pi.hProcess );
      CloseHandle( pi.hThread );
    }
    else
    {
      print_error( arg, arg );
      rc = 1;
    }
  }
  else if (*arg)
  {
    ansi = LoadLibrary( L"ANSI" BITS L".dll" );
    if (ansi == NULL)
    {
      print_error( L"ANSI" BITS L".dll" );
      rc = 1;
    }

    if (*arg == 'e' || *arg == 'E')
    {
      cmd += 2;
      if (*cmd == ' ' || *cmd == '\t')
	++cmd;
      fputws( cmd, stdout );
      if (*arg == 'e')
	putwchar( '\n' );
    }
    else // (*arg == 't' || *arg == 'T')
    {
      BOOL title = (*arg == 'T');
      get_file( arg, &argv, &cmd );
      if (*arg == '\0')
	wcscpy( arg, L"-" );
      do
      {
	if (title)
	{
	  wprintf( L"==> %s <==\n", arg );
	  display( arg, title );
	  putwchar( '\n' );
	}
	else
	  display( arg, title );
	get_file( arg, &argv, &cmd );
      } while (*arg);
    }

    FreeLibrary( ansi );
  }

  if (run || *arg)
    set_original_attr();
  else
    CloseHandle( hConOut );

  return rc;
}


// Display a file.
void display( LPCTSTR name, BOOL title )
{
  HANDLE in, out;
  BOOL	 pipe;
  char	 buf[8192];
  DWORD  len;

  if (*name == '-' && name[1] == '\0')
  {
    pipe = TRUE;
    in = GetStdHandle( STD_INPUT_HANDLE );
  }
  else
  {
    pipe = FALSE;
    in = CreateFile( name, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
		     NULL, OPEN_EXISTING, 0, NULL );
    if (in == INVALID_HANDLE_VALUE)
    {
      print_error( name );
      return;
    }
  }
  if (title)
  {
    putwchar( '\n' );
    // Need to flush, otherwise it's written *after* STD_OUTPUT_HANDLE should
    // it be redirected.
    fflush( stdout );
  }
  out = GetStdHandle( STD_OUTPUT_HANDLE );
  for (;;)
  {
    if (!ReadFile( in, buf, sizeof(buf), &len, NULL ))
    {
      if (GetLastError() != ERROR_BROKEN_PIPE)
	print_error( name );
      break;
    }
    if (len == 0)
      break;
    WriteFile( out, buf, len, &len, NULL );
  }
  if (!pipe)
    CloseHandle( in );
}


void print_error( LPCTSTR name, ... )
{
  LPTSTR errmsg = NULL;
  DWORD err = GetLastError();
  va_list arg;

  if (err == ERROR_BAD_EXE_FORMAT)
  {
    // This error requires an argument, which is a duplicate of name.
    va_start( arg, name );
    FormatMessage( FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER,
		   NULL, err, 0, (LPTSTR)(LPVOID)&errmsg, 0, &arg );
    va_end( arg );
    fwprintf( stderr, L"ANSICON: %s", errmsg );
  }
  else
  {
    FormatMessage( FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER,
		   NULL, err, 0, (LPTSTR)(LPVOID)&errmsg, 0, NULL );
    // Just in case there are other messages requiring args...
    if (errmsg == NULL)
      fwprintf( stderr, L"ANSICON: %s: Error %lu.\n", name, err );
    else
      fwprintf( stderr, L"ANSICON: %s: %s", name, errmsg );
  }
  LocalFree( errmsg );
}


// Add or remove ANSICON to AutoRun.
void process_autorun( TCHAR cmd )
{
  HKEY	 cmdkey;
  TCHAR  ansicon[MAX_PATH+80];
  TCHAR  logstr[80];
  LPTSTR autorun, ansirun;
  DWORD  len, type, exist;
  BOOL	 inst;

  if (log_level)
    _snwprintf( logstr, lenof(logstr), L"set ANSICON_LOG=%d&", log_level );
  else
    *logstr = '\0';
  len = TSIZE(_snwprintf( ansicon, lenof(ansicon),
		L"(if %%ANSICON_VER%%==^%%ANSICON_VER^%% %s\"%s\" -p)",
		logstr, prog_path ) + 1);

  inst = (towlower( cmd ) == 'i');
  RegCreateKeyEx( (iswlower( cmd )) ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE,
		  CMDKEY, 0, NULL,
		  REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL,
		  &cmdkey, &exist );
  exist = 0;
  RegQueryValueEx( cmdkey, AUTORUN, NULL, NULL, NULL, &exist );
  if (exist == 0)
  {
    if (inst)
      RegSetValueEx( cmdkey, AUTORUN, 0, REG_SZ, (PBYTE)ansicon, len );
  }
  else
  {
    // Let's assume there's sufficient memory.
    autorun = malloc( exist + len );
    RegQueryValueEx( cmdkey, AUTORUN, NULL, &type, (PBYTE)autorun, &exist );
    // Remove the existing command, if present.
    ansirun = wcsstr( autorun, L"(if %ANSICON_VER%" );
    if (ansirun != NULL)
    {
      LPTSTR tmp = wcschr( ansirun, '"' );      // opening quote
      tmp = wcschr( tmp + 1, '"' );             // closing quote
      tmp = wcschr( tmp + 1, ')' );             // closing bracket
      if (*++tmp == '&')
	++tmp;
      if (*tmp == '&')
	++tmp;
      if (*tmp == '\0')
      {
	if (ansirun > autorun && ansirun[-1] == '&')
	  --ansirun;
	if (ansirun > autorun && ansirun[-1] == '&')
	  --ansirun;
      }
      wcscpy( ansirun, tmp );
      exist = TSIZE((DWORD)wcslen( autorun ) + 1);
    }
    if (inst)
    {
      if (exist == sizeof(TCHAR))
	RegSetValueEx( cmdkey, AUTORUN, 0, REG_SZ, (PBYTE)ansicon, len );
      else
      {
	memmove( (PBYTE)autorun + len, autorun, exist );
	memcpy( autorun, ansicon, len );
	((PBYTE)autorun)[len-sizeof(TCHAR)] = '&';
	RegSetValueEx( cmdkey, AUTORUN, 0, type, (PBYTE)autorun, exist+len );
      }
    }
    else
    {
      if (exist == sizeof(TCHAR))
	RegDeleteValue( cmdkey, AUTORUN );
      else
	RegSetValueEx( cmdkey, AUTORUN, 0, type, (PBYTE)autorun, exist );
    }
    free( autorun );
  }
  RegCloseKey( cmdkey );
}


// Search each process in the snapshot for id.
BOOL find_proc_id( HANDLE snap, DWORD id, LPPROCESSENTRY32 ppe )
{
  BOOL fOk;

  ppe->dwSize = sizeof(PROCESSENTRY32);
  for (fOk = Process32First( snap, ppe ); fOk; fOk = Process32Next( snap, ppe ))
    if (ppe->th32ProcessID == id)
      break;

  return fOk;
}


// Obtain the process and thread identifiers of the parent process.
BOOL GetParentProcessInfo( LPPROCESS_INFORMATION ppi, LPTSTR name )
{
  HANDLE hSnap;
  PROCESSENTRY32 pe;
  THREADENTRY32  te;
  BOOL	 fOk;

  hSnap = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS|TH32CS_SNAPTHREAD, 0 );

  if (hSnap == INVALID_HANDLE_VALUE)
    return FALSE;

  find_proc_id( hSnap, GetCurrentProcessId(), &pe );
  if (!find_proc_id( hSnap, pe.th32ParentProcessID, &pe ))
  {
    CloseHandle( hSnap );
    return FALSE;
  }

  te.dwSize = sizeof(te);
  for (fOk = Thread32First( hSnap, &te ); fOk; fOk = Thread32Next( hSnap, &te ))
    if (te.th32OwnerProcessID == pe.th32ProcessID)
      break;

  CloseHandle( hSnap );

  ppi->dwProcessId = pe.th32ProcessID;
  ppi->dwThreadId  = te.th32ThreadID;
  wcscpy( name, pe.szExeFile );

  return fOk;
}


// Return the first non-space character from arg.
LPTSTR skip_spaces( LPTSTR arg )
{
  while (*arg == ' ' || *arg == '\t')
    ++arg;

  return arg;
}


// Retrieve an argument from the command line.	cmd gets the existing argv; argv
// is ready for the next argument.
void get_arg( LPTSTR arg, LPTSTR* argv, LPTSTR* cmd )
{
  LPTSTR line;

  line = *cmd = skip_spaces( *argv );
  while (*line != '\0')
  {
    if (*line == ' ' || *line == '\t')
    {
      ++line;
      break;
    }
    if (*line == '"')
    {
      while (*++line != '\0')
      {
	if (*line == '"')
	{
	  ++line;
	  break;
	}
	*arg++ = *line;
      }
    }
    else
      *arg++ = *line++;
  }
  *arg = '\0';
  *argv = line;
}


int glob_sort( const void* a, const void* b )
{
  return lstrcmpi( *(LPCTSTR*)a, *(LPCTSTR*)b );
}


// As get_arg, but expand wildcards.
void get_file( LPTSTR arg, LPTSTR* argv, LPTSTR* cmd )
{
  HANDLE  fh, in;
  WIN32_FIND_DATA fd;
  LPTSTR  path;
  int	  size;
  char	  buf[1024];
  static LPTSTR  name;
  static LPTSTR* glob;
  static int	 globbed;

  if (globbed != 0)
  {
    if (glob[globbed] == NULL)
    {
      free( glob );
      globbed = 0;
    }
    else
    {
      wcscpy( name, glob[globbed++] );
      return;
    }
  }

  get_arg( arg, argv, cmd );
  if (wcspbrk( arg, L"*?" ) != NULL)
  {
    fh = FindFirstFile( arg, &fd );
    if (fh != INVALID_HANDLE_VALUE)
    {
      size = 0;
      do
      {
	if (! (fd.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY |
				      FILE_ATTRIBUTE_HIDDEN)))
	{
	  ++globbed;
	  size += (int)wcslen( fd.cFileName ) + 1;
	}
      } while (FindNextFile( fh, &fd ));
      FindClose( fh );

      if (globbed != 0)
      {
	for (path = name = arg; *path != '\0'; ++path)
	  if (*path == '\\' || *path == '/')
	    name = path + 1;
	glob = malloc( (globbed + 1) * sizeof(LPTSTR) + TSIZE(size) );
	path = (LPTSTR)(glob + globbed + 1);
	globbed = 0;
	fh = FindFirstFile( arg, &fd );
	do
	{
	  if (! (fd.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY |
					FILE_ATTRIBUTE_HIDDEN)))
	  {
	    // Ignore apparent binary files.
	    wcscpy( name, fd.cFileName );
	    in = CreateFile( arg, GENERIC_READ,
			     FILE_SHARE_READ|FILE_SHARE_WRITE,
			     NULL, OPEN_EXISTING, 0, NULL );
	    if (in != INVALID_HANDLE_VALUE)
	    {
	      ReadFile( in, buf, sizeof(buf), (LPVOID)&size, NULL );
	      CloseHandle( in );
	      if (memchr( buf, 0, size ) != NULL)
		continue;
	    }
	    size = (int)wcslen( fd.cFileName ) + 1;
	    memcpy( path, fd.cFileName, TSIZE(size) );
	    glob[globbed++] = path;
	    path += size;
	  }
	} while (FindNextFile( fh, &fd ));
	FindClose( fh );
	glob[globbed] = NULL;

	qsort( glob, globbed, sizeof(LPTSTR), glob_sort );

	wcscpy( name, glob[0] );
	globbed = 1;
      }
    }
  }
}


void help( void )
{
  _putws(
L"ANSICON by Jason Hood <jadoxa@yahoo.com.au>.\n"
L"Version " PVERS L" (" PDATE L").  Freeware.\n"
L"http://ansicon.adoxa.cjb.net/\n"
L"\n"
#ifdef _WIN64
L"Process ANSI escape sequences in Windows console programs.\n"
#else
L"Process ANSI escape sequences in Win32 console programs.\n"
#endif
L"\n"
L"ansicon [-l<level>] [-i] [-I] [-u] [-U] [-m[<attr>]] [-p]\n"
L"        [-e|E string | -t|T [file(s)] | program [args]]\n"
L"\n"
L"  -l\t\tset the logging level (1=process, 2=module, 3=function,\n"
L"    \t\t +4=output, +8=append) for program (-p is unaffected)\n"
L"  -i\t\tinstall - add ANSICON to the AutoRun entry (also implies -p)\n"
L"  -u\t\tuninstall - remove ANSICON from the AutoRun entry\n"
L"  -I -U\t\tuse local machine instead of current user\n"
L"  -m\t\tuse grey on black (\"monochrome\") or <attr> as default color\n"
L"  -p\t\thook into the parent process\n"
L"  -e\t\techo string\n"
L"  -E\t\techo string, don't append newline\n"
L"  -t\t\tdisplay files (\"-\" for stdin), combined as a single stream\n"
L"  -T\t\tdisplay files, name first, blank line before and after\n"
L"  program\trun the specified program\n"
L"  nothing\trun a new command processor, or display stdin if redirected\n"
L"\n"
L"<attr> is one or two hexadecimal digits; please use \"COLOR /?\" for details.\n"
L"It may start with '-' to reverse foreground and background (but not for -p)."
	);
}
