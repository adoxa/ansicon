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
*/

#define PVERS L"1.40"
#define PDATE L"1 March, 2011"

#include "ansicon.h"
#include <shellapi.h>
#include <tlhelp32.h>
#include <ctype.h>
#include <io.h>
#include <objbase.h>
#include <psapi.h>

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
LPTSTR skip_spaces( LPTSTR );
LPTSTR skip_arg( LPTSTR );

void   process_autorun( TCHAR );

BOOL   find_proc_id( HANDLE snap, DWORD id, LPPROCESSENTRY32 ppe );
BOOL   GetParentProcessInfo( LPPROCESS_INFORMATION ppi );


// Find the name of the DLL and inject it.
BOOL Inject( LPPROCESS_INFORMATION ppi )
{
  DWORD len;
  WCHAR dll[MAX_PATH];
  int	type;

#if (MYDEBUG > 0)
  if (GetModuleFileNameEx( ppi->hProcess, NULL, dll, lenof(dll) ))
    DEBUGSTR( L"%s", dll );
#endif
  type = ProcessType( ppi );
  if (type == 0)
    return FALSE;

  len = GetModuleFileName( NULL, dll, lenof(dll) );
  while (dll[len-1] != '\\')
    --len;
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


//int _tmain( int argc, TCHAR* argv[] )
int main( void )
{
  STARTUPINFO si;
  PROCESS_INFORMATION pi;
  TCHAR*  cmd;
  BOOL	  option;
  BOOL	  opt_m;
  BOOL	  installed;
  HMODULE ansi;
  int	  rc = 0;

  int argc;
  LPWSTR* argv = CommandLineToArgvW( GetCommandLine(), &argc );

  if (argc > 1)
  {
    if (lstrcmp( argv[1], L"--help" ) == 0 ||
	(argv[1][0] == '-' && (argv[1][1] == '?' || argv[1][1] == 'h')) ||
	(argv[1][0] == '/' && argv[1][1] == '?'))
    {
      help();
      return rc;
    }
    if (lstrcmp( argv[1], L"--version" ) == 0)
    {
      _putws( L"ANSICON (" BITS L"-bit) version " PVERS L" (" PDATE L")." );
      return rc;
    }
  }

#if (MYDEBUG > 1)
  DEBUGSTR( NULL ); // create a new file
#endif

  option = (argc > 1 && argv[1][0] == '-');
  if (option && (towlower( argv[1][1] ) == 'i' ||
		 towlower( argv[1][1] ) == 'u'))
  {
    process_autorun( argv[1][1] );
    argv[1][1] = 'p';
  }

  get_original_attr();

  opt_m = FALSE;
  if (option && argv[1][1] == 'm')
  {
    WORD attr = 7;
    if (iswxdigit( argv[1][2] ))
    {
      attr = iswdigit( argv[1][2] ) ? argv[1][2] - '0'
				    : (argv[1][2] | 0x20) - 'a' + 10;
      if (iswxdigit( argv[1][3]))
      {
	attr <<= 4;
	attr |= iswdigit( argv[1][3] ) ? argv[1][3] - '0'
				       : (argv[1][3] | 0x20) - 'a' + 10;
      }
    }
    SetConsoleTextAttribute( hConOut, attr );

    opt_m = TRUE;
    ++argv;
    --argc;
    option = (argc > 1 && argv[1][0] == '-');
  }

  installed = (GetEnvironmentVariable( L"ANSICON", NULL, 0 ) != 0);

  if (option && argv[1][1] == 'p')
  {
    // If it's already installed, there's no need to do anything.
    if (installed)
      ;
    else if (GetParentProcessInfo( &pi ))
    {
      pi.hProcess = OpenProcess( PROCESS_ALL_ACCESS, FALSE, pi.dwProcessId );
      pi.hThread  = OpenThread(  THREAD_ALL_ACCESS,  FALSE, pi.dwThreadId  );
      SuspendThread( pi.hThread );
      if (!Inject( &pi ))
      {
	_putws( L"ANSICON: parent process type is not supported." );
	rc = 1;
      }
      ResumeThread( pi.hThread );
      CloseHandle( pi.hThread );
      CloseHandle( pi.hProcess );
    }
    else
    {
      _putws( L"ANSICON: could not obtain the parent process." );
      rc = 1;
    }
  }
  else
  {
    ansi = 0;
    if (!installed)
    {
      ansi = LoadLibrary( L"ANSI" BITS L".dll" );
      if (!ansi)
      {
	fputws( L"ANSICON: failed to load ANSI" BITS L".dll.\n", stderr );
	rc = 1;
      }
    }

    if (option && (argv[1][1] == 't' || argv[1][1] == 'T'))
    {
      BOOL title = (argv[1][1] == 'T');
      if (argc == 2)
      {
	argv[2] = L"-";
	++argc;
      }
      for (; argc > 2; ++argv, --argc)
      {
	if (title)
	  wprintf( L"==> %s <==\n", argv[2] );
	display( argv[2], title );
	if (title)
	  putwchar( '\n' );
      }
    }
    else
    {
      // Retrieve the original command line, skipping our name and the option.
      cmd = skip_spaces( skip_arg( skip_spaces( GetCommandLine() ) ) );
      if (opt_m)
	cmd = skip_spaces( skip_arg( cmd ) );

      if (cmd[0] == '-' && (cmd[1] == 'e' || cmd[1] == 'E'))
      {
	fputws( cmd + 3, stdout );
	if (cmd[1] == 'e')
	  putwchar( '\n' );
      }
      else if (!_isatty( 0 ) && *cmd == '\0')
      {
	display( L"-", FALSE );
      }
      else
      {
	if (*cmd == '\0')
	{
	  cmd = _wgetenv( L"ComSpec" );
	  if (cmd == NULL)
	    cmd = L"cmd";
	}

	ZeroMemory( &si, sizeof(si) );
	si.cb = sizeof(si);
	if (CreateProcess( NULL, cmd, NULL,NULL, TRUE, 0, NULL,NULL, &si, &pi ))
	{
	  BOOL	console = FALSE;
	  TCHAR name[MAX_PATH];
	  DWORD rc;
	  CoInitialize( NULL );
	  do
	  {
	    // When I first tried doing this, it took a little while to
	    // succeed.  Testing again shows it works immediately - perhaps the
	    // CoInitialize introduces enough of a delay.  Still, play it safe
	    // and keep trying.  And if you're wondering why I do it at all,
	    // ProcessType may detect GUI, even for a console process.	That's
	    // fine after injection (including -p), but not here.  We *need* to
	    // suspend our own execution whilst running the child, otherwise
	    // bad things happen (besides which, I want to restore the original
	    // attributes when the child exits).
	    if (GetModuleFileNameEx( pi.hProcess, NULL, name, lenof(name) ))
	    {
	      DWORD_PTR info;
	      info = SHGetFileInfo( name, 0, NULL, 0, SHGFI_EXETYPE );
	      if (info == 0x00004550) // console PE
		console = TRUE;
	      DEBUGSTR( L"%s", name );
	      DEBUGSTR( L"  %s (%p)", (console) ? L"Console" : L"Not console",
				      info );
	      break;
	    }
	    Sleep( 10 );
	  } while (GetExitCodeProcess( pi.hProcess, &rc ) &&
		   rc == STILL_ACTIVE);
	  CoUninitialize();
	  if (console)
	  {
	    SetConsoleCtrlHandler( (PHANDLER_ROUTINE)CtrlHandler, TRUE );
	    WaitForSingleObject( pi.hProcess, INFINITE );
	  }
	  CloseHandle( pi.hProcess );
	  CloseHandle( pi.hThread );
	}
	else
	{
	  *skip_arg( cmd ) = '\0';
	  wprintf( L"ANSICON: '%s' could not be executed.\n", cmd );
	  rc = 1;
	}
      }
    }

    if (ansi)
      FreeLibrary( ansi );
  }

  set_original_attr();
  return rc;
}


void print_error( LPCTSTR name, BOOL title )
{
  LPTSTR errmsg;

  FormatMessage( FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER,
		 NULL, GetLastError(), 0, (LPTSTR)(LPVOID)&errmsg, 0, NULL );
  if (!title)
    wprintf( L"ANSICON: %s: ", name );
  fputws( errmsg, stdout );
  LocalFree( errmsg );
}


// Display a file.
void display( LPCTSTR name, BOOL title )
{
  HANDLE file;
  int	 c;
  LARGE_INTEGER size, offset;

  // Handle the pipe differently.
  if (*name == '-' && name[1] == '\0')
  {
    if (title)
      putwchar( '\n' );
    while ((c = getchar()) != EOF)
      putchar( c );
    return;
  }

  file = CreateFile( name, GENERIC_READ, FILE_SHARE_READ, NULL,
		     OPEN_EXISTING, 0, NULL );
  if (file == INVALID_HANDLE_VALUE)
  {
    print_error( name, title );
    return;
  }

  GetFileSizeEx( file, &size );
  if (size.QuadPart != 0)
  {
    HANDLE map = CreateFileMapping( file, NULL, PAGE_READONLY, 0, 0, NULL );
    if (map)
    {
      if (title)
	putwchar( '\n' );
      offset.QuadPart = 0;
      do
      {
	DWORD len = (size.QuadPart > 65536) ? 65536 : size.LowPart;
	LPVOID mem = MapViewOfFile( map, FILE_MAP_READ, offset.HighPart,
				    offset.LowPart, len );
	if (mem)
	{
	  fwrite( mem, 1, len, stdout );
	  UnmapViewOfFile( mem );
	}
	else
	{
	  print_error( name, title );
	  break;
	}
	offset.QuadPart += len;
	size.QuadPart -= len;
      } while (size.QuadPart);
      CloseHandle( map );
    }
    else
      print_error( name, title );
  }
  CloseHandle( file );
}


// Add or remove ANSICON to AutoRun.
void process_autorun( TCHAR cmd )
{
  HKEY	 cmdkey;
  TCHAR  ansicon[MAX_PATH+8];
  LPTSTR autorun, ansirun;
  DWORD  len, type, exist;
  BOOL	 inst;

  len = GetModuleFileName( NULL, ansicon+2, MAX_PATH );
  ansicon[0] = '&';
  ansicon[1] = ansicon[2+len] = '"';
  wcscpy( ansicon + 3+len, L" -p" );
  len += 6;

  inst = (towlower( cmd ) == 'i');
  RegCreateKeyEx( (iswlower( cmd )) ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE,
		  CMDKEY, 0, NULL,
		  REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL,
		  &cmdkey, &exist );
  exist = 0;
  RegQueryValueEx( cmdkey, AUTORUN, NULL, NULL, NULL, &exist );
  autorun = malloc( exist + len * sizeof(TCHAR) + sizeof(TCHAR) );
  // Let's assume there's sufficient memory.
  if (exist > sizeof(TCHAR))
  {
    exist += sizeof(TCHAR);
    RegQueryValueEx( cmdkey, AUTORUN, NULL, &type, (PBYTE)autorun, &exist );
    ansirun = wcsstr( autorun, ansicon+1 );
    if (inst)
    {
      if (!ansirun)
      {
	wcscpy( (LPTSTR)((PBYTE)autorun + exist - sizeof(TCHAR)), ansicon );
	RegSetValueEx( cmdkey, AUTORUN, 0, type, (PBYTE)autorun,
		       exist + len*sizeof(TCHAR) );
      }
    }
    else
    {
      if (ansirun)
      {
	if (ansirun == autorun && exist == len*sizeof(TCHAR))
	  RegDeleteValue( cmdkey, AUTORUN );
	else
	{
	  if (ansirun > autorun && ansirun[-1] == '&')
	    --ansirun;
	  else if (autorun[len-1] != '&')
	    --len;
	  memcpy( ansirun, ansirun + len, exist - len*sizeof(TCHAR) );
	  RegSetValueEx( cmdkey, AUTORUN, 0, type, (PBYTE)autorun,
			 exist - len*sizeof(TCHAR) );
	}
      }
    }
  }
  else if (inst)
  {
    RegSetValueEx( cmdkey, AUTORUN, 0, REG_SZ, (PBYTE)(ansicon+1),
		   len*sizeof(TCHAR) );
  }

  free( autorun );
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
BOOL GetParentProcessInfo( LPPROCESS_INFORMATION ppi )
{
  HANDLE hSnap;
  PROCESSENTRY32 pe;
  THREADENTRY32  te;
  DWORD  id = GetCurrentProcessId();
  BOOL	 fOk;

  hSnap = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS|TH32CS_SNAPTHREAD, id );

  if (hSnap == INVALID_HANDLE_VALUE)
    return FALSE;

  find_proc_id( hSnap, id, &pe );
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

  return fOk;
}


// Return the first non-space character from cmd.
LPTSTR skip_spaces( LPTSTR cmd )
{
  while ((*cmd == ' ' || *cmd == '\t') && *cmd != '\0')
    ++cmd;

  return cmd;
}


// Return the end of the argument at cmd.
LPTSTR skip_arg( LPTSTR cmd )
{
  while (*cmd != ' ' && *cmd != '\t' && *cmd != '\0')
  {
    if (*cmd == '"')
    {
      do
	++cmd;
      while (*cmd != '"' && *cmd != '\0');
      if (*cmd == '\0')
	--cmd;
    }
    ++cmd;
  }

  return cmd;
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
L"ansicon -i|I | -u|U\n"
L"ansicon [-m[<attr>]] [-p | -e|E string | -t|T [file(s)] | program [args]]\n"
L"\n"
L"  -i\t\tinstall - add ANSICON to the AutoRun entry (implies -p)\n"
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
L"<attr> is one or two hexadecimal digits; please use \"COLOR /?\" for details."
	      );
}
