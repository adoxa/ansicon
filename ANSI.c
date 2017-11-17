/*
  ANSI.c - ANSI escape sequence console driver.

  Jason Hood, 21 & 22 October, 2005.

  Derived from ANSI.xs by Jean-Louis Morel, from his Perl package
  Win32::Console::ANSI.  I removed the codepage conversion ("\e(") and added
  WriteConsole hooking.

  v1.01, 11 & 12 March, 2006:
    disable when console has disabled processed output;
    \e[5m (blink) is the same as \e[4m (underline);
    do not conceal control characters (0 to 31);
    \e[m will restore original color.

  v1.10, 22 February, 2009:
    fix MyWriteConsoleW for strings longer than the buffer;
    initialise attributes to current;
    hook into child processes.

  v1.11, 28 February, 2009:
    fix hooking into child processes (only do console executables).

  v1.12, 9 March, 2009:
    really fix hooking (I didn't realise MinGW didn't generate relocations).

  v1.13, 21 & 27 March, 2009:
    alternate injection method, to work with DEP;
    use Unicode and the current output code page (not OEMCP).

  v1.14, 3 April, 2009:
    fix test for empty import section.

  v1.15, 17 May, 2009:
    properly update lpNumberOfCharsWritten in MyWriteConsoleA.

  v1.20, 26 & 29 May, 17 to 21 June, 2009:
    create an ANSICON environment variable;
    hook GetEnvironmentVariable to create ANSICON dynamically;
    use another injection method.

  v1.22, 5 October, 2009:
    hook LoadLibrary to intercept the newly loaded functions.

  v1.23, 11 November, 2009:
    unload gracefully;
    conceal characters by making foreground same as background;
    reverse the bold/underline attributes, too.

  v1.25, 15, 20 & 21 July, 2010:
    hook LoadLibraryEx (now cscript works);
    Win7 support.

  v1.30, 3 August to 7 September, 2010:
    x64 support.

  v1.31, 13 & 19 November, 2010:
    fix multibyte conversion problems.

  v1.32, 4 to 22 December, 2010:
    test for lpNumberOfCharsWritten/lpNumberOfBytesWritten being NULL;
    recognise DSR and xterm window title;
    ignore sequences starting with \e[? & \e[>;
    close the handles opened by CreateProcess.

  v1.40, 25 & 26 February, 1 March, 2011:
    hook GetProcAddress, addresses issues with .NET (work with PowerShell);
    implement SO & SI to use the DEC Special Graphics Character Set (enables
     line drawing via ASCII); ignore \e(X & \e)X (where X is any character);
    add \e[?25h & \e[?25l to show/hide the cursor (DECTCEM).

  v1.50, 7 to 14 December, 2011:
    added dynamic environment variable ANSICON_VER to return version;
    read ANSICON_EXC environment variable to exclude selected modules;
    read ANSICON_GUI environment variable to hook selected GUI programs;
    read ANSICON_DEF environment variable to set the default GR;
    transfer current GR to child, read it on exit.

  v1.51, 15 January, 5, 22 & 24 February, 2012:
    added log mask 16 to log all the imported modules of imported modules;
    ignore the version within the core API DLL names;
    fix 32-bit process trying to identify 64-bit process;
    hook _lwrite & _hwrite.

  v1.52, 10 April, 1 & 2 June, 2012:
    use ansicon.exe to enable 32-bit to inject into 64-bit;
    implement \e[39m & \e[49m (only setting color, nothing else);
    added the character/line equivalents (keaj`) of the cursor movement
     sequences (ABCDG), as well as vertical absolute (d) and erase characters
     (X).

  v1.53, 12 June, 2012:
    fixed Update_GRM when running multiple processes (e.g. "cl /MP").

  v1.60, 22 to 24 November, 2012:
    alternative method to obtain LLW for 64->32 injection;
    support for VC6 (remove section pragma, rename isdigit to is_digit).

  v1.61, 14 February, 2013:
    go back to using ANSI-LLW.exe for 64->32 injection.

  v1.62, 17 & 18 July, 2013:
    another method to obtain LLW for 64->32 injection.

  v1.64, 2 August, 2013:
    better method of determining a console handle (see IsConsoleHandle).

  v1.65, 28 August, 2013:
    fix \e[K (was using window, not buffer).

  v1.66, 20 & 21 September, 2013:
    fix 32-bit process trying to detect 64-bit process.

  v1.70, 25 January to 26 February, 2014:
    don't hook ourself from LoadLibrary or LoadLibraryEx;
    update the LoadLibraryEx flags that should not cause hooking;
    inject by manipulating the import directory table; for 64-bit AnyCPU use
     ntdll's LdrLoadDll via CreateRemoteThread;
    restore original attributes on detach (for LoadLibrary/FreeLibrary usage);
    log: remove the quotes around the CreateProcess command line string and
	  distinguish NULL and "" args;
    attributes (and saved position) are local to each console window;
    exclude entire programs, by not using an extension in ANSICON_EXC;
    hook modules injected via CreateRemoteThread+LoadLibrary;
    hook all modules loaded due to LoadLibrary, not just the specified;
    don't hook a module that's already hooked us;
    better parsing of escape & CSI sequences;
    ignore xterm 38 & 48 SGR values;
    change G1 blank from space to U+00A0 - No-Break Space;
    use window height, not buffer;
    added more sequences;
    don't add a newline immediately after a wrap;
    restore cursor visibility on unload.

  v1.71, 23 October, 2015:
    add _CRT_NON_CONFORMING_WCSTOK define for VS2015.

  v1.72, 14 to 24 December, 2015:
    recognize the standard handle defines in WriteFile;
    minor speed improvement by caching GetConsoleMode;
    keep track of three handles (ostensibly stdout, stderr and a file);
    test a DOS header exists before writing to e_oemid;
    more flexible/robust handling of data directories;
    files writing to the console will always succeed;
    log: use API file functions and a custom printf;
	 add a blank line between processes;
	 set function name for MyWriteConsoleA;
    scan imports from "kernel32" (without extension);
    added dynamic environment variable CLICOLOR;
    removed _hwrite (it's the same address as _lwrite);
    join multibyte characters split across separate writes;
    remove wcstok, avoiding potential interference with the host;
    similarly, use a private heap instead of malloc.

  v1.80, 26 October to 17 November, 2017:
    fix unloading;
    revert back to (re)storing buffer cursor position;
    increase cache to five handles;
    hook CreateFile & CreateConsoleScreenBuffer to enable readable handles;
    fix cursor report with duplicated digits (e.g. "11" was just "1");
    preserve escape that isn't part of a sequence;
    fix escape followed by CRM in control mode;
    use the system default sound for the bell.
*/

#include "ansicon.h"
#include "version.h"
#include <tlhelp32.h>
#include <mmsystem.h>

#ifndef SND_SENTRY
#define SND_SENTRY 0x80000
#endif

#define is_digit(c) ('0' <= (c) && (c) <= '9')

// ========== Global variables and constants

HANDLE	hConOut;		// handle to CONOUT$
WORD	orgattr;		// original attributes
DWORD	orgmode;		// original mode
CONSOLE_CURSOR_INFO orgcci;	// original cursor state
HANDLE	hHeap;			// local memory heap
HANDLE	hBell;

#define CACHE	5
struct
{
  HANDLE h;
  DWORD  mode;
} cache[CACHE];

#define ESC	'\x1B'          // ESCape character
#define BEL	'\x07'
#define SO	'\x0E'          // Shift Out
#define SI	'\x0F'          // Shift In

#define MAX_ARG 16		// max number of args in an escape sequence
int   state;			// automata state
TCHAR prefix;			// escape sequence prefix ( '[', ']' or '(' );
TCHAR prefix2;			// secondary prefix ( '?' or '>' );
TCHAR suffix;			// escape sequence suffix
TCHAR suffix2;			// escape sequence secondary suffix
int   es_argc;			// escape sequence args count
int   es_argv[MAX_ARG]; 	// escape sequence args
TCHAR Pt_arg[MAX_PATH*2];	// text parameter for Operating System Command
int   Pt_len;
BOOL  shifted;
int   screen_top = -1;		// initial window top when cleared


// DEC Special Graphics Character Set from
// http://vt100.net/docs/vt220-rm/table2-4.html
// Some of these may not look right, depending on the font and code page (in
// particular, the Control Pictures probably won't work at all).
const WCHAR G1[] =
{
  L'\x00a0',    // _ - No-Break Space
  L'\x2666',    // ` - Black Diamond Suit
  L'\x2592',    // a - Medium Shade
  L'\x2409',    // b - HT
  L'\x240c',    // c - FF
  L'\x240d',    // d - CR
  L'\x240a',    // e - LF
  L'\x00b0',    // f - Degree Sign
  L'\x00b1',    // g - Plus-Minus Sign
  L'\x2424',    // h - NL
  L'\x240b',    // i - VT
  L'\x2518',    // j - Box Drawings Light Up And Left
  L'\x2510',    // k - Box Drawings Light Down And Left
  L'\x250c',    // l - Box Drawings Light Down And Right
  L'\x2514',    // m - Box Drawings Light Up And Right
  L'\x253c',    // n - Box Drawings Light Vertical And Horizontal
  L'\x00af',    // o - SCAN 1 - Macron
  L'\x25ac',    // p - SCAN 3 - Black Rectangle
  L'\x2500',    // q - SCAN 5 - Box Drawings Light Horizontal
  L'_',         // r - SCAN 7 - Low Line
  L'_',         // s - SCAN 9 - Low Line
  L'\x251c',    // t - Box Drawings Light Vertical And Right
  L'\x2524',    // u - Box Drawings Light Vertical And Left
  L'\x2534',    // v - Box Drawings Light Up And Horizontal
  L'\x252c',    // w - Box Drawings Light Down And Horizontal
  L'\x2502',    // x - Box Drawings Light Vertical
  L'\x2264',    // y - Less-Than Or Equal To
  L'\x2265',    // z - Greater-Than Or Equal To
  L'\x03c0',    // { - Greek Small Letter Pi
  L'\x2260',    // | - Not Equal To
  L'\x00a3',    // } - Pound Sign
  L'\x00b7',    // ~ - Middle Dot
};

#define FIRST_G1 '_'
#define LAST_G1  '~'


// color constants

#define FOREGROUND_BLACK 0
#define FOREGROUND_WHITE FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_BLUE

#define BACKGROUND_BLACK 0
#define BACKGROUND_WHITE BACKGROUND_RED|BACKGROUND_GREEN|BACKGROUND_BLUE

const BYTE foregroundcolor[8] =
{
  FOREGROUND_BLACK,			// black foreground
  FOREGROUND_RED,			// red foreground
  FOREGROUND_GREEN,			// green foreground
  FOREGROUND_RED | FOREGROUND_GREEN,	// yellow foreground
  FOREGROUND_BLUE,			// blue foreground
  FOREGROUND_BLUE | FOREGROUND_RED,	// magenta foreground
  FOREGROUND_BLUE | FOREGROUND_GREEN,	// cyan foreground
  FOREGROUND_WHITE			// white foreground
};

const BYTE backgroundcolor[8] =
{
  BACKGROUND_BLACK,			// black background
  BACKGROUND_RED,			// red background
  BACKGROUND_GREEN,			// green background
  BACKGROUND_RED | BACKGROUND_GREEN,	// yellow background
  BACKGROUND_BLUE,			// blue background
  BACKGROUND_BLUE | BACKGROUND_RED,	// magenta background
  BACKGROUND_BLUE | BACKGROUND_GREEN,	// cyan background
  BACKGROUND_WHITE,			// white background
};

const BYTE attr2ansi[8] =		// map console attribute to ANSI number
{
  0,					// black
  4,					// blue
  2,					// green
  6,					// cyan
  1,					// red
  5,					// magenta
  3,					// yellow
  7					// white
};


typedef struct
{
  BYTE	foreground;	// ANSI base color (0 to 7; add 30)
  BYTE	background;	// ANSI base color (0 to 7; add 40)
  BYTE	bold;		// console FOREGROUND_INTENSITY bit
  BYTE	underline;	// console BACKGROUND_INTENSITY bit
  BYTE	rvideo; 	// swap foreground/bold & background/underline
  BYTE	concealed;	// set foreground/bold to background/underline
  BYTE	reverse;	// swap console foreground & background attributes
  BYTE	crm;		// showing control characters?
  COORD SavePos;	// saved cursor position
} STATE, *PSTATE;

PSTATE pState;
HANDLE hMap;

void set_ansicon( PCONSOLE_SCREEN_BUFFER_INFO );


void get_state( void )
{
  TCHAR  buf[64];
  HWND	 hwnd;
  BOOL	 init;
  HANDLE hConOut;
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  static STATE state;	// on the odd chance file mapping fails

  if (pState != NULL)
    return;

  hwnd = GetConsoleWindow();
  if (hwnd == NULL)
    return;

  wsprintf( buf, L"ANSICON_State_%X", PtrToUint( hwnd ) );
  hMap = CreateFileMapping( INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
			    0, sizeof(STATE), buf );
  if (hMap == NULL)
  {
  no_go:
    DEBUGSTR( 1, "File mapping failed (%u) - using default state",
		 GetLastError() );
    pState = &state;
    goto do_init;
  }
  init = (GetLastError() != ERROR_ALREADY_EXISTS);

  pState = MapViewOfFile( hMap, FILE_MAP_ALL_ACCESS, 0, 0, 0 );
  if (pState == NULL)
  {
    CloseHandle( hMap );
    hMap = NULL;
    goto no_go;
  }

  if (init)
  {
  do_init:
    hConOut = CreateFile( L"CONOUT$", GENERIC_READ | GENERIC_WRITE,
				      FILE_SHARE_READ | FILE_SHARE_WRITE,
				      NULL, OPEN_EXISTING, 0, NULL );
    if (!GetConsoleScreenBufferInfo( hConOut, &csbi ))
    {
      DEBUGSTR( 1, "Failed to get screen buffer info (%u) - assuming defaults",
		   GetLastError() );
      csbi.wAttributes	   = 7;
      csbi.dwSize.X	   = 80;
      csbi.dwSize.Y	   = 300;
      csbi.srWindow.Left   = 0;
      csbi.srWindow.Right  = 79;
      csbi.srWindow.Top    = 0;
      csbi.srWindow.Bottom = 24;
    }
    if (GetEnvironmentVariable( L"ANSICON_REVERSE", NULL, 0 ))
    {
      SetEnvironmentVariable( L"ANSICON_REVERSE", NULL );
      pState->reverse	 = TRUE;
      pState->foreground = attr2ansi[(csbi.wAttributes >> 4) & 7];
      pState->background = attr2ansi[csbi.wAttributes & 7];
      pState->bold	 = (csbi.wAttributes & BACKGROUND_INTENSITY) >> 4;
      pState->underline  = (csbi.wAttributes & FOREGROUND_INTENSITY) << 4;
    }
    else
    {
      pState->foreground = attr2ansi[csbi.wAttributes & 7];
      pState->background = attr2ansi[(csbi.wAttributes >> 4) & 7];
      pState->bold	 = csbi.wAttributes & FOREGROUND_INTENSITY;
      pState->underline  = csbi.wAttributes & BACKGROUND_INTENSITY;
    }
    if (!GetEnvironmentVariable( L"ANSICON_DEF", NULL, 0 ))
    {
      TCHAR  def[4];
      LPTSTR a = def;
      if (pState->reverse)
      {
	*a++ = '-';
	csbi.wAttributes = ((csbi.wAttributes >> 4) & 15)
			 | ((csbi.wAttributes & 15) << 4);
      }
      wsprintf( a, L"%X", csbi.wAttributes & 255 );
      SetEnvironmentVariable( L"ANSICON_DEF", def );
    }
    set_ansicon( &csbi );
    CloseHandle( hConOut );
  }
}


// Search an environment variable for a string.
BOOL search_env( LPCTSTR var, LPCTSTR val )
{
  static LPTSTR env;
  static DWORD	env_len;
  DWORD  len;
  BOOL	 not;
  LPTSTR end;

  len = GetEnvironmentVariable( var, env, env_len );
  if (len == 0)
    return FALSE;

  if (len > env_len)
  {
    LPTSTR tmp = (env == NULL) ? HeapAlloc( hHeap, 0, TSIZE(len) )
			       : HeapReAlloc( hHeap, 0, env, TSIZE(len) );
    if (tmp == NULL)
      return FALSE;
    env = tmp;
    env_len = (DWORD)HeapSize( hHeap, 0, env );
    GetEnvironmentVariable( var, env, env_len );
  }

  not = (*env == '!');
  if (not && env[1] == '\0')
    return TRUE;

  end = env + not;
  while (*end != '\0')
  {
    var = end;
    do
    {
      if (*end++ == ';')
      {
	end[-1] = '\0';
	break;
      }
    } while (*end != '\0');
    if (_wcsicmp( val, var ) == 0)
      return !not;
  }

  return not;
}


// ========== Print Buffer functions

#define BUFFER_SIZE 2048

int   nCharInBuffer;
WCHAR ChBuffer[BUFFER_SIZE];
WCHAR ChPrev;
BOOL  fWrapped;

//-----------------------------------------------------------------------------
//   FlushBuffer()
// Writes the buffer to the console and empties it.
//-----------------------------------------------------------------------------

void FlushBuffer( void )
{
  DWORD nWritten;

  if (nCharInBuffer <= 0) return;

  if (pState->crm)
  {
    DWORD mode;
    GetConsoleMode( hConOut, &mode );
    SetConsoleMode( hConOut, mode & ~ENABLE_PROCESSED_OUTPUT );
    WriteConsole( hConOut, ChBuffer, nCharInBuffer, &nWritten, NULL );
    SetConsoleMode( hConOut, mode );
  }
  else
  {
    HANDLE hConWrap;
    CONSOLE_CURSOR_INFO cci;
    CONSOLE_SCREEN_BUFFER_INFO csbi;

    if (nCharInBuffer < 4)
    {
      LPWSTR b = ChBuffer;
      do
      {
	WriteConsole( hConOut, b, 1, &nWritten, NULL );
	if (*b != '\r' && *b != '\b' && *b != '\a')
	{
	  GetConsoleScreenBufferInfo( hConOut, &csbi );
	  if (csbi.dwCursorPosition.X == 0)
	    fWrapped = TRUE;
	}
      } while (++b, --nCharInBuffer);
    }
    else
    {
      // To detect wrapping of multiple characters, create a new buffer, write
      // to the top of it and see if the cursor changes line.  This doesn't
      // always work on the normal buffer, since if you're already on the last
      // line, wrapping scrolls everything up and still leaves you on the last.
      hConWrap = CreateConsoleScreenBuffer( GENERIC_READ|GENERIC_WRITE, 0, NULL,
					    CONSOLE_TEXTMODE_BUFFER, NULL );
      // Even though the buffer isn't visible, the cursor still shows up.
      cci.dwSize = 1;
      cci.bVisible = FALSE;
      SetConsoleCursorInfo( hConWrap, &cci );
      // Ensure the buffer is the same width (it gets created using the window
      // width) and more than one line.
      GetConsoleScreenBufferInfo( hConOut, &csbi );
      csbi.dwSize.Y = csbi.srWindow.Bottom - csbi.srWindow.Top + 2;
      SetConsoleScreenBufferSize( hConWrap, csbi.dwSize );
      // Put the cursor on the top line, in the same column.
      csbi.dwCursorPosition.Y = 0;
      SetConsoleCursorPosition( hConWrap, csbi.dwCursorPosition );
      WriteConsole( hConWrap, ChBuffer, nCharInBuffer, &nWritten, NULL );
      GetConsoleScreenBufferInfo( hConWrap, &csbi );
      if (csbi.dwCursorPosition.Y != 0)
	fWrapped = TRUE;
      CloseHandle( hConWrap );
      WriteConsole( hConOut, ChBuffer, nCharInBuffer, &nWritten, NULL );
    }
  }
  nCharInBuffer = 0;
}

//-----------------------------------------------------------------------------
//   PushBuffer( WCHAR c )
// Adds a character in the buffer.
//-----------------------------------------------------------------------------

void PushBuffer( WCHAR c )
{
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  DWORD nWritten;

  ChPrev = c;

  if (c == '\n')
  {
    if (pState->crm)
      ChBuffer[nCharInBuffer++] = c;
    FlushBuffer();
    // Avoid writing the newline if wrap has already occurred.
    GetConsoleScreenBufferInfo( hConOut, &csbi );
    if (pState->crm)
    {
      // If we're displaying controls, then the only way we can be on the left
      // margin is if wrap occurred.
      if (csbi.dwCursorPosition.X != 0)
	WriteConsole( hConOut, L"\n", 1, &nWritten, NULL );
    }
    else
    {
      LPCWSTR nl = L"\n";
      if (fWrapped)
      {
	// It's wrapped, but was anything more written?  Look at the current
	// row, checking that each character is space in current attributes.
	// If it's all blank we can drop the newline.  If the cursor isn't
	// already at the margin, then it was spaces or tabs that caused the
	// wrap, which can be ignored and overwritten.
	CHAR_INFO blank;
	PCHAR_INFO row;
	row = HeapAlloc( hHeap, 0, csbi.dwSize.X * sizeof(CHAR_INFO) );
	if (row != NULL)
	{
	  COORD s, c;
	  SMALL_RECT r;
	  s.X = csbi.dwSize.X;
	  s.Y = 1;
	  c.X = c.Y = 0;
	  r.Left = 0;
	  r.Right = s.X - 1;
	  r.Top = r.Bottom = csbi.dwCursorPosition.Y;
	  ReadConsoleOutput( hConOut, row, s, c, &r );
	  blank.Char.UnicodeChar = ' ';
	  blank.Attributes = csbi.wAttributes;
	  while (*(PDWORD)&row[c.X] == *(PDWORD)&blank)
	  {
	    if (++c.X == s.X)
	    {
	      nl = (csbi.dwCursorPosition.X == 0) ? NULL : L"\r";
	      break;
	    }
	  }
	  HeapFree( hHeap, 0, row );
	}
	fWrapped = FALSE;
      }
      if (nl)
	WriteConsole( hConOut, nl, 1, &nWritten, NULL );
    }
  }
  else
  {
    if (shifted && c >= FIRST_G1 && c <= LAST_G1)
      c = G1[c-FIRST_G1];
    ChBuffer[nCharInBuffer] = c;
    if (++nCharInBuffer == BUFFER_SIZE)
      FlushBuffer();
  }
}

//-----------------------------------------------------------------------------
//   SendSequence( LPTSTR seq )
// Send the string to the input buffer.
//-----------------------------------------------------------------------------

void SendSequence( LPTSTR seq )
{
  DWORD out;
  PINPUT_RECORD in;
  DWORD len;
  HANDLE hStdIn = GetStdHandle( STD_INPUT_HANDLE );

  in = HeapAlloc( hHeap, HEAP_ZERO_MEMORY, 2 * wcslen( seq ) * sizeof(*in) );
  if (in == NULL)
    return;
  for (len = 0; *seq; len += 2, ++seq)
  {
    in[len+0].EventType =
    in[len+1].EventType = KEY_EVENT;
    in[len+0].Event.KeyEvent.wRepeatCount =
    in[len+1].Event.KeyEvent.wRepeatCount = 1;
    in[len+0].Event.KeyEvent.uChar.UnicodeChar =
    in[len+1].Event.KeyEvent.uChar.UnicodeChar = *seq;
    in[len+0].Event.KeyEvent.bKeyDown = TRUE;
  }
  WriteConsoleInput( hStdIn, in, len, &out );
  HeapFree( hHeap, 0, in );
}

// ========== Print functions

//-----------------------------------------------------------------------------
//   InterpretEscSeq()
// Interprets the last escape sequence scanned by ParseAndPrintString
//   prefix             escape sequence prefix
//   es_argc            escape sequence args count
//   es_argv[]          escape sequence args array
//   suffix             escape sequence suffix
//
// for instance, with \e[33;45;1m we have
// prefix = '[',
// es_argc = 3, es_argv[0] = 33, es_argv[1] = 45, es_argv[2] = 1
// suffix = 'm'
//-----------------------------------------------------------------------------

void InterpretEscSeq( void )
{
  int  i;
  WORD attribut;
  CONSOLE_SCREEN_BUFFER_INFO Info;
  CONSOLE_CURSOR_INFO CursInfo;
  DWORD len, NumberOfCharsWritten;
  COORD Pos;
  SMALL_RECT Rect;
  CHAR_INFO  CharInfo;
  DWORD      mode;

#define WIDTH  Info.dwSize.X
#define CUR    Info.dwCursorPosition
#define WIN    Info.srWindow
#define TOP    WIN.Top
#define BOTTOM WIN.Bottom
#define LEFT   0
#define RIGHT  (WIDTH - 1)

#define FillBlank( len, Pos ) \
  FillConsoleOutputCharacter( hConOut, ' ', len, Pos, &NumberOfCharsWritten );\
  FillConsoleOutputAttribute( hConOut, Info.wAttributes, len, Pos, \
			      &NumberOfCharsWritten )

  if (prefix == '[')
  {
    if (prefix2 == '?' && (suffix == 'h' || suffix == 'l') && es_argc == 1)
    {
      switch (es_argv[0])
      {
	case 25:
	  GetConsoleCursorInfo( hConOut, &CursInfo );
	  CursInfo.bVisible = (suffix == 'h');
	  SetConsoleCursorInfo( hConOut, &CursInfo );
	return;

	case 7:
	  mode = ENABLE_PROCESSED_OUTPUT;
	  if (suffix == 'h')
	    mode |= ENABLE_WRAP_AT_EOL_OUTPUT;
	  SetConsoleMode( hConOut, mode );
	return;
      }
    }
    // Ignore any other private sequences.
    if (prefix2 != 0)
      return;

    GetConsoleScreenBufferInfo( hConOut, &Info );
    switch (suffix)
    {
      case 'm':
	if (es_argc == 0) es_argv[es_argc++] = 0;
	for (i = 0; i < es_argc; i++)
	{
	  if (30 <= es_argv[i] && es_argv[i] <= 37)
	  {
	    pState->foreground = es_argv[i] - 30;
	  }
	  else if (40 <= es_argv[i] && es_argv[i] <= 47)
	  {
	    pState->background = es_argv[i] - 40;
	  }
	  else if (es_argv[i] == 38 || es_argv[i] == 48)
	  {
	    // This is technically incorrect, but it's what xterm does, so
	    // that's what we do.  According to T.416 (ISO 8613-6), there is
	    // only one parameter, which is divided into elements.  So where
	    // xterm does "38;2;R;G;B" it should really be "38;2:I:R:G:B" (I is
	    // a colour space identifier).
	    if (i+1 < es_argc)
	    {
	      if (es_argv[i+1] == 2)		// rgb
		i += 4;
	      else if (es_argv[i+1] == 5)	// index
		i += 2;
	    }
	  }
	  else switch (es_argv[i])
	  {
	    case 0:
	    case 39:
	    case 49:
	    {
	      TCHAR def[4];
	      int   a;
	      *def = '7'; def[1] = '\0';
	      GetEnvironmentVariable( L"ANSICON_DEF", def, lenof(def) );
	      a = wcstol( def, NULL, 16 );
	      pState->reverse = FALSE;
	      if (a < 0)
	      {
		pState->reverse = TRUE;
		a = -a;
	      }
	      if (es_argv[i] != 49)
		pState->foreground = attr2ansi[a & 7];
	      if (es_argv[i] != 39)
		pState->background = attr2ansi[(a >> 4) & 7];
	      if (es_argv[i] == 0)
	      {
		if (es_argc == 1)
		{
		  pState->bold	    = a & FOREGROUND_INTENSITY;
		  pState->underline = a & BACKGROUND_INTENSITY;
		}
		else
		{
		  pState->bold	    = 0;
		  pState->underline = 0;
		}
		pState->rvideo    = 0;
		pState->concealed = 0;
	      }
	    }
	    break;

	    case  1: pState->bold      = FOREGROUND_INTENSITY; break;
	    case  5: // blink
	    case  4: pState->underline = BACKGROUND_INTENSITY; break;
	    case  7: pState->rvideo    = 1; break;
	    case  8: pState->concealed = 1; break;
	    case 21: // oops, this actually turns on double underline
		     // but xterm turns off bold too, so that's alright
	    case 22: pState->bold      = 0; break;
	    case 25:
	    case 24: pState->underline = 0; break;
	    case 27: pState->rvideo    = 0; break;
	    case 28: pState->concealed = 0; break;
	  }
	}
	if (pState->concealed)
	{
	  if (pState->rvideo)
	  {
	    attribut = foregroundcolor[pState->foreground]
		     | backgroundcolor[pState->foreground];
	    if (pState->bold)
	      attribut |= FOREGROUND_INTENSITY | BACKGROUND_INTENSITY;
	  }
	  else
	  {
	    attribut = foregroundcolor[pState->background]
		     | backgroundcolor[pState->background];
	    if (pState->underline)
	      attribut |= FOREGROUND_INTENSITY | BACKGROUND_INTENSITY;
	  }
	}
	else if (pState->rvideo)
	{
	  attribut = foregroundcolor[pState->background]
		   | backgroundcolor[pState->foreground];
	  if (pState->bold)
	    attribut |= BACKGROUND_INTENSITY;
	  if (pState->underline)
	    attribut |= FOREGROUND_INTENSITY;
	}
	else
	  attribut = foregroundcolor[pState->foreground] | pState->bold
		   | backgroundcolor[pState->background] | pState->underline;
	if (pState->reverse)
	  attribut = ((attribut >> 4) & 15) | ((attribut & 15) << 4);
	SetConsoleTextAttribute( hConOut, attribut );
      return;

      case 'J':
	if (es_argc == 0) es_argv[es_argc++] = 0; // ESC[J == ESC[0J
	if (es_argc != 1) return;
	switch (es_argv[0])
	{
	  case 0: // ESC[0J erase from cursor to end of display
	    len = (BOTTOM - CUR.Y) * WIDTH + WIDTH - CUR.X;
	    FillBlank( len, CUR );
	  return;

	  case 1: // ESC[1J erase from start to cursor.
	    Pos.X = 0;
	    Pos.Y = TOP;
	    len   = (CUR.Y - TOP) * WIDTH + CUR.X + 1;
	    FillBlank( len, Pos );
	  return;

	  case 2: // ESC[2J Clear screen and home cursor
	    if (TOP != screen_top || BOTTOM == Info.dwSize.Y - 1)
	    {
	      // Rather than clearing the existing window, make the current
	      // line the new top of the window (assuming this is the first
	      // thing a program does).
	      int range = BOTTOM - TOP;
	      if (CUR.Y + range < Info.dwSize.Y)
	      {
		TOP = CUR.Y;
		BOTTOM = TOP + range;
	      }
	      else
	      {
		BOTTOM = Info.dwSize.Y - 1;
		TOP = BOTTOM - range;
		Rect.Left = LEFT;
		Rect.Right = RIGHT;
		Rect.Top = CUR.Y - TOP;
		Rect.Bottom = CUR.Y - 1;
		Pos.X = Pos.Y = 0;
		CharInfo.Char.UnicodeChar = ' ';
		CharInfo.Attributes = Info.wAttributes;
		ScrollConsoleScreenBuffer(hConOut, &Rect, NULL, Pos, &CharInfo);
	      }
	      SetConsoleWindowInfo( hConOut, TRUE, &WIN );
	      screen_top = TOP;
	    }
	    Pos.X = LEFT;
	    Pos.Y = TOP;
	    len   = (BOTTOM - TOP + 1) * WIDTH;
	    FillBlank( len, Pos );
	    // Not technically correct, but perhaps expected.
	    SetConsoleCursorPosition( hConOut, Pos );
	  return;

	  default:
	  return;
	}

      case 'K':
	if (es_argc == 0) es_argv[es_argc++] = 0; // ESC[K == ESC[0K
	if (es_argc != 1) return;
	switch (es_argv[0])
	{
	  case 0: // ESC[0K Clear to end of line
	    len = WIDTH - CUR.X;
	    FillBlank( len, CUR );
	  return;

	  case 1: // ESC[1K Clear from start of line to cursor
	    Pos.X = LEFT;
	    Pos.Y = CUR.Y;
	    FillBlank( CUR.X + 1, Pos );
	  return;

	  case 2: // ESC[2K Clear whole line.
	    Pos.X = LEFT;
	    Pos.Y = CUR.Y;
	    FillBlank( WIDTH, Pos );
	  return;

	  default:
	  return;
	}

      case 'X': // ESC[#X Erase # characters.
	if (es_argc == 0) es_argv[es_argc++] = 1; // ESC[X == ESC[1X
	if (es_argc != 1) return;
	FillBlank( es_argv[0], CUR );
      return;

      case 'L': // ESC[#L Insert # blank lines.
	if (es_argc == 0) es_argv[es_argc++] = 1; // ESC[L == ESC[1L
	if (es_argc != 1) return;
	Rect.Left   = WIN.Left	= LEFT;
	Rect.Right  = WIN.Right = RIGHT;
	Rect.Top    = CUR.Y;
	Rect.Bottom = BOTTOM;
	Pos.X = LEFT;
	Pos.Y = CUR.Y + es_argv[0];
	CharInfo.Char.UnicodeChar = ' ';
	CharInfo.Attributes = Info.wAttributes;
	ScrollConsoleScreenBuffer( hConOut, &Rect, &WIN, Pos, &CharInfo );
	// Technically should home the cursor, but perhaps not expected.
      return;

      case 'M': // ESC[#M Delete # lines.
	if (es_argc == 0) es_argv[es_argc++] = 1; // ESC[M == ESC[1M
	if (es_argc != 1) return;
	Rect.Left   = WIN.Left	= LEFT;
	Rect.Right  = WIN.Right = RIGHT;
	Rect.Bottom = BOTTOM;
	Rect.Top    = CUR.Y - es_argv[0];
	Pos.X = LEFT;
	Pos.Y = TOP = CUR.Y;
	CharInfo.Char.UnicodeChar = ' ';
	CharInfo.Attributes = Info.wAttributes;
	ScrollConsoleScreenBuffer( hConOut, &Rect, &WIN, Pos, &CharInfo );
	// Technically should home the cursor, but perhaps not expected.
      return;

      case 'P': // ESC[#P Delete # characters.
	if (es_argc == 0) es_argv[es_argc++] = 1; // ESC[P == ESC[1P
	if (es_argc != 1) return;
	Rect.Left   = WIN.Left	= CUR.X;
	Rect.Right  = WIN.Right = RIGHT;
	Pos.X	    = CUR.X - es_argv[0];
	Pos.Y	    =
	Rect.Top    =
	Rect.Bottom = CUR.Y;
	CharInfo.Char.UnicodeChar = ' ';
	CharInfo.Attributes = Info.wAttributes;
	ScrollConsoleScreenBuffer( hConOut, &Rect, &WIN, Pos, &CharInfo );
      return;

      case '@': // ESC[#@ Insert # blank characters.
	if (es_argc == 0) es_argv[es_argc++] = 1; // ESC[@ == ESC[1@
	if (es_argc != 1) return;
	Rect.Left   = WIN.Left	= CUR.X;
	Rect.Right  = WIN.Right = RIGHT;
	Pos.X	    = CUR.X + es_argv[0];
	Pos.Y	    =
	Rect.Top    =
	Rect.Bottom = CUR.Y;
	CharInfo.Char.UnicodeChar = ' ';
	CharInfo.Attributes = Info.wAttributes;
	ScrollConsoleScreenBuffer( hConOut, &Rect, &WIN, Pos, &CharInfo );
      return;

      case 'k': // ESC[#k
      case 'A': // ESC[#A Moves cursor up # lines
	if (es_argc == 0) es_argv[es_argc++] = 1; // ESC[A == ESC[1A
	if (es_argc != 1) return;
	Pos.Y = CUR.Y - es_argv[0];
	if (Pos.Y < TOP) Pos.Y = TOP;
	Pos.X = CUR.X;
	SetConsoleCursorPosition( hConOut, Pos );
      return;

      case 'e': // ESC[#e
      case 'B': // ESC[#B Moves cursor down # lines
	if (es_argc == 0) es_argv[es_argc++] = 1; // ESC[B == ESC[1B
	if (es_argc != 1) return;
	Pos.Y = CUR.Y + es_argv[0];
	if (Pos.Y > BOTTOM) Pos.Y = BOTTOM;
	Pos.X = CUR.X;
	SetConsoleCursorPosition( hConOut, Pos );
      return;

      case 'a': // ESC[#a
      case 'C': // ESC[#C Moves cursor forward # spaces
	if (es_argc == 0) es_argv[es_argc++] = 1; // ESC[C == ESC[1C
	if (es_argc != 1) return;
	Pos.X = CUR.X + es_argv[0];
	if (Pos.X > RIGHT) Pos.X = RIGHT;
	Pos.Y = CUR.Y;
	SetConsoleCursorPosition( hConOut, Pos );
      return;

      case 'j': // ESC[#j
      case 'D': // ESC[#D Moves cursor back # spaces
	if (es_argc == 0) es_argv[es_argc++] = 1; // ESC[D == ESC[1D
	if (es_argc != 1) return;
	Pos.X = CUR.X - es_argv[0];
	if (Pos.X < LEFT) Pos.X = LEFT;
	Pos.Y = CUR.Y;
	SetConsoleCursorPosition( hConOut, Pos );
      return;

      case 'E': // ESC[#E Moves cursor down # lines, column 1.
	if (es_argc == 0) es_argv[es_argc++] = 1; // ESC[E == ESC[1E
	if (es_argc != 1) return;
	Pos.Y = CUR.Y + es_argv[0];
	if (Pos.Y > BOTTOM) Pos.Y = BOTTOM;
	Pos.X = LEFT;
	SetConsoleCursorPosition( hConOut, Pos );
      return;

      case 'F': // ESC[#F Moves cursor up # lines, column 1.
	if (es_argc == 0) es_argv[es_argc++] = 1; // ESC[F == ESC[1F
	if (es_argc != 1) return;
	Pos.Y = CUR.Y - es_argv[0];
	if (Pos.Y < TOP) Pos.Y = TOP;
	Pos.X = LEFT;
	SetConsoleCursorPosition( hConOut, Pos );
      return;

      case '`': // ESC[#`
      case 'G': // ESC[#G Moves cursor column # in current row.
	if (es_argc == 0) es_argv[es_argc++] = 1; // ESC[G == ESC[1G
	if (es_argc != 1) return;
	Pos.X = es_argv[0] - 1;
	if (Pos.X > RIGHT) Pos.X = RIGHT;
	if (Pos.X < LEFT) Pos.X = LEFT;
	Pos.Y = CUR.Y;
	SetConsoleCursorPosition( hConOut, Pos );
      return;

      case 'd': // ESC[#d Moves cursor row #, current column.
	if (es_argc == 0) es_argv[es_argc++] = 1; // ESC[d == ESC[1d
	if (es_argc != 1) return;
	Pos.Y = es_argv[0] - 1;
	if (Pos.Y < TOP) Pos.Y = TOP;
	if (Pos.Y > BOTTOM) Pos.Y = BOTTOM;
	SetConsoleCursorPosition( hConOut, Pos );
      return;

      case 'f': // ESC[#;#f
      case 'H': // ESC[#;#H Moves cursor to line #, column #
	if (es_argc == 0)
	  es_argv[es_argc++] = 1; // ESC[H == ESC[1;1H
	if (es_argc == 1)
	  es_argv[es_argc++] = 1; // ESC[#H == ESC[#;1H
	if (es_argc > 2) return;
	Pos.X = es_argv[1] - 1;
	if (Pos.X < LEFT) Pos.X = LEFT;
	if (Pos.X > RIGHT) Pos.X = RIGHT;
	Pos.Y = es_argv[0] - 1;
	if (Pos.Y < TOP) Pos.Y = TOP;
	if (Pos.Y > BOTTOM) Pos.Y = BOTTOM;
	SetConsoleCursorPosition( hConOut, Pos );
      return;

      case 'I': // ESC[#I Moves cursor forward # tabs
	if (es_argc == 0) es_argv[es_argc++] = 1; // ESC[I == ESC[1I
	if (es_argc != 1) return;
	Pos.Y = CUR.Y;
	Pos.X = (CUR.X & -8) + es_argv[0] * 8;
	if (Pos.X > RIGHT) Pos.X = RIGHT;
	SetConsoleCursorPosition( hConOut, Pos );
      return;

      case 'Z': // ESC[#Z Moves cursor back # tabs
	if (es_argc == 0) es_argv[es_argc++] = 1; // ESC[Z == ESC[1Z
	if (es_argc != 1) return;
	Pos.Y = CUR.Y;
	if ((CUR.X & 7) == 0)
	   Pos.X = CUR.X - es_argv[0] * 8;
	else
	   Pos.X = (CUR.X & -8) - (es_argv[0] - 1) * 8;
	if (Pos.X < LEFT) Pos.X = LEFT;
	SetConsoleCursorPosition( hConOut, Pos );
      return;

      case 'b': // ESC[#b Repeat character
	if (es_argc == 0) es_argv[es_argc++] = 1; // ESC[b == ESC[1b
	if (es_argc != 1) return;
	while (--es_argv[0] >= 0)
	  PushBuffer( ChPrev );
      return;

      case 's': // ESC[s Saves cursor position for recall later
	if (es_argc != 0) return;
	pState->SavePos = CUR;
      return;

      case 'u': // ESC[u Return to saved cursor position
	if (es_argc != 0) return;
	Pos = pState->SavePos;
	if (Pos.X > RIGHT) Pos.X = RIGHT;
	if (Pos.Y > BOTTOM) Pos.Y = BOTTOM;
	SetConsoleCursorPosition( hConOut, Pos );
      return;

      case 'n': // ESC[#n Device status report
	if (es_argc != 1) return; // ESC[n == ESC[0n -> ignored
	switch (es_argv[0])
	{
	  case 5: // ESC[5n Report status
	    SendSequence( L"\33[0n" ); // "OK"
	  return;

	  case 6: // ESC[6n Report cursor position
	  {
	    TCHAR buf[32];
	    wsprintf( buf, L"\33[%d;%dR", CUR.Y - TOP + 1, CUR.X + 1 );
	    SendSequence( buf );
	  }
	  return;

	  default:
	  return;
	}

      case 't': // ESC[#t Window manipulation
	if (es_argc != 1) return;
	if (es_argv[0] == 21)	// ESC[21t Report xterm window's title
	{
	  TCHAR buf[MAX_PATH*2];
	  DWORD len = GetConsoleTitle( buf+3, lenof(buf)-3-2 );
	  // Too bad if it's too big or fails.
	  buf[0] = ESC;
	  buf[1] = ']';
	  buf[2] = 'l';
	  buf[3+len] = ESC;
	  buf[3+len+1] = '\\';
	  buf[3+len+2] = '\0';
	  SendSequence( buf );
	}
      return;

      case 'h': // ESC[#h Set Mode
	if (es_argc == 1 && es_argv[0] == 3)
	  pState->crm = TRUE;
      return;

      case 'l': // ESC[#l Reset Mode
      return;			// ESC[3l is handled during parsing

      default:
      return;
    }
  }
  else // (prefix == ']')
  {
    // Ignore any "private" sequences.
    if (prefix2 != 0)
      return;

    if (es_argc == 1 && (es_argv[0] == 0 || // ESC]0;titleST - icon (ignored) &
			 es_argv[0] == 2))  // ESC]2;titleST - window
    {
      SetConsoleTitle( Pt_arg );
    }
  }
}

DWORD WINAPI BellThread( LPVOID param )
{
  // XP doesn't support SND_SENTRY, so if it fails, try without.
  if (!PlaySound( (LPTSTR)SND_ALIAS_SYSTEMDEFAULT, NULL,
		  SND_SENTRY | SND_ALIAS_ID | SND_SYNC ))
    PlaySound( (LPTSTR)SND_ALIAS_SYSTEMDEFAULT, NULL, SND_ALIAS_ID | SND_SYNC );
  CloseHandle( hBell );
  hBell = NULL;
  return 0;
}

//-----------------------------------------------------------------------------
//   ParseAndPrintString(hDev, lpBuffer, nNumberOfBytesToWrite)
// Parses the string lpBuffer, interprets the escapes sequences and prints the
// characters in the device hDev (console).
// The lexer is a three states automata.
// If the number of arguments es_argc > MAX_ARG, only the MAX_ARG-1 firsts and
// the last arguments are processed (no es_argv[] overflow).
//-----------------------------------------------------------------------------

BOOL
ParseAndPrintString( HANDLE hDev,
		     LPCVOID lpBuffer,
		     DWORD nNumberOfBytesToWrite,
		     LPDWORD lpNumberOfBytesWritten
		     )
{
  DWORD   i;
  LPCTSTR s;

  if (hDev != hConOut)	// reinit if device has changed
  {
    hConOut = hDev;
    state = 1;
    shifted = FALSE;
  }
  for (i = nNumberOfBytesToWrite, s = (LPCTSTR)lpBuffer; i > 0; i--, s++)
  {
    int c = *s; 		// more efficient to use int than short, fwiw

    if (state == 1)
    {
      if (c == ESC)
      {
	suffix2 = 0;
	get_state();
	state = (pState->crm) ? 7 : 2;
      }
      else if (c == BEL)
      {
	if (hBell == NULL)
	  hBell = CreateThread( NULL, 4096, BellThread, NULL, 0, NULL );
      }
      else if (c == SO) shifted = TRUE;
      else if (c == SI) shifted = FALSE;
      else PushBuffer( (WCHAR)c );
    }
    else if (state == 2)
    {
      if (c == ESC)
	PushBuffer( ESC );
      else if (c >= '\x20' && c <= '\x2f')
	suffix2 = c;
      else if (suffix2 != 0)
	state = 1;
      else if (c == '[' ||      // CSI Control Sequence Introducer
	       c == ']')        // OSC Operating System Command
      {
	FlushBuffer();
	prefix = c;
	prefix2 = 0;
	Pt_len = 0;
	*Pt_arg = '\0';
	state = 3;
      }
      else if (c == 'P' ||      // DCS Device Control String
	       c == 'X' ||      // SOS Start Of String
	       c == '^' ||      // PM  Privacy Message
	       c == '_')        // APC Application Program Command
      {
	*Pt_arg = '\0';
	state = 6;
      }
      else
      {
	PushBuffer( ESC );
	PushBuffer( (WCHAR)c );
	state = 1;
      }
    }
    else if (state == 3)
    {
      if (is_digit( c ))
      {
        es_argc = 0;
	es_argv[0] = c - '0';
        state = 4;
      }
      else if (c == ';')
      {
        es_argc = 1;
        es_argv[0] = 0;
	es_argv[1] = 0;
        state = 4;
      }
      else if (c == ':')
      {
	// ignore it
      }
      else if (c >= '\x3b' && c <= '\x3f')
      {
	prefix2 = c;
      }
      else if (c >= '\x20' && c <= '\x2f')
      {
	suffix2 = c;
      }
      else if (suffix2 != 0)
      {
	state = 1;
      }
      else
      {
        es_argc = 0;
        suffix = c;
        InterpretEscSeq();
        state = 1;
      }
    }
    else if (state == 4)
    {
      if (is_digit( c ))
      {
	es_argv[es_argc] = 10 * es_argv[es_argc] + (c - '0');
      }
      else if (c == ';')
      {
        if (es_argc < MAX_ARG-1) es_argc++;
	es_argv[es_argc] = 0;
	if (prefix == ']')
	  state = 5;
      }
      else if (c >= '\x3a' && c <= '\x3f')
      {
	// ignore 'em
      }
      else if (c >= '\x20' && c <= '\x2f')
      {
	suffix2 = c;
      }
      else if (suffix2 != 0)
      {
	state = 1;
      }
      else
      {
	es_argc++;
        suffix = c;
        InterpretEscSeq();
        state = 1;
      }
    }
    else if (state == 5)
    {
      if (c == BEL)
      {
	Pt_arg[Pt_len] = '\0';
        InterpretEscSeq();
        state = 1;
      }
      else if (c == '\\' && Pt_len > 0 && Pt_arg[Pt_len-1] == ESC)
      {
	Pt_arg[--Pt_len] = '\0';
        InterpretEscSeq();
        state = 1;
      }
      else if (Pt_len < lenof(Pt_arg)-1)
	Pt_arg[Pt_len++] = c;
    }
    else if (state == 6)
    {
      if (c == BEL || (c == '\\' && *Pt_arg == ESC))
	state = 1;
      else
	*Pt_arg = c;
    }
    else if (state == 7)
    {
      if (c == '[') state = 8;
      else
      {
	PushBuffer( ESC );
	if (c != ESC)
	{
	  PushBuffer( (WCHAR)c );
	  state = 1;
	}
      }
    }
    else if (state == 8)
    {
      if (c == '3') state = 9;
      else
      {
	PushBuffer( ESC );
	PushBuffer( '[' );
	PushBuffer( (WCHAR)c );
	state = 1;
      }
    }
    else if (state == 9)
    {
      if (c == 'l') pState->crm = FALSE;
      else
      {
	PushBuffer( ESC );
	PushBuffer( '[' );
	PushBuffer( '3' );
	PushBuffer( (WCHAR)c );
      }
      state = 1;
    }
  }
  FlushBuffer();
  if (lpNumberOfBytesWritten != NULL)
    *lpNumberOfBytesWritten = nNumberOfBytesToWrite - i;
  return (i == 0);
}


// ========== Hooking API functions
//
// References about API hooking (and dll injection):
// - Matt Pietrek ~ Windows 95 System Programming Secrets.
// - Jeffrey Richter ~ Programming Applications for Microsoft Windows 4th ed.

const char APIKernel[]		   = "kernel32.dll";
const char APIConsole[] 	   = "API-MS-Win-Core-Console-";
const char APIProcessThreads[]	   = "API-MS-Win-Core-ProcessThreads-";
const char APIProcessEnvironment[] = "API-MS-Win-Core-ProcessEnvironment-";
const char APILibraryLoader[]	   = "API-MS-Win-Core-LibraryLoader-";
const char APIFile[]		   = "API-MS-Win-Core-File-";

typedef struct
{
  PCSTR   name;
  DWORD   len;
  HMODULE base;
} API_DATA, *PAPI_DATA;

API_DATA APIs[] =
{
  { APIConsole, 	   sizeof(APIConsole) - 1,	      NULL },
  { APIProcessThreads,	   sizeof(APIProcessThreads) - 1,     NULL },
  { APIProcessEnvironment, sizeof(APIProcessEnvironment) - 1, NULL },
  { APILibraryLoader,	   sizeof(APILibraryLoader) - 1,      NULL },
  { APIFile,		   sizeof(APIFile) - 1, 	      NULL },
  { NULL,		   0,				      NULL }
};


HMODULE   hKernel;		// Kernel32 module handle
HINSTANCE hDllInstance; 	// Dll instance handle
#if defined(_WIN64) || defined(W32ON64)
LPTSTR	  DllNameType;		// pointer to process type within DllName
#endif

typedef struct
{
  PCSTR lib;
  PSTR	name;
  PROC	newfunc;
  PROC	oldfunc;
  PROC	apifunc;
  PULONG_PTR myimport;
} HookFn, *PHookFn;

HookFn Hooks[];

const char zIgnoring[]	= "Ignoring";
const char zScanning[]	= "Scanning";
const char zSkipping[]	= "Skipping";
const char zHooking[]	= "Hooking";
const char zUnhooking[] = "Unhooking";


//-----------------------------------------------------------------------------
//   HookAPIOneMod
// Substitute a new function in the Import Address Table (IAT) of the
// specified module.
// Return FALSE on error and TRUE on success.
//-----------------------------------------------------------------------------

BOOL HookAPIOneMod(
    HMODULE hFromModule,	// Handle of the module to intercept calls from
    PHookFn Hooks,		// Functions to replace
    BOOL    restore,		// Restore the original functions
    LPCSTR  sp			// Logging indentation
    )
{
  PIMAGE_DOS_HEADER	   pDosHeader;
  PIMAGE_NT_HEADERS	   pNTHeader;
  PIMAGE_IMPORT_DESCRIPTOR pImportDesc;
  PIMAGE_THUNK_DATA	   pThunk;
  PHookFn		   hook;
  BOOL			   self;

  if (hFromModule == NULL)
  {
    self = TRUE;
    hFromModule = hDllInstance;
  }
  else
    self = FALSE;

  // Tests to make sure we're looking at a module image (the 'MZ' header)
  pDosHeader = (PIMAGE_DOS_HEADER)hFromModule;
  if (pDosHeader->e_magic != IMAGE_DOS_SIGNATURE)
  {
    DEBUGSTR( 1, "Image has no DOS header!" );
    return FALSE;
  }

  // The MZ header has a pointer to the PE header
  pNTHeader = MakeVA( PIMAGE_NT_HEADERS, pDosHeader->e_lfanew );

  // One more test to make sure we're looking at a "PE" image
  if (pNTHeader->Signature != IMAGE_NT_SIGNATURE)
  {
    DEBUGSTR( 1, "Image has no NT header!" );
    return FALSE;
  }

  // We now have a valid pointer to the module's PE header.
  // Get a pointer to its imports section.
  pImportDesc = MakeVA( PIMAGE_IMPORT_DESCRIPTOR,
			pNTHeader->IMPORTDIR.VirtualAddress );

  // Bail out if the RVA of the imports section is 0 (it doesn't exist)
  if (pImportDesc == (PIMAGE_IMPORT_DESCRIPTOR)pDosHeader)
    return TRUE;

  // Iterate through the array of imported module descriptors, looking
  // for the module whose name matches the pszFunctionModule parameter.
  for (; pImportDesc->Name; pImportDesc++)
  {
    BOOL kernel = TRUE;
    PSTR pszModName = MakeVA( PSTR, pImportDesc->Name );
    if (_strnicmp( pszModName, APIKernel, 8 ) != 0 ||
	(_stricmp( pszModName+8, APIKernel+8 ) != 0 && pszModName[8] != '\0'))
    {
      PAPI_DATA lib;
      for (lib = APIs; lib->name; ++lib)
      {
	if (_strnicmp( pszModName, lib->name, lib->len ) == 0)
	{
	  if (lib->base == NULL)
	  {
	    lib->base = GetModuleHandleA( pszModName );
	    for (hook = Hooks; hook->name; ++hook)
	      if (hook->lib == lib->name)
		hook->apifunc = GetProcAddress( lib->base, hook->name );
	  }
	  break;
	}
      }
      if (lib->name == NULL)
      {
	if (log_level & 16)
	  DEBUGSTR( 2, " %s%s %s", sp, zIgnoring, pszModName );
	continue;
      }
      kernel = FALSE;
    }
    if (log_level & 16)
      DEBUGSTR( 2, " %s%s %s", sp, zScanning, pszModName );

    // Get a pointer to the found module's import address table (IAT).
    pThunk = MakeVA( PIMAGE_THUNK_DATA, pImportDesc->FirstThunk );

    // Blast through the table of import addresses, looking for the ones
    // that match the original addresses.
    while (pThunk->u1.Function)
    {
      for (hook = Hooks; hook->name; ++hook)
      {
	PROC patch = 0;
	if (restore)
	{
	  if ((PROC)pThunk->u1.Function == hook->newfunc)
	    patch = (kernel) ? hook->oldfunc : hook->apifunc;
	}
	else if ((PROC)pThunk->u1.Function == hook->oldfunc ||
		 (PROC)pThunk->u1.Function == hook->apifunc)
	{
	  if (self)
	  {
	    hook->myimport = &pThunk->u1.Function;
	    DEBUGSTR( 3, "  %s%s", sp, hook->name );
	  }
	  else
	  {
	    // Don't hook if our import already points to the module being
	    // hooked (i.e. it's already hooked us).
	    MEMORY_BASIC_INFORMATION minfo;
	    VirtualQuery( (LPVOID)*hook->myimport, &minfo, sizeof(minfo) );
	    if (minfo.AllocationBase != hFromModule)
	      patch = hook->newfunc;
	  }
	}
	if (patch)
	{
	  DWORD pr;

	  DEBUGSTR( 3, "  %s%s", sp, hook->name );
	  // Change the access protection on the region of committed pages in
	  // the virtual address space of the current process.
	  VirtualProtect( &pThunk->u1.Function, PTRSZ, PAGE_READWRITE, &pr );

	  // Overwrite the original address with the address of the new function.
	  pThunk->u1.Function = (DWORD_PTR)patch;

	  // Put the page attributes back the way they were.
	  VirtualProtect( &pThunk->u1.Function, PTRSZ, pr, &pr );
	}
      }
      pThunk++; // Advance to next imported function address
    }
  }

  return TRUE;	// Function not found
}

//-----------------------------------------------------------------------------
//   HookAPIAllMod
// Substitute a new function in the Import Address Table (IAT) of all
// the modules in the current process.
// Return FALSE on error and TRUE on success.
//-----------------------------------------------------------------------------

BOOL HookAPIAllMod( PHookFn Hooks, BOOL restore, BOOL indent )
{
  HANDLE	hModuleSnap;
  MODULEENTRY32 me;
  BOOL		fOk;
  LPCSTR	op, sp;
  DWORD 	pr;

  // Take a snapshot of all modules in the current process.
  hModuleSnap = CreateToolhelp32Snapshot( TH32CS_SNAPMODULE,
					  GetCurrentProcessId() );

  if (hModuleSnap == INVALID_HANDLE_VALUE)
  {
    DEBUGSTR( 1, "Failed to create snapshot (%u)", GetLastError() );
    return FALSE;
  }

  op = (restore) ? zUnhooking : zHooking;
  sp = (indent) ? "  " : "";

  // Fill the size of the structure before using it.
  me.dwSize = sizeof(MODULEENTRY32);

  // Walk the module list of the modules.
  for (fOk = Module32First( hModuleSnap, &me ); fOk;
       fOk = Module32Next( hModuleSnap, &me ))
  {
    // We don't hook functions in our own module.
    if (me.hModule == hDllInstance || me.hModule == hKernel)
      continue;

    if (!restore)
    {
      // Don't scan what we've already scanned.
      if (*(PDWORD)((PBYTE)me.hModule + 36) == 'ISNA')    // e_oemid, e_oeminfo
      {
	if (log_level & 16)
	  DEBUGSTR( 2, "%s%s %S", sp, zSkipping, me.szModule );
	continue;
      }
      // It's possible for the PE header to be inside the DOS header.
      if (*(PDWORD)((PBYTE)me.hModule + 0x3C) >= 0x40)
      {
	VirtualProtect( (PBYTE)me.hModule + 36, 4, PAGE_READWRITE, &pr );
	*(PDWORD)((PBYTE)me.hModule + 36) = 'ISNA';
	VirtualProtect( (PBYTE)me.hModule + 36, 4, pr, &pr );
      }
    }
    else
    {
      if (*(PDWORD)((PBYTE)me.hModule + 36) != 'ISNA' &&
	  *(PDWORD)((PBYTE)me.hModule + 0x3C) >= 0x40)
      {
	if (log_level & 16)
	  DEBUGSTR( 2, "%s%s %S", sp, zSkipping, me.szModule );
	continue;
      }
    }
    if (search_env( L"ANSICON_EXC", me.szModule ))
    {
      DEBUGSTR( 2, "%s%s %S", sp, zIgnoring, me.szModule );
      continue;
    }

    // Hook the functions in this module.
    DEBUGSTR( 2, "%s%s %S", sp, op, me.szModule );
    if (!HookAPIOneMod( me.hModule, Hooks, restore, sp ))
    {
      CloseHandle( hModuleSnap );
      return FALSE;
    }
  }
  CloseHandle( hModuleSnap );
  DEBUGSTR( 2, "%s%s completed", sp, op );
  return TRUE;
}


// ========== Child process injection

#define MAX_DEV_PATH (32+MAX_PATH)	// device form instead of drive letter

static LPTSTR get_program( LPTSTR app, HANDLE hProcess,
			   BOOL wide, LPCVOID lpApp, LPCVOID lpCmd )
{
  app[MAX_DEV_PATH-1] = '\0';

  if (lpApp == NULL)
  {
    typedef DWORD (WINAPI *PGPIFNW)( HANDLE, LPTSTR, DWORD );
    static PGPIFNW GetProcessImageFileName;

    if (GetProcessImageFileName == NULL)
    {
      // Use Ex to avoid potential recursion with other hooks.
      HMODULE psapi = LoadLibraryEx( L"psapi.dll", NULL, 0 );
      if (psapi != NULL)
      {
	GetProcessImageFileName = (PGPIFNW)GetProcAddress( psapi,
						  "GetProcessImageFileNameW" );
      }
      if (GetProcessImageFileName == NULL)
	GetProcessImageFileName = INVALID_HANDLE_VALUE;
    }
    if (GetProcessImageFileName == INVALID_HANDLE_VALUE ||
	GetProcessImageFileName( hProcess, app, MAX_DEV_PATH ) == 0)
    {
      LPTSTR  name;
      LPCTSTR term = L" \t";

      if (wide)
      {
	LPCTSTR pos;
	for (pos = lpCmd; *pos == ' ' || *pos == '\t'; ++pos) ;
	if (*pos == '"')
	{
	  term = L"\"";
	  ++pos;
	}
	wcsncpy( app, pos, MAX_DEV_PATH-1 );
      }
      else
      {
	LPCSTR pos;
	for (pos = lpCmd; *pos == ' ' || *pos == '\t'; ++pos) ;
	if (*pos == '"')
	{
	  term = L"\"";
	  ++pos;
	}
	MultiByteToWideChar( CP_ACP, 0, pos, -1, app, MAX_DEV_PATH-1 );
      }
      // CreateProcess only works with surrounding quotes ('"a name"' works,
      // but 'a" "name' fails), so that's all I'll test, too.  However, it also
      // tests for a file at each separator ('a name' tries "a.exe" before
      // "a name.exe") which I won't do.
      name = wcspbrk( app, term );
      if (name != NULL)
	*name = '\0';
    }
  }
  else
  {
    if (wide)
      wcsncpy( app, lpApp, MAX_DEV_PATH-1 );
    else
      MultiByteToWideChar( CP_ACP, 0, lpApp, -1, app, MAX_DEV_PATH-1 );
  }
  return get_program_name( app );
}


// Inject code into the target process to load our DLL.
void Inject( DWORD dwCreationFlags, LPPROCESS_INFORMATION lpi,
	     LPPROCESS_INFORMATION child_pi,
	     BOOL wide, LPCVOID lpApp, LPCVOID lpCmd )
{
  int	 type;
  PBYTE  base;
  BOOL	 gui;
  WCHAR  app[MAX_DEV_PATH];
  LPTSTR name;

  name = get_program( app, child_pi->hProcess, wide, lpApp, lpCmd );
  DEBUGSTR( 1, "%S (%u)", name, child_pi->dwProcessId );
  if (search_env( L"ANSICON_EXC", name ))
  {
    DEBUGSTR( 1, "  Excluded" );
    type = 0;
  }
  else
  {
    type = ProcessType( child_pi, &base, &gui );
    if (gui && type > 0)
    {
      if (!search_env( L"ANSICON_GUI", name ))
      {
	DEBUGSTR( 1, "  %s", zIgnoring );
	type = 0;
      }
    }
  }
  if (type > 0)
  {
#ifdef _WIN64
    if (type == 64)
    {
      ansi_bits[0] = '6';
      ansi_bits[1] = '4';
      InjectDLL( child_pi, base );
    }
    else if (type == 32)
    {
      ansi_bits[0] = '3';
      ansi_bits[1] = '2';
      InjectDLL32( child_pi, base );
    }
    else // (type == 48)
    {
      InjectDLL64( child_pi );
    }
#else
#ifdef W32ON64
    if (type != 32)
    {
      TCHAR args[64];
      STARTUPINFO si;
      PROCESS_INFORMATION pi;
      wcscpy( DllNameType, L"CON.exe" );
      wsprintf( args, L"ansicon -P%ld", child_pi->dwProcessId );
      ZeroMemory( &si, sizeof(si) );
      si.cb = sizeof(si);
      if (CreateProcess( DllName, args, NULL, NULL, FALSE, 0, NULL, NULL,
			 &si, &pi ))
      {
	WaitForSingleObject( pi.hProcess, INFINITE );
	CloseHandle( pi.hProcess );
	CloseHandle( pi.hThread );
      }
      else
	DEBUGSTR( 1, "Could not execute %\"S (%u)", DllName, GetLastError() );
      wcscpy( DllNameType, L"32.dll" );
    }
    else
#endif
    InjectDLL( child_pi, base );
#endif
  }

  if (!(dwCreationFlags & CREATE_SUSPENDED))
    ResumeThread( child_pi->hThread );

  if (lpi != NULL)
  {
    memcpy( lpi, child_pi, sizeof(PROCESS_INFORMATION) );
  }
  else
  {
    CloseHandle( child_pi->hThread );
    CloseHandle( child_pi->hProcess );
  }
}


BOOL WINAPI MyCreateProcessA( LPCSTR lpApplicationName,
			      LPSTR lpCommandLine,
			      LPSECURITY_ATTRIBUTES lpThreadAttributes,
			      LPSECURITY_ATTRIBUTES lpProcessAttributes,
			      BOOL bInheritHandles,
			      DWORD dwCreationFlags,
			      LPVOID lpEnvironment,
			      LPCSTR lpCurrentDirectory,
			      LPSTARTUPINFOA lpStartupInfo,
			      LPPROCESS_INFORMATION lpProcessInformation )
{
  PROCESS_INFORMATION child_pi;

  DEBUGSTR( 1, "CreateProcessA: %\"s, %#s", lpApplicationName, lpCommandLine );

  // May need to initialise the state, to propagate environment variables.
  get_state();
  if (!CreateProcessA( lpApplicationName,
		       lpCommandLine,
		       lpThreadAttributes,
		       lpProcessAttributes,
		       bInheritHandles,
		       dwCreationFlags | CREATE_SUSPENDED,
		       lpEnvironment,
		       lpCurrentDirectory,
		       lpStartupInfo,
		       &child_pi ))
  {
    DEBUGSTR( 1, "  Failed (%u)", GetLastError() );
    return FALSE;
  }

  Inject( dwCreationFlags, lpProcessInformation, &child_pi,
	  FALSE, lpApplicationName, lpCommandLine );

  return TRUE;
}


BOOL WINAPI MyCreateProcessW( LPCWSTR lpApplicationName,
			      LPWSTR lpCommandLine,
			      LPSECURITY_ATTRIBUTES lpThreadAttributes,
			      LPSECURITY_ATTRIBUTES lpProcessAttributes,
			      BOOL bInheritHandles,
			      DWORD dwCreationFlags,
			      LPVOID lpEnvironment,
			      LPCWSTR lpCurrentDirectory,
			      LPSTARTUPINFOW lpStartupInfo,
			      LPPROCESS_INFORMATION lpProcessInformation )
{
  PROCESS_INFORMATION child_pi;

  DEBUGSTR( 1, "CreateProcessW: %\"S, %#S", lpApplicationName, lpCommandLine );

  get_state();
  if (!CreateProcessW( lpApplicationName,
		       lpCommandLine,
		       lpThreadAttributes,
		       lpProcessAttributes,
		       bInheritHandles,
		       dwCreationFlags | CREATE_SUSPENDED,
		       lpEnvironment,
		       lpCurrentDirectory,
		       lpStartupInfo,
		       &child_pi ))
  {
    DEBUGSTR( 1, "  Failed (%u)", GetLastError() );
    return FALSE;
  }

  Inject( dwCreationFlags, lpProcessInformation, &child_pi,
	  TRUE, lpApplicationName, lpCommandLine );

  return TRUE;
}


FARPROC WINAPI MyGetProcAddress( HMODULE hModule, LPCSTR lpProcName )
{
  PHookFn hook;
  FARPROC proc;

  proc = GetProcAddress( hModule, lpProcName );

  if (proc != NULL)
  {
    if (hModule == hKernel)
    {
      // Ignore LoadLibrary so other hooks continue to work (our version
      // might end up at a different address).
      if (proc == Hooks[0].oldfunc || proc == Hooks[1].oldfunc)
      {
	DEBUGSTR( 3, "GetProcAddress: %s (ignoring)", lpProcName );
	return proc;
      }
      for (hook = Hooks + 2; hook->name; ++hook)
      {
	if (proc == hook->oldfunc)
	{
	  DEBUGSTR( 3, "GetProcAddress: %s", lpProcName );
	  return hook->newfunc;
	}
      }
    }
    else
    {
      PAPI_DATA api;
      for (api = APIs; api->name; ++api)
      {
	if (hModule == api->base)
	{
	  if (proc == Hooks[0].apifunc || proc == Hooks[1].apifunc)
	  {
	    DEBUGSTR( 3, "GetProcAddress: %s (ignoring)", lpProcName );
	    return proc;
	  }
	  for (hook = Hooks + 2; hook->name; ++hook)
	  {
	    if (proc == hook->apifunc)
	    {
	      DEBUGSTR( 3, "GetProcAddress: %s", lpProcName );
	      return hook->newfunc;
	    }
	  }
	  break;
	}
      }
    }
  }

  return proc;
}


HMODULE WINAPI MyLoadLibraryA( LPCSTR lpFileName )
{
  HMODULE hMod = LoadLibraryA( lpFileName );
  DEBUGSTR( 2, "LoadLibraryA %s", lpFileName );
  HookAPIAllMod( Hooks, FALSE, TRUE );
  return hMod;
}


HMODULE WINAPI MyLoadLibraryW( LPCWSTR lpFileName )
{
  HMODULE hMod = LoadLibraryW( lpFileName );
  DEBUGSTR( 2, "LoadLibraryW %S", lpFileName );
  HookAPIAllMod( Hooks, FALSE, TRUE );
  return hMod;
}


HMODULE WINAPI MyLoadLibraryExA( LPCSTR lpFileName, HANDLE hFile,
				 DWORD dwFlags )
{
  HMODULE hMod = LoadLibraryExA( lpFileName, hFile, dwFlags );
  if (!(dwFlags & (LOAD_LIBRARY_AS_DATAFILE |
		   LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE |
		   LOAD_LIBRARY_AS_IMAGE_RESOURCE)))
  {
    DEBUGSTR( 2, "LoadLibraryExA %s", lpFileName );
    HookAPIAllMod( Hooks, FALSE, TRUE );
  }
  return hMod;
}


HMODULE WINAPI MyLoadLibraryExW( LPCWSTR lpFileName, HANDLE hFile,
				 DWORD dwFlags )
{
  HMODULE hMod = LoadLibraryExW( lpFileName, hFile, dwFlags );
  if (!(dwFlags & (LOAD_LIBRARY_AS_DATAFILE |
		   LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE |
		   LOAD_LIBRARY_AS_IMAGE_RESOURCE)))
  {
    DEBUGSTR( 2, "LoadLibraryExW %S", lpFileName );
    HookAPIAllMod( Hooks, FALSE, TRUE );
  }
  return hMod;
}


//-----------------------------------------------------------------------------
//   IsConsoleHandle
// Determine if the handle is writing to the console, with processed output.
//-----------------------------------------------------------------------------
BOOL IsConsoleHandle( HANDLE h )
{
  int c;

  for (c = 0; c < CACHE; ++c)
    if (cache[c].h == h)
      return (cache[c].mode & ENABLE_PROCESSED_OUTPUT);

  while (--c > 0)
    cache[c] = cache[c-1];

  cache[0].h = h;
  cache[0].mode = 0;
  if (!GetConsoleMode( h, &cache[0].mode ))
  {
    // GetConsoleMode could fail if the console was not opened for reading
    // (which is what Microsoft's conio output does).  Verify the handle with
    // WriteConsole (processed output is the default).
    DWORD written;
    if (WriteConsole( h, NULL, 0, &written, NULL ))
      cache[0].mode = ENABLE_PROCESSED_OUTPUT;
  }

  return (cache[0].mode & ENABLE_PROCESSED_OUTPUT);
}

//-----------------------------------------------------------------------------
//   MySetConsoleMode
// It seems GetConsoleMode is a relatively slow function, so call it once and
// keep track of changes directly.
//-----------------------------------------------------------------------------
BOOL
WINAPI MySetConsoleMode( HANDLE hCon, DWORD mode )
{
  BOOL rc = SetConsoleMode( hCon, mode );
  if (rc)
  {
    int c;
    for (c = 0; c < CACHE; ++c)
    {
      // The mode is associated with the buffer, not the handle.
      GetConsoleMode( cache[c].h, &cache[c].mode );
    }
  }
  return rc;
}


//-----------------------------------------------------------------------------
//   MyWrite...
// The new functions that must replace the original Write... functions.  These
// functions have exactly the same signature as the original ones.  This
// module is not hooked, so we can still call the original functions ourselves.
//-----------------------------------------------------------------------------

static LPCSTR write_func;

BOOL
WINAPI MyWriteConsoleA( HANDLE hCon, LPCVOID lpBuffer,
			DWORD nNumberOfCharsToWrite,
			LPDWORD lpNumberOfCharsWritten, LPVOID lpReserved )
{
  LPWSTR buf;
  WCHAR  wBuf[1024];
  DWORD  len, wlen;
  UINT	 cp;
  BOOL	 rc = TRUE;
  LPCSTR aBuf;
  static char  mb[4];
  static DWORD mb_len, mb_size;

  if (nNumberOfCharsToWrite != 0 && IsConsoleHandle( hCon ))
  {
    DEBUGSTR( 4, "%s: %u %\"<s",
		 write_func == NULL ? "WriteConsoleA" : write_func,
		 nNumberOfCharsToWrite, lpBuffer );
    write_func = NULL;
    aBuf = lpBuffer;
    len = nNumberOfCharsToWrite;
    wlen = 0;
    cp = GetConsoleOutputCP();
    // How to determine a multibyte character set?  Cmd.Exe has IsDBCSCodePage,
    // which tests code page numbers; ConHost has IsAvailableFarEastCodePage,
    // which uses TranslateCharsetInfo; I used GetCPInfo in CMDRead.  Let's use
    // IsDBCSCodePage, as that avoids another API call.
    if (cp == 932 || cp == 936 || cp == 949 || cp == 950)
    {
      if (mb_len == 1)
      {
	mb[1] = *aBuf++;
	--len;
	DEBUGSTR( 4, "  %strail byte, removing & writing %\"*s",
		     (len == 0) ? "" : "starts with a ", 2, mb );
	wlen = MultiByteToWideChar( cp, 0, mb, 2, wBuf, lenof(wBuf) );
	ParseAndPrintString( hCon, wBuf, wlen, NULL );
	mb_len = 0;
      }
      // A lead byte might also be a trail byte, so count all consecutive lead
      // bytes - an even number means complete pairs, whilst an odd number
      // means the last lead byte has been split.
      if (len != 0 && IsDBCSLeadByteEx( cp, aBuf[len-1] ))
      {
	int lead = 1;
	int pos = len - 1;
	while (--pos >= 0 && IsDBCSLeadByteEx( cp, aBuf[pos] ))
	  ++lead;
	if (lead & 1)
	{
	  mb[mb_len++] = aBuf[--len];
	  DEBUGSTR( 4, "  %slead byte, removing",
		       (len == 0) ? "" : "ends with a " );
	}
      }
    }
    else if (cp == CP_UTF8)
    {
      if (mb_len != 0)
      {
	while ((*aBuf & 0xC0) == 0x80)
	{
	  mb[mb_len++] = *aBuf++;
	  --len;
	  if (mb_len == mb_size)
	    break;
	  if (len == 0)
	  {
	    DEBUGSTR( 4, "  trail byte%s, removing",
			 (nNumberOfCharsToWrite == 1) ? "" : "s" );
	    if (lpNumberOfCharsWritten != NULL)
	      *lpNumberOfCharsWritten = 0;
	    goto check_written;
	  }
	}
	if (log_level & 4)
	{
	  DWORD tlen = nNumberOfCharsToWrite - len;
	  if (tlen == 0)
	    DEBUGSTR( 4, "  incomplete UTF-8 sequence, writing %\"*s",
			 mb_len, mb );
	  else if (len == 0)
	    DEBUGSTR( 4, "  trail byte%s, removing & writing %\"*s",
			 (tlen == 1) ? "" : "s", mb_len, mb );
	  else if (tlen == 1)
	    DEBUGSTR( 4, "  starts with a trail byte, removing & writing %\"*s",
			 mb_len, mb );
	  else
	    DEBUGSTR( 4, "  starts with %u trail bytes, removing & writing %\"*s",
			 tlen, mb_len, mb );
	}
	wlen = MultiByteToWideChar( cp, 0, mb, mb_len, wBuf, lenof(wBuf) );
	ParseAndPrintString( hCon, wBuf, wlen, NULL );
	mb_len = 0;
      }
      // In UTF-8, the high bit set means a lead or trail byte; if the next
      // bit is clear, it's a trail byte; otherwise the number of set high bits
      // counts the bytes in the sequence.  The maximum legitimate sequence is
      // four bytes.
      if (len != 0 && (aBuf[len-1] & 0x80))
      {
	int pos = len;
	while (--pos >= 0 && (aBuf[pos] & 0xC0) == 0x80)
	  ;
	if (pos >= 0 && (aBuf[pos] & 0x80) && len - pos < 4 &&
	    (pos == 0 || (aBuf[pos-1] & 0xC0) != 0xC0))
	{
	  char lead = aBuf[pos];
	  mb_size = 0;
	  do
	  {
	    ++mb_size;
	    lead <<= 1;
	  } while (lead & 0x80);
	  if (mb_size <= 4 && mb_size > len - pos)
	  {
	    mb_len = len - pos;
	    memcpy( mb, aBuf + pos, mb_len );
	    len = pos;
	    if (log_level & 4)
	    {
	      if (mb_len == nNumberOfCharsToWrite)
		DEBUGSTR( 4, "  lead byte%s, removing",
			     (mb_len == 1) ? "" : "s" );
	      else if (mb_len == 1)
		DEBUGSTR( 4, "  ends with a lead byte, removing" );
	      else
		DEBUGSTR( 4, "  ends with %u lead bytes, removing", mb_len );
	    }
	  }
	}
      }
    }
    if (len == 0)
    {
      if (lpNumberOfCharsWritten != NULL)
	*lpNumberOfCharsWritten = wlen;
      goto check_written;
    }
    if (len <= lenof(wBuf))
      buf = wBuf;
    else
    {
      buf = HeapAlloc( hHeap, 0, TSIZE(len) );
      if (buf == NULL)
      {
	DEBUGSTR( 4, "HeapAlloc failed, using original function" );
	rc = WriteConsoleA( hCon, aBuf,len, lpNumberOfCharsWritten,lpReserved );
	goto check_written;
      }
    }
    len = MultiByteToWideChar( cp, 0, aBuf, len, buf, len );
    rc = ParseAndPrintString( hCon, buf, len, lpNumberOfCharsWritten );
    if (wlen != 0 && rc && lpNumberOfCharsWritten != NULL)
      *lpNumberOfCharsWritten += wlen;
    if (buf != wBuf)
      HeapFree( hHeap, 0, buf );
  check_written:
    if (rc && lpNumberOfCharsWritten != NULL &&
	      *lpNumberOfCharsWritten != nNumberOfCharsToWrite)
    {
      // Converting a multibyte character to Unicode results in a different
      // "character" count.  This causes some programs to think not everything
      // was written, so the difference is sent again.	Fudge the (presumably)
      // correct count.
      if (search_env( L"ANSICON_API", prog ))
	*lpNumberOfCharsWritten = nNumberOfCharsToWrite;
    }
    return rc;
  }

  return WriteConsoleA( hCon, lpBuffer, nNumberOfCharsToWrite,
			lpNumberOfCharsWritten, lpReserved );
}

BOOL
WINAPI MyWriteConsoleW( HANDLE hCon, LPCVOID lpBuffer,
			DWORD nNumberOfCharsToWrite,
			LPDWORD lpNumberOfCharsWritten, LPVOID lpReserved )
{
  if (nNumberOfCharsToWrite != 0 && IsConsoleHandle( hCon ))
  {
    DEBUGSTR( 4, "WriteConsoleW: %u %\"<S",
		 nNumberOfCharsToWrite, lpBuffer );
    return ParseAndPrintString( hCon, lpBuffer,
				nNumberOfCharsToWrite,
				lpNumberOfCharsWritten );
  }

  return WriteConsoleW( hCon, lpBuffer, nNumberOfCharsToWrite,
			lpNumberOfCharsWritten, lpReserved );
}

BOOL
WINAPI MyWriteFile( HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite,
		    LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED lpOverlapped )
{
  if (nNumberOfBytesToWrite != 0 && IsConsoleHandle( hFile ))
  {
    if (HandleToULong( hFile ) == STD_OUTPUT_HANDLE ||
	HandleToULong( hFile ) == STD_ERROR_HANDLE)
      hFile = GetStdHandle( HandleToULong( hFile ) );

    write_func = "WriteFile";
    MyWriteConsoleA( hFile, lpBuffer,nNumberOfBytesToWrite, NULL,lpOverlapped );
    if (lpNumberOfBytesWritten != NULL)
      *lpNumberOfBytesWritten = nNumberOfBytesToWrite;
    return TRUE;
  }

  return WriteFile( hFile, lpBuffer, nNumberOfBytesToWrite,
		    lpNumberOfBytesWritten, lpOverlapped );
}


#define HHFILE (HANDLE)(DWORD_PTR)

UINT
WINAPI My_lwrite( HFILE hFile, LPCSTR lpBuffer, UINT uBytes )
{
  if (uBytes != 0 && IsConsoleHandle( HHFILE hFile ))
  {
    write_func = "_lwrite";
    MyWriteConsoleA( HHFILE hFile, lpBuffer, uBytes, NULL, NULL );
    return uBytes;
  }

  return _lwrite( hFile, lpBuffer, uBytes );
}


VOID
WINAPI MyExitProcess( UINT uExitCode )
{
  if (hBell != NULL)
    WaitForSingleObject( hBell, INFINITE );
  ExitProcess( uExitCode );
}


DWORD WINAPI FreeLibraryThread( LPVOID param )
{
  FreeLibraryAndExitThread( hDllInstance, 0 );
  return 0;
}

BOOL
WINAPI MyFreeLibrary( HMODULE hModule )
{
  if (hModule == hDllInstance)
  {
    if (hBell != NULL)
      WaitForSingleObject( hBell, INFINITE );
    CloseHandle( CreateThread( NULL, 4096, FreeLibraryThread, NULL, 0, NULL ) );
    return TRUE;
  }

  return FreeLibrary( hModule );
}


//-----------------------------------------------------------------------------
//   MyCreate...
// Add GENERIC_READ access to enable retrieving console info.
//-----------------------------------------------------------------------------

HANDLE
WINAPI MyCreateFileA( LPCSTR lpFileName, DWORD dwDesiredAccess,
		      DWORD dwShareMode,
		      LPSECURITY_ATTRIBUTES lpSecurityAttributes,
		      DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes,
		      HANDLE hTemplateFile )
{
  if (dwDesiredAccess == GENERIC_WRITE)
  {
    if (_stricmp( lpFileName, "con" ) == 0)
      lpFileName = "CONOUT$";
    if (_stricmp( lpFileName, "CONOUT$" ) == 0)
      dwDesiredAccess |= GENERIC_READ;
  }
  return CreateFileA( lpFileName, dwDesiredAccess, dwShareMode,
		      lpSecurityAttributes, dwCreationDisposition,
		      dwFlagsAndAttributes, hTemplateFile );
}

HANDLE
WINAPI MyCreateFileW( LPCWSTR lpFileName, DWORD dwDesiredAccess,
		      DWORD dwShareMode,
		      LPSECURITY_ATTRIBUTES lpSecurityAttributes,
		      DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes,
		      HANDLE hTemplateFile )
{
  if (dwDesiredAccess == GENERIC_WRITE)
  {
    if (_wcsicmp( lpFileName, L"con" ) == 0)
      lpFileName = L"CONOUT$";
    if (_wcsicmp( lpFileName, L"CONOUT$" ) == 0)
      dwDesiredAccess |= GENERIC_READ;
  }
  return CreateFileW( lpFileName, dwDesiredAccess, dwShareMode,
		      lpSecurityAttributes, dwCreationDisposition,
		      dwFlagsAndAttributes, hTemplateFile );
}

HANDLE
WINAPI MyCreateConsoleScreenBuffer( DWORD dwDesiredAccess, DWORD dwShareMode,
				    const SECURITY_ATTRIBUTES* lpSecurityAttributes,
				    DWORD dwFlags, LPVOID lpScreenBufferData )
{
  dwDesiredAccess |= GENERIC_READ;
  return CreateConsoleScreenBuffer( dwDesiredAccess, dwShareMode,
				    lpSecurityAttributes, dwFlags,
				    lpScreenBufferData );
}


// ========== Environment variable

void set_ansicon( PCONSOLE_SCREEN_BUFFER_INFO pcsbi )
{
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  TCHAR buf[64];

  if (pcsbi == NULL)
  {
    HANDLE hConOut;
    hConOut = CreateFile( L"CONOUT$", GENERIC_READ | GENERIC_WRITE,
				      FILE_SHARE_READ | FILE_SHARE_WRITE,
				      NULL, OPEN_EXISTING, 0, NULL );
    GetConsoleScreenBufferInfo( hConOut, &csbi );
    CloseHandle( hConOut );
    pcsbi = &csbi;
  }

  wsprintf( buf, L"%dx%d (%dx%d)",
	    pcsbi->dwSize.X, pcsbi->dwSize.Y,
	    pcsbi->srWindow.Right - pcsbi->srWindow.Left + 1,
	    pcsbi->srWindow.Bottom - pcsbi->srWindow.Top + 1 );
  SetEnvironmentVariable( L"ANSICON", buf );
}

DWORD
WINAPI MyGetEnvironmentVariableA( LPCSTR lpName, LPSTR lpBuffer, DWORD nSize )
{
  if (_stricmp( lpName, "ANSICON_VER" ) == 0)
  {
    if (nSize < sizeof(PVEREA))
      return sizeof(PVEREA);
    memcpy( lpBuffer, PVEREA, sizeof(PVEREA) );
    return sizeof(PVEREA) - 1;
  }

  if (_stricmp( lpName, "CLICOLOR" ) == 0)
  {
    if (nSize < 2)
      return 2;
    lpBuffer[0] = '1';
    lpBuffer[1] = '\0';
    return 1;
  }

  if (_stricmp( lpName, "ANSICON" ) == 0)
    set_ansicon( NULL );

  return GetEnvironmentVariableA( lpName, lpBuffer, nSize );
}

DWORD
WINAPI MyGetEnvironmentVariableW( LPCWSTR lpName, LPWSTR lpBuffer, DWORD nSize )
{
  if (_wcsicmp( lpName, L"ANSICON_VER" ) == 0)
  {
    if (nSize < lenof(PVERE))
      return lenof(PVERE);
    memcpy( lpBuffer, PVERE, sizeof(PVERE) );
    return lenof(PVERE) - 1;
  }

  if (_wcsicmp( lpName, L"CLICOLOR" ) == 0)
  {
    if (nSize < 2)
      return 2;
    lpBuffer[0] = '1';
    lpBuffer[1] = '\0';
    return 1;
  }

  if (_wcsicmp( lpName, L"ANSICON" ) == 0)
    set_ansicon( NULL );

  return GetEnvironmentVariableW( lpName, lpBuffer, nSize );
}


// ========== Initialisation

HookFn Hooks[] = {
  // These two are expected first!
  { APILibraryLoader,	   "LoadLibraryA",              (PROC)MyLoadLibraryA,              NULL, NULL, NULL },
  { APILibraryLoader,	   "LoadLibraryW",              (PROC)MyLoadLibraryW,              NULL, NULL, NULL },
  { APIProcessThreads,	   "CreateProcessA",            (PROC)MyCreateProcessA,            NULL, NULL, NULL },
  { APIProcessThreads,	   "CreateProcessW",            (PROC)MyCreateProcessW,            NULL, NULL, NULL },
  { APIProcessEnvironment, "GetEnvironmentVariableA",   (PROC)MyGetEnvironmentVariableA,   NULL, NULL, NULL },
  { APIProcessEnvironment, "GetEnvironmentVariableW",   (PROC)MyGetEnvironmentVariableW,   NULL, NULL, NULL },
  { APILibraryLoader,	   "GetProcAddress",            (PROC)MyGetProcAddress,            NULL, NULL, NULL },
  { APILibraryLoader,	   "LoadLibraryExA",            (PROC)MyLoadLibraryExA,            NULL, NULL, NULL },
  { APILibraryLoader,	   "LoadLibraryExW",            (PROC)MyLoadLibraryExW,            NULL, NULL, NULL },
  { APIConsole, 	   "SetConsoleMode",            (PROC)MySetConsoleMode,            NULL, NULL, NULL },
  { APIConsole, 	   "WriteConsoleA",             (PROC)MyWriteConsoleA,             NULL, NULL, NULL },
  { APIConsole, 	   "WriteConsoleW",             (PROC)MyWriteConsoleW,             NULL, NULL, NULL },
  { APIFile,		   "WriteFile",                 (PROC)MyWriteFile,                 NULL, NULL, NULL },
  { APIKernel,		   "_lwrite",                   (PROC)My_lwrite,                   NULL, NULL, NULL },
  { APIProcessThreads,	   "ExitProcess",               (PROC)MyExitProcess,               NULL, NULL, NULL },
  { APILibraryLoader,	   "FreeLibrary",               (PROC)MyFreeLibrary,               NULL, NULL, NULL },
  { APIFile,		   "CreateFileA",               (PROC)MyCreateFileA,               NULL, NULL, NULL },
  { APIFile,		   "CreateFileW",               (PROC)MyCreateFileW,               NULL, NULL, NULL },
  { APIKernel,		   "CreateConsoleScreenBuffer", (PROC)MyCreateConsoleScreenBuffer, NULL, NULL, NULL },
  { NULL, NULL, NULL, NULL, NULL, NULL }
};

//-----------------------------------------------------------------------------
//   OriginalAttr()
// Determine the original attributes for use by \e[m.
//-----------------------------------------------------------------------------
void OriginalAttr( PVOID lpReserved )
{
  HANDLE hConOut;
  CONSOLE_SCREEN_BUFFER_INFO csbi;

  hConOut = CreateFile( L"CONOUT$", GENERIC_READ | GENERIC_WRITE,
				    FILE_SHARE_READ | FILE_SHARE_WRITE,
				    NULL, OPEN_EXISTING, 0, NULL );
  if (!GetConsoleScreenBufferInfo( hConOut, &csbi ))
    csbi.wAttributes = 7;

  // If we were loaded dynamically, remember the current attributes to restore
  // upon unloading.  However, if we're the 64-bit DLL, but the image is 32-
  // bit, then the dynamic load was due to injecting into AnyCPU.
  if (lpReserved == NULL)
  {
#ifdef _WIN64
    PIMAGE_DOS_HEADER pDosHeader;
    PIMAGE_NT_HEADERS pNTHeader;
    pDosHeader = (PIMAGE_DOS_HEADER)GetModuleHandle( NULL );
    pNTHeader = MakeVA( PIMAGE_NT_HEADERS, pDosHeader->e_lfanew );
    if (pNTHeader->FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64)
#endif
    orgattr = csbi.wAttributes;
    GetConsoleMode( hConOut, &orgmode );
    GetConsoleCursorInfo( hConOut, &orgcci );
  }
  CloseHandle( hConOut );

  get_state();
}


//-----------------------------------------------------------------------------
//   DllMain()
// Function called by the system when processes and threads are initialized
// and terminated.
//-----------------------------------------------------------------------------

// Need to export something for static loading to work, this is as good as any.
__declspec(dllexport)
BOOL WINAPI DllMain( HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved )
{
  BOOL	  bResult = TRUE;
  PHookFn hook;
  TCHAR   logstr[4];
  typedef LONG (WINAPI *PNTQIT)( HANDLE, int, PVOID, ULONG, PULONG );
  static PNTQIT NtQueryInformationThread;

  if (dwReason == DLL_PROCESS_ATTACH)
  {
    hHeap = HeapCreate( 0, 0, 128 * 1024 );

    *logstr = '\0';
    GetEnvironmentVariable( L"ANSICON_LOG", logstr, lenof(logstr) );
    log_level = _wtoi( logstr );
    prog = get_program_name( NULL );
#if defined(_WIN64) || defined(W32ON64)
    DllNameType = DllName - 6 +
#endif
    GetModuleFileName( hInstance, DllName, lenof(DllName) );
    set_ansi_dll();

    hDllInstance = hInstance; // save Dll instance handle
    DEBUGSTR( 1, "hDllInstance = %p", hDllInstance );

    // Get the entry points to the original functions.
    hKernel = GetModuleHandleA( APIKernel );
    for (hook = Hooks; hook->name; ++hook)
      hook->oldfunc = GetProcAddress( hKernel, hook->name );

    // Get my import addresses, to detect if anyone's hooked me.
    DEBUGSTR( 2, "Storing my imports" );
    HookAPIOneMod( NULL, Hooks, FALSE, "" );

    bResult = HookAPIAllMod( Hooks, FALSE, FALSE );
    OriginalAttr( lpReserved );

    NtQueryInformationThread = (PNTQIT)GetProcAddress(
		 GetModuleHandle( L"ntdll.dll" ), "NtQueryInformationThread" );
    if (NtQueryInformationThread == NULL)
      DisableThreadLibraryCalls( hInstance );
  }
  else if (dwReason == DLL_PROCESS_DETACH)
  {
    if (lpReserved == NULL)
    {
      DEBUGSTR( 1, "Unloading" );
      HookAPIAllMod( Hooks, TRUE, FALSE );
    }
    else
    {
      DEBUGSTR( 1, "Terminating" );
    }
    if (orgattr != 0)
    {
      hConOut = CreateFile( L"CONOUT$", GENERIC_READ | GENERIC_WRITE,
					FILE_SHARE_READ | FILE_SHARE_WRITE,
					NULL, OPEN_EXISTING, 0, NULL );
      SetConsoleTextAttribute( hConOut, orgattr );
      SetConsoleMode( hConOut, orgmode );
      SetConsoleCursorInfo( hConOut, &orgcci );
      CloseHandle( hConOut );
    }
    if (hMap != NULL)
    {
      UnmapViewOfFile( pState );
      CloseHandle( hMap );
    }
    HeapDestroy( hHeap );
  }
  else if (dwReason == DLL_THREAD_DETACH)
  {
    PVOID start;
    if (NtQueryInformationThread( GetCurrentThread(),
				  9 /* ThreadQuerySetWin32StartAddress */,
				  &start, sizeof(start), NULL ) == 0
	&& (start == Hooks[0].oldfunc || start == Hooks[1].oldfunc
	 || start == Hooks[0].apifunc || start == Hooks[1].apifunc))
    {
      DEBUGSTR( 2, "Injection detected" );
      HookAPIAllMod( Hooks, FALSE, TRUE );
    }
  }
  return bResult;
}
