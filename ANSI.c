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

  v1.80, 26 October to 24 December, 2017:
    fix unloading;
    revert back to (re)storing buffer cursor position;
    increase cache to five handles;
    hook CreateFile & CreateConsoleScreenBuffer to enable readable handles;
    fix cursor report with duplicated digits (e.g. "11" was just "1");
    preserve escape that isn't part of a sequence;
    fix escape followed by CRM in control mode;
    use the system default sound for the bell;
    add DECPS Play Sound;
    use intermediate byte '+' to use buffer, not window;
    ESC followed by a control character will display that character;
    added palette sequences;
    change the scan lines in the graphics set to their actual Unicode chars;
    added IND, NEL & RI (using buffer, in keeping with LF);
    added DA, DECCOLM, DECNCSM, DECSC & DECRC (with SGR & G0);
    partially support SCS (just G0 as DEC special & ASCII);
    an explicit zero parameter should still default to one;
    restrict parameters to a maximum value of 32767;
    added tab handling;
    added the bright SGR colors, recognised the system indices;
    added insert mode;
    BS/CR/CUB/HPB after wrap will move back to the previous line(s);
    added DECOM, DECSTBM, SD & SU;
    only flush before accessing the console, adding a mode to flush immediately;
    added DECSTR & RIS;
    fix state problems with windowless processes.

  v1.81, 26 to 28 December, 2017:
    combine multiple CRs as one (to ignore all CRs before LF);
    don't process CR or BS during CRM;
    don't flush CR immediately (to catch following LF);
    fix CRM with all partial RM sequences;
    check for the empty buffer within the critical section;
    palette improvements.

  v1.82, 12 & 13 February, 2018:
    add ANSICON_WRAP environment variable for programs that expect the wrap;
    flush and invalidate the cache on CloseHandle;
    make IsConsoleHandle a critical section, for multithreaded processes;
    use APIConsole for all console functions (needed for Windows 10).

  v1.83, 16 February, 2018:
    create the flush thread on first use.

  v1.84, 17 February, 26 April to 10 May, 2018:
    close the flush handles on detach;
    dynamically load WINMM.DLL;
    remove dependency on the CRT and USER32.DLL;
    replace bsearch (in procrva.c) with specific code;
    if the primary thread is detached exit the process;
    get real WriteFile handle before testing for console;
    use remote load on Win8+ when the process has no IAT;
    increase heap to 256KiB to fix logging of really long command lines;
    default to 7 or -7 if ANSICON_DEF could not be parsed;
    scrolling will use the default attribute for new lines;
    workaround Windows 10 1803 console bug.

  v1.85, 22 & 23 August, 2018:
    fix creating the wrap buffer;
    always inject from ansicon.exe, even if it's GUI or excluded;
    log CreateFile calls;
    preserve last error.

  v1.86, 4 November, 2018:
    always unhook, even on terminate;
    check the DLL still exists before adding to imports.

  v1.87, 3 February, 2019:
    some hooked functions are not imported, so myimport wasn't set;
    add missing SetCurrentConsoleFontEx to list of hooks.

  v1.88, 1 March, 2019:
    a detached process has no console handle (fixes set_ansicon).

  v1.89, 29 April, 2019:
    an eight-digit window handle would break my custom printf.
*/

#include "ansicon.h"
#include "version.h"

#include <mmsystem.h>
#ifndef SND_SENTRY
#define SND_SENTRY 0x80000
#endif
#undef PlaySound
typedef BOOL (WINAPI *FnPlaySound)( LPCWSTR, HMODULE, DWORD );
FnPlaySound PlaySound;
HMODULE winmm;

#define is_digit(c) ('0' <= (c) && (c) <= '9')

// ========== Global variables and constants

HANDLE	hConOut;		// handle to CONOUT$
WORD	orgattr;		// original attributes
DWORD	orgmode;		// original mode
CONSOLE_CURSOR_INFO orgcci;	// original cursor state
HANDLE	hHeap;			// local memory heap
HANDLE	hBell, hFlush;
BOOL	ansicon;		// are we in ansicon.exe?

#define CACHE	5
struct Cache
{
  HANDLE h;
  DWORD  mode;
} cache[CACHE];

#define ESC	'\x1B'          // ESCape character
#define BEL	'\x07'          // BELl
#define HT	'\x09'          // Horizontal Tabulation
#define SO	'\x0E'          // Shift Out
#define SI	'\x0F'          // Shift In

#define MAX_ARG 16		// max number of args in an escape sequence
int   state;			// automata state
TCHAR prefix;			// escape sequence prefix ( '[' or ']' );
TCHAR prefix2;			// secondary prefix ( one of '<=>?' );
TCHAR suffix;			// escape sequence final byte
TCHAR suffix2;			// escape sequence intermediate byte
int   ibytes;			// count of intermediate bytes
int   es_argc;			// escape sequence args count
int   es_argv[MAX_ARG]; 	// escape sequence args
TCHAR Pt_arg[4096];		// text parameter for Operating System Command
int   Pt_len;
BOOL  shifted, G0_special, SaveG0;
BOOL  wm = FALSE;		// does program detect wrap itself?
BOOL  awm = TRUE;		// autowrap mode
BOOL  im;			// insert mode
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
  L'\x23ba',    // o - Horizontal Scan Line-1
  L'\x23bb',    // p - Horizontal Scan Line-3
  L'\x2500',    // q - Box Drawings Light Horizontal (SCAN 5)
  L'\x23bc',    // r - Horizontal Scan Line-7
  L'\x23bd',    // s - Horizontal Scan Line-9
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

const BYTE foregroundcolor[16] =
{
  FOREGROUND_BLACK,			// black foreground
  FOREGROUND_RED,			// red foreground
  FOREGROUND_GREEN,			// green foreground
  FOREGROUND_RED | FOREGROUND_GREEN,	// yellow foreground
  FOREGROUND_BLUE,			// blue foreground
  FOREGROUND_BLUE | FOREGROUND_RED,	// magenta foreground
  FOREGROUND_BLUE | FOREGROUND_GREEN,	// cyan foreground
  FOREGROUND_WHITE,			// white foreground
  FOREGROUND_INTENSITY | FOREGROUND_BLACK,
  FOREGROUND_INTENSITY | FOREGROUND_RED,
  FOREGROUND_INTENSITY | FOREGROUND_GREEN,
  FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN,
  FOREGROUND_INTENSITY | FOREGROUND_BLUE,
  FOREGROUND_INTENSITY | FOREGROUND_BLUE | FOREGROUND_RED,
  FOREGROUND_INTENSITY | FOREGROUND_BLUE | FOREGROUND_GREEN,
  FOREGROUND_INTENSITY | FOREGROUND_WHITE
};

const BYTE backgroundcolor[16] =
{
  BACKGROUND_BLACK,			// black background
  BACKGROUND_RED,			// red background
  BACKGROUND_GREEN,			// green background
  BACKGROUND_RED | BACKGROUND_GREEN,	// yellow background
  BACKGROUND_BLUE,			// blue background
  BACKGROUND_BLUE | BACKGROUND_RED,	// magenta background
  BACKGROUND_BLUE | BACKGROUND_GREEN,	// cyan background
  BACKGROUND_WHITE,			// white background
  BACKGROUND_INTENSITY | BACKGROUND_BLACK,
  BACKGROUND_INTENSITY | BACKGROUND_RED,
  BACKGROUND_INTENSITY | BACKGROUND_GREEN,
  BACKGROUND_INTENSITY | BACKGROUND_RED | BACKGROUND_GREEN,
  BACKGROUND_INTENSITY | BACKGROUND_BLUE,
  BACKGROUND_INTENSITY | BACKGROUND_BLUE | BACKGROUND_RED,
  BACKGROUND_INTENSITY | BACKGROUND_BLUE | BACKGROUND_GREEN,
  BACKGROUND_INTENSITY | BACKGROUND_WHITE
};

const BYTE attr2ansi[16] =		// map console attribute to ANSI number
{					//  or vice versa
  0,					// black
  4,					// blue
  2,					// green
  6,					// cyan
  1,					// red
  5,					// magenta
  3,					// yellow
  7,					// white
  8,					// bright black
 12,					// bright blue
 10,					// bright green
 14,					// bright cyan
  9,					// bright red
 13,					// bright magenta
 11,					// bright yellow
 15,					// bright white
};


typedef struct _CONSOLE_SCREEN_BUFFER_INFOX {
  ULONG      cbSize;
  COORD      dwSize;
  COORD      dwCursorPosition;
  WORD	     wAttributes;
  SMALL_RECT srWindow;
  COORD      dwMaximumWindowSize;
  WORD	     wPopupAttributes;
  BOOL	     bFullscreenSupported;
  COLORREF   ColorTable[16];
} CONSOLE_SCREEN_BUFFER_INFOX, *PCONSOLE_SCREEN_BUFFER_INFOX;

typedef BOOL (WINAPI *PHCSBIX)(
  HANDLE hConsoleOutput,
  PCONSOLE_SCREEN_BUFFER_INFOX lpConsoleScreenBufferInfoEx
);

PHCSBIX GetConsoleScreenBufferInfoX, SetConsoleScreenBufferInfoX;

BOOL WINAPI GetConsoleScreenBufferInfoEx_repl( HANDLE h,
					       PCONSOLE_SCREEN_BUFFER_INFOX i )
{
  return FALSE;
}


typedef struct _CONSOLE_FONT_INFOX {
  ULONG      cbSize;
  DWORD      nFont;
  COORD      dwFontSize;
  UINT	     FontFamily;
  UINT	     FontWeight;
  WCHAR      FaceName[LF_FACESIZE];
} CONSOLE_FONT_INFOX, *PCONSOLE_FONT_INFOX;

typedef BOOL (WINAPI *PHBCFIX)(
  HANDLE hConsoleOutput,
  BOOL bMaximumWindow,
  PCONSOLE_FONT_INFOX lpConsoleCurrentFontEx
);

PHBCFIX SetCurrentConsoleFontX;


// Reduce verbosity.
#define CURPOS dwCursorPosition
#define ATTR   Info.wAttributes
#define WIDTH  Info.dwSize.X
#define HEIGHT Info.dwSize.Y
#define CUR    Info.CURPOS
#define WIN    Info.srWindow
#define TOP    WIN.Top
#define BOTTOM WIN.Bottom
#define LAST   (HEIGHT - 1)
#define LEFT   0
#define RIGHT  (WIDTH - 1)


#define MAX_TABS 2048

typedef struct
{
  BYTE	foreground;	// ANSI base color (0 to 7; add 30)
  BYTE	background;	// ANSI base color (0 to 7; add 40)
  BYTE	bold;		// console FOREGROUND_INTENSITY bit
  BYTE	underline;	// console BACKGROUND_INTENSITY bit
  BYTE	rvideo; 	// swap foreground/bold & background/underline
  BYTE	concealed;	// set foreground/bold to background/underline
  BYTE	reverse;	// swap console foreground & background attributes
} SGR;

SGR orgsgr;		// original SGR

typedef struct
{
  SGR	   sgr, SaveSgr;
  WORD	   SaveAttr;
  BYTE	   fm;		// flush mode
  BYTE	   crm; 	// showing control characters?
  BYTE	   om;		// origin mode
  BYTE	   tb_margins;	// top/bottom margins set?
  SHORT    top_margin;
  SHORT    bot_margin;
  COORD    SavePos;	// saved cursor position
  COLORREF o_palette[16];  // original palette, for resetting
  COLORREF x_palette[240]; // xterm 256-color palette, less 16 system colors
  SHORT    buf_width;	// buffer width prior to setting 132 columns
  SHORT    win_width;	// window width prior to setting 132 columns
  BYTE	   noclear;	// don't clear the screen on column mode change
  BYTE	   tabs;	// handle tabs directly
  BYTE	   tab_stop[MAX_TABS];
} STATE, *PSTATE;

STATE  default_state;	// for when there's no window or file mapping
PSTATE pState = &default_state;
BOOL   valid_state;
HANDLE hMap;

#include "palette.h"

void set_ansicon( PCONSOLE_SCREEN_BUFFER_INFO );


void get_state( void )
{
  TCHAR  buf[64];
  HWND	 hwnd;
  BOOL	 init;
  HANDLE hConOut;
  CONSOLE_SCREEN_BUFFER_INFO  Info;
  CONSOLE_SCREEN_BUFFER_INFOX csbix;

  if (valid_state)
    return;

  hwnd = GetConsoleWindow();
  if (hwnd == NULL)
    return;

  valid_state = TRUE;

  ac_wprintf( buf, "ANSICON_State_%X", PtrToUint( hwnd ) );
  hMap = CreateFileMapping( INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
			    0, sizeof(STATE), buf );
  init = (GetLastError() != ERROR_ALREADY_EXISTS);
  pState = MapViewOfFile( hMap, FILE_MAP_ALL_ACCESS, 0, 0, 0 );
  if (pState == NULL)
  {
    DEBUGSTR( 1, "File mapping failed (%u) - using default state",
		 GetLastError() );
    pState = &default_state;
    CloseHandle( hMap );
    hMap = NULL;
  }

  if (init)
  {
    hConOut = CreateFile( L"CONOUT$", GENERIC_READ | GENERIC_WRITE,
				      FILE_SHARE_READ | FILE_SHARE_WRITE,
				      NULL, OPEN_EXISTING, 0, NULL );
    csbix.cbSize = sizeof(csbix);
    if (GetConsoleScreenBufferInfoX( hConOut, &csbix ))
    {
      arrcpy( pState->o_palette, csbix.ColorTable );
      ATTR = csbix.wAttributes;
    }
    else
    {
      arrcpy( pState->o_palette, legacy_palette );
      if (!GetConsoleScreenBufferInfo( hConOut, &Info ))
	ATTR = 7;
    }
    arrcpy( pState->x_palette, xterm_palette );

    pState->sgr.foreground = attr2ansi[ATTR & 7];
    pState->sgr.background = attr2ansi[(ATTR >> 4) & 7];
    pState->sgr.bold	   = ATTR & FOREGROUND_INTENSITY;
    pState->sgr.underline  = ATTR & BACKGROUND_INTENSITY;

    CloseHandle( hConOut );
  }

  if (!GetEnvironmentVariable( L"ANSICON_DEF", NULL, 0 ))
  {
    TCHAR  def[4];
    LPTSTR a = def;
    hConOut = CreateFile( L"CONOUT$", GENERIC_READ | GENERIC_WRITE,
				      FILE_SHARE_READ | FILE_SHARE_WRITE,
				      NULL, OPEN_EXISTING, 0, NULL );
    if (!GetConsoleScreenBufferInfo( hConOut, &Info ))
    {
      RtlZeroMemory( &Info, sizeof(Info) );
      ATTR = 7;
    }
    if (pState->sgr.reverse)
    {
      *a++ = '-';
      ATTR = ((ATTR >> 4) & 15) | ((ATTR & 15) << 4);
    }
    ac_wprintf( a, "%X", ATTR & 255 );
    SetEnvironmentVariable( L"ANSICON_DEF", def );
    set_ansicon( &Info );
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
    if (lstrcmpi( val, var ) == 0)
      return !not;
  }

  return not;
}


// ========== Print Buffer functions

#define BUFFER_SIZE 2048

int   nCharInBuffer;
WCHAR ChBuffer[BUFFER_SIZE];
WCHAR ChPrev;
int   nWrapped;
CRITICAL_SECTION CritSect;
HANDLE hFlushTimer;

void MoveDown( BOOL home );


// Well, this is annoying.  Setting the cursor position on any buffer always
// displays the cursor on the active buffer (at least on 7).  Since there's no
// way to tell which buffer is active (hooking SetConsoleActiveScreenBuffer
// isn't sufficient, since multiple handles could refer to the same buffer),
// hide the cursor, do the move and restore the cursor.
BOOL SetConsoleCursorPos( HANDLE hConsoleOutput, COORD dwCursorPosition )
{
  CONSOLE_CURSOR_INFO CursInfo;
  BOOL rc;

  GetConsoleCursorInfo( hConsoleOutput, &CursInfo );
  if (CursInfo.bVisible)
  {
    CursInfo.bVisible = FALSE;
    SetConsoleCursorInfo( hConsoleOutput, &CursInfo );
    rc = SetConsoleCursorPosition( hConsoleOutput, dwCursorPosition );
    CursInfo.bVisible = TRUE;
    SetConsoleCursorInfo( hConsoleOutput, &CursInfo );
  }
  else
    rc = SetConsoleCursorPosition( hConsoleOutput, dwCursorPosition );

  return rc;
}


// Set the cursor position, resetting the wrap flag.
void set_pos( int x, int y )
{
  COORD pos = { x, y };
  SetConsoleCursorPos( hConOut, pos );
  nWrapped = 0;
}


// Get the default attribute, as-is if !ATTR (i.e. preserve negative), else for
// the console (swap foreground/background if negative).
int get_default_attr( BOOL attr )
{
  TCHAR def[4];
  int	a;

  *def = '7'; def[1] = '\0';
  GetEnvironmentVariable( L"ANSICON_DEF", def, lenof(def) );
  a = ac_wcstol( def, NULL, 16 );
  if (a == 0)
    a = (*def == '-') ? -7 : 7;
  if (a > 0 || !attr)
    return a;
  a = -a;
  return ((a >> 4) & 15) | ((a & 15) << 4);
}


//-----------------------------------------------------------------------------
//   FlushBuffer()
// Writes the buffer to the console and empties it.
//-----------------------------------------------------------------------------

void FlushBuffer( void )
{
  DWORD nWritten;

  EnterCriticalSection( &CritSect );

  if (nCharInBuffer <= 0)
  {
    LeaveCriticalSection( &CritSect );
    return;
  }

  if ((wm || !awm) && !im && !pState->tb_margins)
  {
    if (pState->crm)
    {
      SetConsoleMode( hConOut, cache[0].mode & ~ENABLE_PROCESSED_OUTPUT );
      WriteConsole( hConOut, ChBuffer, nCharInBuffer, &nWritten, NULL );
      SetConsoleMode( hConOut, cache[0].mode );
    }
    else
      WriteConsole( hConOut, ChBuffer, nCharInBuffer, &nWritten, NULL );
  }
  else
  {
    HANDLE hConWrap;
    CONSOLE_CURSOR_INFO cci;
    CONSOLE_SCREEN_BUFFER_INFO Info, wi;

    if (nCharInBuffer < 4 && !im && !pState->tb_margins)
    {
      LPWSTR b = ChBuffer;
      if (pState->crm)
	SetConsoleMode( hConOut, cache[0].mode & ~ENABLE_PROCESSED_OUTPUT );
      do
      {
	WriteConsole( hConOut, b, 1, &nWritten, NULL );
	if (pState->crm || (*b != '\r' && *b != '\b' && *b != '\a'))
	{
	  GetConsoleScreenBufferInfo( hConOut, &Info );
	  if (CUR.X == 0)
	    ++nWrapped;
	}
      } while (++b, --nCharInBuffer);
      if (pState->crm)
	SetConsoleMode( hConOut, cache[0].mode );
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
      // width) and contains sufficient lines.
      GetConsoleScreenBufferInfo( hConOut, &Info );
      wi.dwSize.X = WIDTH;
      wi.dwSize.Y = 0;
      if (WIN.Right - WIN.Left + 1 != WIDTH)
	wi.dwSize.Y = BOTTOM - TOP + 1;
      if (BOTTOM - TOP < 2 * nCharInBuffer / WIDTH)
	wi.dwSize.Y = 2 * nCharInBuffer / WIDTH + 1;
      if (wi.dwSize.Y)
	SetConsoleScreenBufferSize( hConWrap, wi.dwSize );
      // Put the cursor on the top line, in the same column.
      wi.CURPOS.X = CUR.X;
      wi.CURPOS.Y = 0;
      SetConsoleCursorPosition( hConWrap, wi.CURPOS );
      if (pState->crm)
	SetConsoleMode( hConWrap, (awm) ? ENABLE_WRAP_AT_EOL_OUTPUT : 0 );
      else if (!awm)
	SetConsoleMode( hConWrap, ENABLE_PROCESSED_OUTPUT );
      else if (cache[0].mode & 4) // ENABLE_VIRTUAL_TERMINAL_PROCESSING
      {
	// Windows 10 1803 writes to the active buffer if VT is enabled.
	SetConsoleMode( hConWrap, cache[0].mode & ~4 );
      }
      WriteConsole( hConWrap, ChBuffer, nCharInBuffer, &nWritten, NULL );
      GetConsoleScreenBufferInfo( hConWrap, &wi );
      if (pState->tb_margins && CUR.Y + wi.CURPOS.Y > TOP + pState->bot_margin)
      {
	if (CUR.Y > TOP + pState->bot_margin)
	{
	  // If we're at the bottom of the window, outside the margins, then
	  // just keep overwriting the last line.
	  if (CUR.Y + wi.CURPOS.Y > BOTTOM)
	  {
	    PCHAR_INFO row = HeapAlloc( hHeap, 0, WIDTH * sizeof(CHAR_INFO) );
	    if (row != NULL)
	    {
	      COORD s, c;
	      SMALL_RECT r;
	      s.X = WIDTH;
	      s.Y = 1;
	      c.X = c.Y = 0;
	      for (r.Top = 0; r.Top <= wi.CURPOS.Y; ++r.Top)
	      {
		if (r.Top == 0)
		{
		  r.Left = CUR.X;
		  r.Right = RIGHT;
		}
		else if (r.Top == wi.CURPOS.Y)
		{
		  r.Left = LEFT;
		  r.Right = wi.CURPOS.X - 1;
		}
		else
		{
		  r.Left = LEFT;
		  r.Right = RIGHT;
		}
		r.Bottom = r.Top;
		ReadConsoleOutput( hConWrap, row, s, c, &r );
		r.Top = r.Bottom = CUR.Y;
		WriteConsoleOutput( hConOut, row, s, c, &r );
		if (CUR.Y != BOTTOM)
		  ++CUR.Y;
	      }
	      HeapFree( hHeap, 0, row );
	      CloseHandle( hConWrap );
	      nWrapped = 0;
	      goto done;
	    }
	  }
	}
	else if (wi.CURPOS.Y > pState->bot_margin - pState->top_margin)
	{
	  // The line is bigger than the scroll region, copy that portion.
	  PCHAR_INFO row = HeapAlloc( hHeap, 0,
				(pState->bot_margin - pState->top_margin + 1)
				* WIDTH * sizeof(CHAR_INFO) );
	  if (row != NULL)
	  {
	    COORD s, c;
	    SMALL_RECT r;
	    s.X = WIDTH;
	    s.Y = pState->bot_margin - pState->top_margin + 1;
	    c.X = c.Y = 0;
	    r.Left = LEFT;
	    r.Right = RIGHT;
	    r.Bottom = wi.CURPOS.Y;
	    r.Top = r.Bottom - (pState->bot_margin - pState->top_margin);
	    ReadConsoleOutput( hConWrap, row, s, c, &r );
	    r.Top = TOP + pState->top_margin;
	    r.Bottom = TOP + pState->bot_margin;
	    WriteConsoleOutput( hConOut, row, s, c, &r );
	    HeapFree( hHeap, 0, row );
	    CloseHandle( hConWrap );
	    nWrapped = pState->bot_margin - pState->top_margin;
	    goto done;
	  }
	}
	else
	{
	  // Scroll the region, then write as normal.
	  SMALL_RECT sr;
	  COORD      c;
	  CHAR_INFO  ci;

	  ci.Char.UnicodeChar = ' ';
	  ci.Attributes = get_default_attr( TRUE );
	  c.X	    =
	  sr.Left   = LEFT;
	  sr.Right  = RIGHT;
	  sr.Top    = TOP + pState->top_margin;
	  sr.Bottom = TOP + pState->bot_margin;
	  c.Y	    = sr.Top - wi.CURPOS.Y;
	  ScrollConsoleScreenBuffer( hConOut, &sr, &sr, c, &ci );
	  CUR.Y -= wi.CURPOS.Y;
	  SetConsoleCursorPos( hConOut, CUR );
	}
      }
      nWrapped += wi.CURPOS.Y;
      CloseHandle( hConWrap );
      if (im && !nWrapped)
      {
	SMALL_RECT sr, cr;
	CHAR_INFO  ci;		// unused, but necessary

	cr.Top = cr.Bottom = sr.Top = sr.Bottom = CUR.Y;
	cr.Right = sr.Right = RIGHT;
	sr.Left = CUR.X;
	cr.Left = CUR.X = wi.CURPOS.X;
	ScrollConsoleScreenBuffer( hConOut, &sr, &cr, CUR, &ci );
      }
      else if (nWrapped && CUR.Y + nWrapped > LAST)
      {
	// The buffer is going to scroll; do it manually in order to use the
	// default attribute, not current.
	SMALL_RECT sr;
	COORD	   c;
	CHAR_INFO  ci;

	ci.Char.UnicodeChar = ' ';
	ci.Attributes = get_default_attr( TRUE );
	c.X	  =
	sr.Left   = LEFT;
	sr.Right  = RIGHT;
	sr.Top	  = 0;
	sr.Bottom = LAST;
	c.Y	  = -wi.CURPOS.Y;
	ScrollConsoleScreenBuffer( hConOut, &sr, &sr, c, &ci );
	CUR.Y -= wi.CURPOS.Y;
	SetConsoleCursorPos( hConOut, CUR );
      }
      if (pState->crm)
      {
	SetConsoleMode( hConOut, cache[0].mode & ~ENABLE_PROCESSED_OUTPUT );
	WriteConsole( hConOut, ChBuffer, nCharInBuffer, &nWritten, NULL );
	SetConsoleMode( hConOut, cache[0].mode );
      }
      else
	WriteConsole( hConOut, ChBuffer, nCharInBuffer, &nWritten, NULL );
    }
  }
done:
  nCharInBuffer = 0;

  LeaveCriticalSection( &CritSect );
}

//-----------------------------------------------------------------------------
//   PushBuffer( WCHAR c )
// Adds a character in the buffer.
//-----------------------------------------------------------------------------

void PushBuffer( WCHAR c )
{
  CONSOLE_SCREEN_BUFFER_INFO Info;

  ChPrev = c;

  if (c == '\n')
  {
    if (pState->crm)
      ChBuffer[nCharInBuffer++] = c;
    FlushBuffer();
    if (wm)
    {
      MoveDown( TRUE );
      return;
    }
    // Avoid writing the newline if wrap has already occurred.
    GetConsoleScreenBufferInfo( hConOut, &Info );
    if (pState->crm)
    {
      // If we're displaying controls, then the only way we can be on the left
      // margin is if wrap occurred.
      if (CUR.X != 0)
	MoveDown( TRUE );
    }
    else
    {
      BOOL nl = TRUE;
      if (nWrapped)
      {
	// It's wrapped, but was anything more written?  Look at the current
	// row, checking that each character is space in current attributes.
	// If it's all blank we can drop the newline.  If the cursor isn't
	// already at the margin, then it was spaces or tabs that caused the
	// wrap, which can be ignored and overwritten.
	CHAR_INFO blank;
	PCHAR_INFO row = HeapAlloc( hHeap, 0, WIDTH * sizeof(CHAR_INFO) );
	if (row != NULL)
	{
	  COORD s, c;
	  SMALL_RECT r;
	  s.X = WIDTH;
	  s.Y = 1;
	  c.X = c.Y = 0;
	  r.Left = LEFT;
	  r.Right = RIGHT;
	  r.Top = r.Bottom = CUR.Y;
	  ReadConsoleOutput( hConOut, row, s, c, &r );
	  blank.Char.UnicodeChar = ' ';
	  blank.Attributes = ATTR;
	  while (*(PDWORD)&row[c.X] == *(PDWORD)&blank)
	  {
	    if (++c.X == s.X)
	    {
	      if (CUR.X != 0)
	      {
		CUR.X = 0;
		SetConsoleCursorPos( hConOut, CUR );
	      }
	      nl = FALSE;
	      break;
	    }
	  }
	  HeapFree( hHeap, 0, row );
	}
	nWrapped = 0;
      }
      if (nl)
	MoveDown( TRUE );
    }
    return;
  }
  if (!pState->crm)
  {
    if (nCharInBuffer > 0 && ChBuffer[nCharInBuffer-1] == '\r')
    {
      if (c == '\r') return; // \r\r\r... == \r, thus \r\r\n == \r\n
      FlushBuffer();
      if (nWrapped)
      {
	GetConsoleScreenBufferInfo( hConOut, &Info );
	CUR.Y -= nWrapped;
	if (CUR.Y < 0) CUR.Y = 0;
	if (pState->tb_margins && CUR.Y < TOP) CUR.Y = TOP;
	set_pos( LEFT, CUR.Y );
      }
    }
    if (c == '\b')
    {
      FlushBuffer();
      if (nWrapped)
      {
	GetConsoleScreenBufferInfo( hConOut, &Info );
	if (CUR.X == LEFT)
	{
	  CUR.X = RIGHT;
	  CUR.Y--;
	  SetConsoleCursorPos( hConOut, CUR );
	  --nWrapped;
	  return;
	}
      }
    }
  }
  if (shifted && c >= FIRST_G1 && c <= LAST_G1)
    c = G1[c-FIRST_G1];
  ChBuffer[nCharInBuffer] = c;
  if (++nCharInBuffer == BUFFER_SIZE)
    FlushBuffer();
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

  in = HeapAlloc( hHeap, HEAP_ZERO_MEMORY, 2 * lstrlen( seq ) * sizeof(*in) );
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

void send_palette_sequence( COLORREF c )
{
  BYTE	r, g, b;
  TCHAR buf[16];

  r = GetRValue( c );
  g = GetGValue( c );
  b = GetBValue( c );
  if ((c & 0x0F0F0F) == ((c >> 4) & 0x0F0F0F))
    ac_wprintf( buf, "#%X%X%X", r & 0xF, g & 0xF, b & 0xF );
  else
    ac_wprintf( buf, "#%2X%2X%2X", r, g, b );
  SendSequence( buf );
}


// Clear existing tabs and set tab stops at every size columns.
void init_tabs( int size )
{
  int i;

  RtlZeroMemory( pState->tab_stop, MAX_TABS );
  for (i = 0; i < MAX_TABS; i += size)
    pState->tab_stop[i] = TRUE;
  pState->tabs = TRUE;
}


// Find the "distance" between two colors.
// https://www.compuphase.com/cmetric.htm
int color_distance( COLORREF c1, COLORREF c2 )
{
  int rmean = (GetRValue( c1 ) + GetRValue( c2 )) / 2;
  int r = GetRValue( c1 ) - GetRValue( c2 );
  int g = GetGValue( c1 ) - GetGValue( c2 );
  int b = GetBValue( c1 ) - GetBValue( c2 );
  return (((512 + rmean) * r * r) >> 8) +
	 4 * g * g +
	 (((767 - rmean) * b * b) >> 8);
}

// Find the nearest color to a system color.
int find_nearest_color( COLORREF col )
{
  int d, d_min;
  int i, idx;
  CONSOLE_SCREEN_BUFFER_INFOX csbix;
  const COLORREF* table;

  csbix.cbSize = sizeof(csbix);
  table = (GetConsoleScreenBufferInfoX( hConOut, &csbix ))
	  ? csbix.ColorTable : legacy_palette;

  d_min = color_distance( col, table[0] );
  if (d_min == 0) return 0;
  idx = 0;
  for (i = 1; i < 16; ++i)
  {
    d = color_distance( col, table[i] );
    if (d < d_min)
    {
      if (d == 0) return i;
      d_min = d;
      idx = i;
    }
  }

  return idx;
}


// ========== Reset

void InterpretEscSeq( void );

void Reset( BOOL hard )
{
  CONSOLE_CURSOR_INFO CursInfo;
  CONSOLE_SCREEN_BUFFER_INFOX csbix;

  GetConsoleCursorInfo( hConOut, &CursInfo );
  CursInfo.bVisible = TRUE;
  SetConsoleCursorInfo( hConOut, &CursInfo );
  im =
  pState->om =
  pState->crm =
  pState->tb_margins = FALSE;
  awm = TRUE;
  SetConsoleMode( hConOut, cache[0].mode | ENABLE_WRAP_AT_EOL_OUTPUT );
  shifted = G0_special = SaveG0 = FALSE;
  pState->SavePos.X = pState->SavePos.Y = 0;
  pState->SaveAttr = 0;
  es_argv[0] = es_argc = 0;
  prefix = '[';
  prefix2 = suffix2 = 0;
  suffix = 'm';
  InterpretEscSeq();

  if (hard)
  {
    pState->tabs =
    pState->noclear = FALSE;
    prefix2 = '?';
    es_argv[0] = 3; es_argc = 1;
    suffix2 = '+';
    suffix = 'l';
    InterpretEscSeq();
    screen_top = -1;
    csbix.cbSize = sizeof(csbix);
    if (GetConsoleScreenBufferInfoX( hConOut, &csbix ))
    {
      arrcpy( csbix.ColorTable, pState->o_palette );
      ++csbix.srWindow.Right;
      ++csbix.srWindow.Bottom;
      SetConsoleScreenBufferInfoX( hConOut, &csbix );
    }
    arrcpy( pState->x_palette, xterm_palette );
  }
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
  int  p1, p2;
  WORD attribut;
  CONSOLE_SCREEN_BUFFER_INFO Info;
  CONSOLE_CURSOR_INFO CursInfo;
  DWORD len, NumberOfCharsWritten;
  COORD Pos;
  SMALL_RECT Rect;
  CHAR_INFO  CharInfo;
  DWORD      mode;
  SHORT      top, bottom;

#define FillBlank( len, Pos ) \
  FillConsoleOutputCharacter( hConOut, ' ', len, Pos, &NumberOfCharsWritten );\
  FillConsoleOutputAttribute( hConOut, ATTR, len, Pos, &NumberOfCharsWritten )

  if (prefix == '[')
  {
    if (prefix2 == '?' && (suffix2 == 0 || suffix2 == '+'))
    {
      if (suffix == 'h' || suffix == 'l')
      {
	for (i = 0; i < es_argc; i++)
	  switch (es_argv[i])
	  {
	    case 25: // DECTCEM
	      GetConsoleCursorInfo( hConOut, &CursInfo );
	      CursInfo.bVisible = (suffix == 'h');
	      SetConsoleCursorInfo( hConOut, &CursInfo );
	    break;

	    case 7: // DECAWM
	      awm = (suffix == 'h');
	      mode = cache[0].mode;
	      if (awm)
		mode |= ENABLE_WRAP_AT_EOL_OUTPUT;
	      else
		mode &= ~ENABLE_WRAP_AT_EOL_OUTPUT;
	      SetConsoleMode( hConOut, mode );
	    break;

	    case 6: // DECOM
	      pState->om = (suffix == 'h');
	    break;

	    case 95: // DECNCSM
	      pState->noclear = (suffix == 'h');
	    break;

	    case 3: // DECCOLM
	    {
	      COORD buf;
	      SMALL_RECT win;

	      pState->tb_margins = FALSE;

	      buf.X = (suffix == 'l') ? pState->buf_width : 132;
	      if (buf.X != 0)
	      {
		GetConsoleScreenBufferInfo( hConOut, &Info );
		buf.Y = HEIGHT;
		win.Left = LEFT;
		win.Top = TOP;
		win.Bottom = BOTTOM;
		if (suffix == 'h')
		{
		  pState->buf_width = WIDTH;
		  pState->win_width = WIN.Right - WIN.Left;
		  win.Right = 131;
		}
		else
		{
		  win.Right = pState->win_width;
		  pState->buf_width = 0;
		}
		// The buffer cannot be smaller than the window; the window
		// cannot be bigger than the buffer.
		if (WIN.Right - WIN.Left > win.Right)
		{
		  SetConsoleWindowInfo( hConOut, TRUE, &win );
		  SetConsoleScreenBufferSize( hConOut, buf );
		}
		else
		{
		  SetConsoleScreenBufferSize( hConOut, buf );
		  SetConsoleWindowInfo( hConOut, TRUE, &win );
		}
	      }
	      // Even if the screen is not cleared, scroll in a new window the
	      // first time this is used.
	      if (pState->noclear &&
		  (suffix2 == '+' || (TOP == screen_top && CUR.Y != LAST)))
	      {
		set_pos( LEFT, (suffix2 == '+') ? 0 : TOP );
		break;
	      }
	      prefix2 = 0;
	      es_argv[0] = 2;
	      suffix = 'J';
	      break;
	    }
	}
      }
      else if (suffix == 'W') // DECST8C
      {
	if (es_argv[0] != 5 || es_argc > 2) return;
	if (es_argc == 1) es_argv[1] = 8;
	init_tabs( es_argv[1] );
	return;
      }
    }
    // Ignore any other private sequences.
    if (prefix2 != 0)
      return;

    // Even an explicit parameter of 0 should be defaulted to 1.
    p1 = (es_argv[0] == 0) ? 1 : es_argv[0];
    p2 = (es_argv[1] == 0) ? 1 : es_argv[1];

    GetConsoleScreenBufferInfo( hConOut, &Info );
    if (suffix2 == '+')
    {
      top    = 0;
      bottom = LAST;
    }
    else
    {
      top    = TOP;
      bottom = BOTTOM;
    }
    if (suffix2 == 0 || suffix2 == '+') switch (suffix)
    {
      case 'm': // SGR
      {
	BYTE b = FOREGROUND_INTENSITY;
	BYTE u = BACKGROUND_INTENSITY;

	if (es_argc == 0) es_argc++; // ESC[m == ESC[0m
	for (i = 0; i < es_argc; i++)
	{
	  if (30 <= es_argv[i] && es_argv[i] <= 37)
	  {
	    pState->sgr.foreground = es_argv[i] - 30;
	  }
	  else if (40 <= es_argv[i] && es_argv[i] <= 47)
	  {
	    pState->sgr.background = es_argv[i] - 40;
	  }
	  else if (90 <= es_argv[i] && es_argv[i] <= 97)
	  {
	    pState->sgr.foreground = es_argv[i] - 90 + 8;
	  }
	  else if (100 <= es_argv[i] && es_argv[i] <= 107)
	  {
	    pState->sgr.background = es_argv[i] - 100 + 8;
	  }
	  else if (es_argv[i] == 38 || es_argv[i] == 48)
	  {
	    // This is technically incorrect, but it's what xterm does, so
	    // that's what we do.  According to T.416 (ISO 8613-6), there is
	    // only one parameter, which is divided into elements.  So where
	    // xterm does "38;2;R;G;B" it should really be "38:2:I:R:G:B" (I is
	    // a color space identifier).
	    if (++i < es_argc)
	    {
	      COLORREF col = CLR_INVALID;
	      int idx = -1;
	      int arg = es_argv[i-1];

	      if (es_argv[i] == 2)		// rgb
	      {
		if (i+3 < es_argc)
		  col = RGB( es_argv[i+1], es_argv[i+2], es_argv[i+3] );
		i += 3;
	      }
	      else if (es_argv[i] == 5) 	// index
	      {
		if (++i < es_argc)
		{
		  if (es_argv[i] < 16)
		    idx = es_argv[i];
		  else if (es_argv[i] < 256)
		    col = pState->x_palette[es_argv[i] - 16];
		}
	      }
	      if (col != CLR_INVALID)
		idx = attr2ansi[find_nearest_color( col )];
	      if (idx != -1)
	      {
		if (arg == 38)
		{
		  pState->sgr.foreground = idx;
		  b = 0;
		}
		else
		{
		  pState->sgr.background = idx;
		  u = 0;
		}
	      }
	    }
	  }
	  else switch (es_argv[i])
	  {
	    case 0:
	    case 39:
	    case 49:
	    {
	      int a = get_default_attr( FALSE );
	      pState->sgr.reverse = FALSE;
	      if (a < 0)
	      {
		pState->sgr.reverse = TRUE;
		a = -a;
	      }
	      if (es_argv[i] != 49)
		pState->sgr.foreground = attr2ansi[a & 7];
	      if (es_argv[i] != 39)
		pState->sgr.background = attr2ansi[(a >> 4) & 7];
	      if (es_argv[i] == 0)
	      {
		if (es_argc == 1)
		{
		  pState->sgr.bold	= a & FOREGROUND_INTENSITY;
		  pState->sgr.underline = a & BACKGROUND_INTENSITY;
		}
		else
		{
		  pState->sgr.bold	= 0;
		  pState->sgr.underline = 0;
		}
		pState->sgr.rvideo    = 0;
		pState->sgr.concealed = 0;
	      }
	    }
	    break;

	    case  1: pState->sgr.bold	   = FOREGROUND_INTENSITY; break;
	    case  5: // blink
	    case  4: pState->sgr.underline = BACKGROUND_INTENSITY; break;
	    case  7: pState->sgr.rvideo    = 1; break;
	    case  8: pState->sgr.concealed = 1; break;
	    case 21: // oops, this actually turns on double underline
		     // but xterm turns off bold too, so that's alright
	    case 22: pState->sgr.bold	   = 0; break;
	    case 25:
	    case 24: pState->sgr.underline = 0; break;
	    case 27: pState->sgr.rvideo    = 0; break;
	    case 28: pState->sgr.concealed = 0; break;
	  }
	}
	b &= pState->sgr.bold;
	u &= pState->sgr.underline;
	if (pState->sgr.concealed)
	{
	  if (pState->sgr.rvideo)
	  {
	    attribut = foregroundcolor[pState->sgr.foreground]
		     | backgroundcolor[pState->sgr.foreground];
	    if (b)
	      attribut |= FOREGROUND_INTENSITY | BACKGROUND_INTENSITY;
	  }
	  else
	  {
	    attribut = foregroundcolor[pState->sgr.background]
		     | backgroundcolor[pState->sgr.background];
	    if (u)
	      attribut |= FOREGROUND_INTENSITY | BACKGROUND_INTENSITY;
	  }
	}
	else if (pState->sgr.rvideo)
	{
	  attribut = foregroundcolor[pState->sgr.background]
		   | backgroundcolor[pState->sgr.foreground];
	  if (b) attribut |= BACKGROUND_INTENSITY;
	  if (u) attribut |= FOREGROUND_INTENSITY;
	}
	else
	  attribut = foregroundcolor[pState->sgr.foreground] | b
		   | backgroundcolor[pState->sgr.background] | u;
	if (pState->sgr.reverse)
	  attribut = ((attribut >> 4) & 15) | ((attribut & 15) << 4);
	SetConsoleTextAttribute( hConOut, attribut );
      }
      return;

      case 'J': // ED
	if (es_argc > 1) return; // ESC[J == ESC[0J
	switch (es_argv[0])
	{
	  case 0: // ESC[0J erase from cursor to end of display
	    len = (bottom - CUR.Y) * WIDTH + WIDTH - CUR.X;
	    FillBlank( len, CUR );
	  return;

	  case 1: // ESC[1J erase from start to cursor.
	    Pos.X = LEFT;
	    Pos.Y = top;
	    len   = (CUR.Y - top) * WIDTH + CUR.X + 1;
	    FillBlank( len, Pos );
	  return;

	  case 2: // ESC[2J Clear screen and home cursor
	    if (suffix2 != '+' && (TOP != screen_top || CUR.Y == LAST))
	    {
	      // Rather than clearing the existing window, make the current
	      // line the new top of the window (assuming this is the first
	      // thing a program does).
	      int range = BOTTOM - TOP;
	      if (CUR.Y + range < HEIGHT)
	      {
		TOP = CUR.Y;
		BOTTOM = TOP + range;
	      }
	      else
	      {
		BOTTOM = LAST;
		TOP = BOTTOM - range;
		Rect.Left = LEFT;
		Rect.Right = RIGHT;
		Rect.Top = CUR.Y - TOP;
		Rect.Bottom = CUR.Y - 1;
		Pos.X = Pos.Y = 0;
		CharInfo.Char.UnicodeChar = ' ';
		CharInfo.Attributes = ATTR;
		ScrollConsoleScreenBuffer(hConOut, &Rect, NULL, Pos, &CharInfo);
	      }
	      SetConsoleWindowInfo( hConOut, TRUE, &WIN );
	      screen_top = TOP;
	      top = TOP;
	      bottom = BOTTOM;
	    }
	    Pos.X = LEFT;
	    Pos.Y = top;
	    len   = (bottom - top + 1) * WIDTH;
	    FillBlank( len, Pos );
	    // Not technically correct, but perhaps expected.
	    set_pos( Pos.X, Pos.Y );
	  return;
	}
      return;

      case 'K': // EL
	if (es_argc > 1) return; // ESC[K == ESC[0K
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
	}
      return;

      case 'X': // ECH - ESC[#X Erase # characters.
	if (es_argc > 1) return; // ESC[X == ESC[1X
	FillBlank( p1, CUR );
      return;

      case 'r': // DECSTBM - ESC[#;#r Set top and bottom margins.
	if (es_argc == 0 && suffix2 == '+')
	{
	  pState->tb_margins = FALSE; // ESC[+r == remove margins
	  return;
	}
	if (es_argc > 2) return;
	if (es_argv[1] == 0) es_argv[1] = BOTTOM - TOP + 1;
	pState->top_margin = p1 - 1;
	pState->bot_margin = es_argv[1] - 1;
	if (pState->bot_margin > BOTTOM - TOP)
	  pState->bot_margin = BOTTOM - TOP;
	if (pState->top_margin >= pState->bot_margin)
	  return; // top must be less than bottom
	pState->tb_margins = TRUE;
	set_pos( LEFT, pState->om ? TOP + pState->top_margin : TOP );
      return;

      case 'S': // SU - ESC[#S Scroll up/Pan down.
      case 'T': // SD - ESC[#T Scroll down/Pan up.
	if (es_argc > 1) return; // ESC[S == ESC[1S
	Pos.X	   =
	Rect.Left  = LEFT;
	Rect.Right = RIGHT;
	if (pState->tb_margins)
	{
	  Rect.Top    = TOP + pState->top_margin;
	  Rect.Bottom = TOP + pState->bot_margin;
	}
	else
	{
	  Rect.Top    = top;
	  Rect.Bottom = bottom;
	}
	Pos.Y = Rect.Top + (suffix == 'T' ? p1 : -p1);
	CharInfo.Char.UnicodeChar = ' ';
	CharInfo.Attributes = get_default_attr( TRUE );
	ScrollConsoleScreenBuffer( hConOut, &Rect, &Rect, Pos, &CharInfo );
      return;

      case 'L': // IL - ESC[#L Insert # blank lines.
      case 'M': // DL - ESC[#M Delete # lines.
	if (es_argc > 1) return; // ESC[L == ESC[1L
	Pos.X	   =
	Rect.Left  = LEFT;
	Rect.Right = RIGHT;
	Rect.Top   = CUR.Y;
	if (pState->tb_margins)
	{
	  if (CUR.Y < TOP + pState->top_margin ||
	      CUR.Y > TOP + pState->bot_margin) return;
	  Rect.Bottom = TOP + pState->bot_margin;
	}
	else
	{
	  Rect.Bottom = bottom;
	}
	Pos.Y = Rect.Top + (suffix == 'L' ? p1 : -p1);
	CharInfo.Char.UnicodeChar = ' ';
	CharInfo.Attributes = ATTR;
	ScrollConsoleScreenBuffer( hConOut, &Rect, &Rect, Pos, &CharInfo );
	// Technically should home the cursor, but perhaps not expected.
      return;

      case '@': // ICH - ESC[#@ Insert # blank characters.
      case 'P': // DCH - ESC[#P Delete # characters.
	if (es_argc > 1) return; // ESC[P == ESC[1P
	Rect.Left   = CUR.X;
	Rect.Right  = RIGHT;
	Rect.Top    =
	Rect.Bottom = CUR.Y;
	if (suffix == '@')
	  CUR.X += p1;
	else
	  CUR.X -= p1;
	CharInfo.Char.UnicodeChar = ' ';
	CharInfo.Attributes = ATTR;
	ScrollConsoleScreenBuffer( hConOut, &Rect, &Rect, CUR, &CharInfo );
      return;

      case 'k': // VPB - ESC[#k
      case 'A': // CUU - ESC[#A Moves cursor up # lines
      case 'F': // CPL - ESC[#F Moves cursor up # lines, column 1.
	if (es_argc > 1) return; // ESC[A == ESC[1A
	Pos.Y = CUR.Y - p1;
	if (pState->tb_margins && (pState->om ||
				   CUR.Y >= TOP + pState->top_margin))
	  top = TOP + pState->top_margin;
	if (Pos.Y < top) Pos.Y = top;
	set_pos( (suffix == 'F') ? LEFT : CUR.X, Pos.Y );
      return;

      case 'e': // VPR - ESC[#e
      case 'B': // CUD - ESC[#B Moves cursor down # lines
      case 'E': // CNL - ESC[#E Moves cursor down # lines, column 1.
	if (es_argc > 1) return; // ESC[B == ESC[1B
	Pos.Y = CUR.Y + p1;
	if (pState->tb_margins && (pState->om ||
				   CUR.Y <= TOP + pState->bot_margin))
	  bottom = TOP + pState->bot_margin;
	if (Pos.Y > bottom) Pos.Y = bottom;
	set_pos( (suffix == 'E') ? LEFT : CUR.X, Pos.Y );
      return;

      case 'a': // HPR - ESC[#a
      case 'C': // CUF - ESC[#C Moves cursor forward # spaces
	if (es_argc > 1) return; // ESC[C == ESC[1C
	Pos.X = CUR.X + p1;
	if (Pos.X > RIGHT) Pos.X = RIGHT;
	set_pos( Pos.X, CUR.Y );
      return;

      case 'j': // HPB - ESC[#j
      case 'D': // CUB - ESC[#D Moves cursor back # spaces
	if (es_argc > 1) return; // ESC[D == ESC[1D
      cub:
	Pos.X = CUR.X - p1;
	while (Pos.X < LEFT && nWrapped-- && CUR.Y != top)
	{
	  Pos.X += WIDTH;
	  CUR.Y--;
	}
	if (Pos.X < LEFT) Pos.X = LEFT;
	set_pos( Pos.X, CUR.Y );
      return;

      case '`': // HPA - ESC[#`
      case 'G': // CHA - ESC[#G Moves cursor column # in current row.
	if (es_argc > 1) return; // ESC[G == ESC[1G
	Pos.X = p1 - 1;
	if (Pos.X > RIGHT) Pos.X = RIGHT;
	set_pos( Pos.X, CUR.Y );
      return;

      case 'f': // HVP - ESC[#;#f
      case 'H': // CUP - ESC[#;#H Moves cursor to line #, column #
	if (es_argc > 2) return; // ESC[H == ESC[1;1H  ESC[#H == ESC[#;1H
	CUR.X = p2 - 1;
	if (CUR.X > RIGHT) CUR.X = RIGHT;
	--es_argc; // so we can fall through

      case 'd': // VPA - ESC[#d Moves cursor row #, current column.
	if (es_argc > 1) return; // ESC[d == ESC[1d
	if (pState->tb_margins && pState->om)
	{
	  top = TOP + pState->top_margin;
	  bottom = TOP + pState->bot_margin;
	}
	Pos.Y = top + p1 - 1;
	if (Pos.Y < top) Pos.Y = top;
	if (Pos.Y > bottom) Pos.Y = bottom;
	set_pos( CUR.X, Pos.Y );
      return;

      case 'g': // TBC
	if (es_argc > 1) return; // ESC[g == ESC[0g
	switch (es_argv[0])
	{
	  case 0: // ESC[0g Clear tab at cursor
	    if (!pState->tabs) init_tabs( 8 );
	    if (CUR.X < MAX_TABS) pState->tab_stop[CUR.X] = FALSE;
	  return;

	  case 3: // ESC[3g Clear all tabs
	    RtlZeroMemory( pState->tab_stop, MAX_TABS );
	    pState->tabs = TRUE;
	  return;

	  case 8: // ESC[8g Let console handle tabs
	    pState->tabs = FALSE;
	  return;
	}
      return;

      case 'I': // CHT - ESC[#I Moves cursor forward # tabs
	if (es_argc > 1) return; // ESC[I == ESC[1I
	Pos.Y = CUR.Y;
	if (pState->tabs)
	{
	  Pos.X = CUR.X;
	  while (++Pos.X < MAX_TABS && (!pState->tab_stop[Pos.X] || --p1 > 0)) ;
	}
	else
	  Pos.X = (CUR.X & -8) + p1 * 8;
	if (Pos.X > RIGHT) Pos.X = RIGHT;
	// Don't use set_pos, the tabs could be discarded.
	SetConsoleCursorPos( hConOut, Pos );
      return;

      case 'Z': // CBT - ESC[#Z Moves cursor back # tabs
	if (es_argc > 1) return; // ESC[Z == ESC[1Z
	if (pState->tabs)
	{
	  Pos.X = (CUR.X < MAX_TABS) ? CUR.X : MAX_TABS;
	  while (--Pos.X > 0 && (!pState->tab_stop[Pos.X] || --p1 > 0)) ;
	}
	else
	{
	  if ((CUR.X & 7) == 0)
	     Pos.X = CUR.X - p1 * 8;
	  else
	     Pos.X = (CUR.X & -8) - (p1 - 1) * 8;
	  if (Pos.X < LEFT) Pos.X = LEFT;
	}
	set_pos( Pos.X, CUR.Y );
      return;

      case 'b': // REP - ESC[#b Repeat character
	if (es_argc > 1) return; // ESC[b == ESC[1b
	if (ChPrev == '\b') goto cub;
	while (--p1 >= 0)
	  PushBuffer( ChPrev );
      return;

      case 's': // SCOSC - ESC[s Saves cursor position for recall later
	if (es_argc != 0) return;
	pState->SavePos = CUR;
      return;

      case 'u': // SCORC - ESC[u Return to saved cursor position
	if (es_argc != 0) return;
	Pos = pState->SavePos;
	if (Pos.X > RIGHT) Pos.X = RIGHT;
	if (Pos.Y > LAST) Pos.Y = LAST;
	set_pos( Pos.X, Pos.Y );
      return;

      case 'c': // DA - ESC[#c Device attributes
	if (es_argc > 1 || es_argv[0] != 0) return; // ESC[c == ESC[0c
	SendSequence( L"\33[?62;1c" ); // VT220 with 132 columns
      return;

      case 'n': // DSR - ESC[#n Device status report
	if (es_argc != 1) return; // ESC[n == ESC[0n -> ignored
	switch (es_argv[0])
	{
	  case 5: // ESC[5n Report status
	    SendSequence( L"\33[0n" ); // "OK"
	  return;

	  case 6: // ESC[6n Report cursor position
	  {
	    TCHAR buf[32];
	    ac_wprintf( buf, "\33[%d;%d%cR",
			     CUR.Y - top + 1, CUR.X + 1,
			     (suffix2 == '+') ? '+' : '\0' );
	    SendSequence( buf );
	  }
	  return;
	}
      return;

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

      case 'h': // SM - ESC[#...h Set Mode
      case 'l': // RM - ESC[#...l Reset Mode
      {
	BOOL state = (suffix == 'h');
	for (i = 0; i < es_argc; i++)
	  if (suffix2 == '+') switch (es_argv[i])
	  {
	    case 1: // ACFM
	      pState->fm = state;
	    break;
	  }
	  else switch (es_argv[i])
	  {
	    case 3: // CRM
	      pState->crm = state;
	    break;

	    case 4: // IRM
	      im = state;
	    break;
	  }
	return;
      }

      default:
      return;
    }
    if (suffix2 == '!') switch (suffix)
    {
      case 'p': // DECSTR - ESC[!p Soft reset
	if (es_argc != 0) return;
	Reset( FALSE );
      return;

      default:
      return;
    }
    if (suffix2 == ',') switch (suffix)
    {
      case '~': // DECPS - ESC[#;#;#...,~ Play Sound
      {
	// Frequencies of notes obtained from:
	//	https://pages.mtu.edu/~suits/notefreqs.html
	//	http://www.liutaiomottola.com/formulae/freqtab.htm
	// This is different to what the VT520 manual has, but since that
	// only specifies four frequencies, so be it. I've also rounded to
	// even numbers, as the Beep function seems to stutter on odd.
	static const DWORD snd_freq[] = { 0,
//	C    C#/Db  D	 D#/Eb	E     F    F#/Gb  G    G#/Ab  A    A#/Bb  B
/* 5 */  524,  554,  588,  622,  660,  698,  740,  784,  830,  880,  932,  988,
/* 6 */ 1046, 1108, 1174, 1244, 1318, 1396, 1480, 1568, 1662, 1760, 1864, 1976,
/* 7 */ 2094
	};
	DWORD dur;
	if (es_argc < 2) return;
	dur = es_argv[1];
	if (dur <= 48)	// use 1/32 second
	  dur = 1000 * dur / 32;
	else if (dur > 8000)		// max out at 8 seconds
	  dur = 8000;
	if (es_argc == 2)		// no notes
	  Sleep( dur );
	else for (i = 2; i < es_argc; ++i)
	{
	  if (es_argv[0] == 0) // zero volume
	    Sleep( dur );
	  else
	    Beep( (es_argv[i] < lenof(snd_freq)) ? snd_freq[es_argv[i]]
						 : es_argv[i], dur );
	}
	return;
      }

      default:
      return;
    }
  }
  else // (prefix == ']')
  {
    // Ignore any "private" sequences.
    if (prefix2 != 0 || es_argc != 1)
      return;

    if (es_argv[0] == 0 || // ESC]0;titleST - icon (ignored) &
	es_argv[0] == 2)   // ESC]2;titleST - window
    {
      SetConsoleTitle( Pt_arg );
    }
    else if (es_argv[0] == 4 || // ESC]4;paletteST - set/get color(s)
	     es_argv[0] == 104) // ESC]104;paletteST - reset color(s)
    {
      CONSOLE_SCREEN_BUFFER_INFOX csbix;
      csbix.cbSize = sizeof(csbix);
      if (!GetConsoleScreenBufferInfoX( hConOut, &csbix ))
	memcpy( csbix.ColorTable, legacy_palette, sizeof(legacy_palette) );
      if (es_argv[0] == 4)
      {
	BYTE r, g, b;
	DWORD c;
	LPTSTR beg, end;
	BOOL started = FALSE;
	for (beg = Pt_arg;; beg = end + 1)
	{
	  i = (int)ac_wcstoul( beg, &end, 10 );
	  if (end == beg || (*end != ';' && *end != '\0') || i >= 256)
	    break;
	  if (end[2] == ';' || end[2] == '\0')
	  {
	    if (end[1] == '*')
	    {
	      SendSequence( L"\33]4;" );
	      end[1] = '\0';
	      SendSequence( beg );
	      if (i < 16)
		for (; i < 16; ++i)
		{
		  send_palette_sequence( csbix.ColorTable[attr2ansi[i]] );
		  SendSequence( (i == 15) ? L"\a" : L"," );
		}
	      else
		for (; i < 256; ++i)
		{
		  send_palette_sequence( pState->x_palette[i - 16] );
		  SendSequence( (i == 255) ? L"\a" : L"," );
		}
	    }
	    else if (end[1] == '?')
	    {
	      if (!started)
	      {
		SendSequence( L"\33]4" );
		started = TRUE;
	      }
	      SendSequence( L";" );
	      end[1] = '\0';
	      SendSequence( beg );
	      send_palette_sequence( (i < 16) ? csbix.ColorTable[attr2ansi[i]]
					      : pState->x_palette[i - 16] );
	    }
	    else
	      break;
	    end += (end[2] == '\0') ? 1 : 2;
	  }
	  else
	  {
	    if (started)
	    {
	      started = FALSE;
	      SendSequence( L"\a" );
	    }
	    for (beg = end + 1;; beg = end + 1)
	    {
	      BOOL valid;
	      if (*beg == '#')
	      {
		valid = TRUE;
		c = (DWORD)ac_wcstoul( ++beg, &end, 16 );
		if (end - beg == 3)
		{
		  r = (BYTE)(c >> 8);
		  g = (BYTE)(c >> 4) & 0xF;
		  b = (BYTE)c & 0xF;
		  r |= r << 4;
		  g |= g << 4;
		  b |= b << 4;
		}
		else if (end - beg == 6)
		{
		  r = (BYTE)(c >> 16);
		  g = (BYTE)(c >> 8);
		  b = (BYTE)c;
		}
		else
		  valid = FALSE;
	      }
	      else if (memcmp( beg, L"rgb:", 8 ) == 0)
	      {
		valid = FALSE;
		c = (DWORD)ac_wcstoul( beg += 4, &end, 16 );
		if (*end == '/' && (end - beg == 2 || end - beg == 4))
		{
		  r = (BYTE)(end - beg == 2 ? c : c >> 8);
		  c = (DWORD)ac_wcstoul( beg = end + 1, &end, 16 );
		  if (*end == '/' && (end - beg == 2 || end - beg == 4))
		  {
		    g = (BYTE)(end - beg == 2 ? c : c >> 8);
		    c = (DWORD)ac_wcstoul( beg = end + 1, &end, 16 );
		    if ((*end == ',' || *end == ';' || *end == '\0') &&
			(end - beg == 2 || end - beg == 4))
		    {
		      b = (BYTE)(end - beg == 2 ? c : c >> 8);
		      valid = TRUE;
		    }
		  }
		}
	      }
	      else
	      {
		valid = FALSE;
		c = (DWORD)ac_wcstoul( beg, &end, 10 );
		if (*end == ',' && c < 256)
		{
		  r = (BYTE)c;
		  c = (DWORD)ac_wcstoul( end + 1, &end, 10 );
		  if (*end == ',' && c < 256)
		  {
		    g = (BYTE)c;
		    c = (DWORD)ac_wcstoul( end + 1, &end, 10 );
		    if ((*end == ',' || *end == ';' || *end == '\0') && c < 256)
		    {
		      b = (BYTE)c;
		      valid = TRUE;
		    }
		  }
		}
	      }
	      if (valid)
	      {
		if (i < 16)
		  csbix.ColorTable[attr2ansi[i++]] = RGB( r, g, b );
		else
		  pState->x_palette[i++ - 16] = RGB( r, g, b );
	      }
	      if (*end != ',' || i == 256)
	      {
		while (*end != ';' && *end != '\0')
		  ++end;
		break;
	      }
	    }
	  }
	  if (*end != ';')
	    break;
	}
	if (started)
	  SendSequence( L"\a" );
      }
      else // (es_argv[0] == 104)
      {
	// Reset each index, or the entire palette.
	if (Pt_len == 0)
	{
	  arrcpy( csbix.ColorTable, pState->o_palette );
	  arrcpy( pState->x_palette, xterm_palette );
	}
	else
	{
	  LPTSTR beg, end;
	  for (beg = Pt_arg;; beg = end + 1)
	  {
	    i = (int)ac_wcstoul( beg, &end, 10 );
	    if (end == beg || (*end != ';' && *end != '\0') || i >= 256)
	      break;
	    if (i < 16)
	    {
	      i = attr2ansi[i];
	      csbix.ColorTable[i] = pState->o_palette[i];
	    }
	    else
	      pState->x_palette[i - 16] = xterm_palette[i - 16];
	    if (*end == '\0')
	      break;
	  }
	}
      }
      if (SetConsoleScreenBufferInfoX)
      {
	++csbix.srWindow.Right;
	++csbix.srWindow.Bottom;
	SetConsoleScreenBufferInfoX( hConOut, &csbix );
      }
    }
  }
}


void MoveDown( BOOL home )
{
  CONSOLE_SCREEN_BUFFER_INFO Info;
  SMALL_RECT Rect;
  COORD      Pos;
  CHAR_INFO  CharInfo;

  GetConsoleScreenBufferInfo( hConOut, &Info );
  if (pState->tb_margins && CUR.Y == TOP + pState->bot_margin)
  {
    Rect.Left = LEFT;
    Rect.Right = RIGHT;
    Rect.Top = TOP + pState->top_margin + 1;
    Rect.Bottom = TOP + pState->bot_margin;
    Pos.X = LEFT;
    Pos.Y = TOP + pState->top_margin;
    CharInfo.Char.UnicodeChar = ' ';
    CharInfo.Attributes = get_default_attr( TRUE );
    ScrollConsoleScreenBuffer( hConOut, &Rect, NULL, Pos, &CharInfo );
    if (home)
    {
      CUR.X = 0;
      SetConsoleCursorPos( hConOut, CUR );
    }
  }
  else if (pState->tb_margins && CUR.Y == BOTTOM)
  {
    if (home)
    {
      CUR.X = 0;
      SetConsoleCursorPos( hConOut, CUR );
    }
  }
  else if (CUR.Y == LAST)
  {
    Rect.Left = LEFT;
    Rect.Right = RIGHT;
    Rect.Top = 1;
    Rect.Bottom = LAST;
    Pos.X = Pos.Y = 0;
    CharInfo.Char.UnicodeChar = ' ';
    CharInfo.Attributes = get_default_attr( TRUE );
    ScrollConsoleScreenBuffer( hConOut, &Rect, NULL, Pos, &CharInfo );
    if (home)
    {
      CUR.X = 0;
      SetConsoleCursorPos( hConOut, CUR );
    }
  }
  else
  {
    if (home) CUR.X = 0;
    ++CUR.Y;
    SetConsoleCursorPos( hConOut, CUR );
  }
}

void MoveUp( void )
{
  CONSOLE_SCREEN_BUFFER_INFO Info;
  SMALL_RECT Rect;
  COORD      Pos;
  CHAR_INFO  CharInfo;

  GetConsoleScreenBufferInfo( hConOut, &Info );
  if (pState->tb_margins && CUR.Y == TOP + pState->top_margin)
  {
    Rect.Left = LEFT;
    Rect.Right = RIGHT;
    Rect.Top = TOP + pState->top_margin;
    Rect.Bottom = TOP + pState->bot_margin - 1;
    Pos.X = LEFT;
    Pos.Y = TOP + pState->top_margin + 1;
    CharInfo.Char.UnicodeChar = ' ';
    CharInfo.Attributes = get_default_attr( TRUE );
    ScrollConsoleScreenBuffer( hConOut, &Rect, NULL, Pos, &CharInfo );
  }
  else if (pState->tb_margins && CUR.Y == TOP)
  {
    // do nothing
  }
  else if (CUR.Y == 0)
  {
    Rect.Left = LEFT;
    Rect.Right = RIGHT;
    Rect.Top = 0;
    Rect.Bottom = LAST - 1;
    Pos.X = LEFT;
    Pos.Y = 1;
    CharInfo.Char.UnicodeChar = ' ';
    CharInfo.Attributes = get_default_attr( TRUE );
    ScrollConsoleScreenBuffer( hConOut, &Rect, NULL, Pos, &CharInfo );
  }
  else
  {
    --CUR.Y;
    SetConsoleCursorPos( hConOut, CUR );
  }
}


DWORD WINAPI FlushThread( LPVOID param )
{
  for (;;)
  {
    WaitForSingleObject( hFlushTimer, INFINITE );
    EnterCriticalSection( &CritSect );
    FlushBuffer();
    LeaveCriticalSection( &CritSect );
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

  EnterCriticalSection( &CritSect );

  if (hDev != hConOut)	// reinit if device has changed
  {
    FlushBuffer();
    hConOut = hDev;
    state = 1;
    im = shifted = G0_special = FALSE;
  }
  for (i = nNumberOfBytesToWrite, s = (LPCTSTR)lpBuffer; i > 0; i--, s++)
  {
    int c = *s; 		// more efficient to use int than short, fwiw

    if (state == 1)
    {
      if (c == ESC)
      {
	suffix2 = 0;
	ibytes = 0;
	get_state();
	state = (pState->crm) ? 7 : 2;
      }
      else if (pState->crm) PushBuffer( (WCHAR)c );
      else if (c == BEL)
      {
	if (PlaySound == NULL)
	{
	  winmm = LoadLibraryEx( L"winmm.dll", NULL, 0 );
	  if (winmm != NULL)
	    PlaySound = (FnPlaySound)GetProcAddress( winmm, "PlaySoundW" );
	  if (PlaySound == NULL)
	    PlaySound = INVALID_HANDLE_VALUE;
	}
	if (PlaySound == INVALID_HANDLE_VALUE)
	  PushBuffer( (WCHAR)c );
	else if (hBell == NULL)
	  hBell = CreateThread( NULL, 4096, BellThread, NULL, 0, NULL );
      }
      else if (c == SO) shifted = TRUE;
      else if (c == SI) shifted = G0_special;
      else if (c == HT && pState->tabs)
      {
	CONSOLE_SCREEN_BUFFER_INFO Info;
	FlushBuffer();
	GetConsoleScreenBufferInfo( hConOut, &Info );
	while (++CUR.X < MAX_TABS && !pState->tab_stop[CUR.X]) ;
	if (CUR.X > RIGHT) CUR.X = RIGHT;
	// Don't use set_pos, the tab could be discarded.
	SetConsoleCursorPos( hConOut, CUR );
      }
      else if (im && (c == HT || c == '\r' || c == '\b' || c == '\n'))
      {
	FlushBuffer();
	im = FALSE;
	PushBuffer( (WCHAR)c );
	FlushBuffer();
	im = TRUE;
      }
      else PushBuffer( (WCHAR)c );
    }
    else if (state == 2)
    {
      if (c < '\x20')
      {
	FlushBuffer();
	pState->crm = TRUE;
	ChBuffer[nCharInBuffer++] = c;	// skip newline handling
	FlushBuffer();
	pState->crm = FALSE;
	state = 1;
      }
      else if (c >= '\x20' && c <= '\x2f')
      {
	suffix2 = c;
	++ibytes;
      }
      else if (ibytes != 0)
      {
	if (ibytes == 1 &&
	    suffix2 == '(')     // SCS - Designate G0 character set
	{
	  if (c == '0')
	    shifted = G0_special = TRUE;
	  else if (c == 'B')
	    shifted = G0_special = FALSE;
	}
	state = 1;
      }
      else if (c == 'E')        // NEL Next Line
      {
	PushBuffer( '\n' );
	state = 1;
      }
      else if (c == 'D')        // IND Index
      {
	FlushBuffer();
	MoveDown( FALSE );
	state = 1;
      }
      else if (c == 'M')        // RI  Reverse Index
      {
	FlushBuffer();
	MoveUp();
        state = 1;
      }
      else if (c == 'H')        // HTS Character Tabulation Set
      {
	CONSOLE_SCREEN_BUFFER_INFO Info;
	if (!pState->tabs) init_tabs( 8 );
	FlushBuffer();
	GetConsoleScreenBufferInfo( hConOut, &Info );
	if (CUR.X < MAX_TABS) pState->tab_stop[CUR.X] = TRUE;
	state = 1;
      }
      else if (c == '7')        // DECSC Save Cursor
      {
	CONSOLE_SCREEN_BUFFER_INFO Info;
	FlushBuffer();
	GetConsoleScreenBufferInfo( hConOut, &Info );
	pState->SavePos = CUR;
	pState->SaveSgr = pState->sgr;
	pState->SaveAttr = ATTR;
	SaveG0 = G0_special;
	state = 1;
      }
      else if (c == '8')        // DECRC Restore Cursor
      {
	CONSOLE_SCREEN_BUFFER_INFO Info;
	FlushBuffer();
	GetConsoleScreenBufferInfo( hConOut, &Info );
	CUR = pState->SavePos;
	if (CUR.X > RIGHT) CUR.X = RIGHT;
	if (CUR.Y > LAST)  CUR.Y = LAST;
	set_pos( CUR.X, CUR.Y );
	if (pState->SaveAttr != 0)  // assume 0 means not saved
	{
	  pState->sgr = pState->SaveSgr;
	  SetConsoleTextAttribute( hConOut, pState->SaveAttr );
	  shifted = G0_special = SaveG0;
	}
	state = 1;
      }
      else if (c == 'c')        // RIS Reset to Initial State
      {
	Reset( TRUE );
      }
      else if (c == '[' ||      // CSI Control Sequence Introducer
	       c == ']')        // OSC Operating System Command
      {
	FlushBuffer();
	prefix = c;
	prefix2 = 0;
	es_argc = 0;
	es_argv[0] = es_argv[1] = 0;
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
	es_argv[0] = c - '0';
        state = 4;
      }
      else if (c == ';')
      {
        es_argc = 1;
        state = 4;
      }
      else if (c == ':')
      {
	// ignore it
      }
      else if (c >= '\x3c' && c <= '\x3f')
      {
	prefix2 = c;
      }
      else if (c >= '\x20' && c <= '\x2f')
      {
	suffix2 = c;
	++ibytes;
      }
      else if (ibytes > 1)
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
	if (es_argv[es_argc] > 32767) es_argv[es_argc] = 32767;
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
	++ibytes;
      }
      else if (ibytes > 1)
      {
	state = 1;
      }
      else if (prefix == ']')
      {
	es_argc++;
	state = 5;
	goto state5;
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
    state5:
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
	if (c == ESC) state = 7;
	else
	{
	  PushBuffer( (WCHAR)c );
	  state = 1;
	}
      }
    }
    else if (state == 9)
    {
      if (c == 'l')
      {
	FlushBuffer();
	pState->crm = FALSE;
	state = 1;
      }
      else
      {
	PushBuffer( ESC );
	PushBuffer( '[' );
	PushBuffer( '3' );
	if (c == ESC) state = 7;
	else
	{
	  PushBuffer( (WCHAR)c );
	  state = 1;
	}
      }
    }
  }
  if (nCharInBuffer > 0)
  {
    if (pState->fm && ChBuffer[nCharInBuffer-1] != '\r') FlushBuffer();
    else
    {
      LARGE_INTEGER due;
      due.QuadPart = -150000;
      if (hFlush == NULL)
	hFlush = CreateThread( NULL, 4096, FlushThread, NULL, 0, NULL );
      SetWaitableTimer( hFlushTimer, &due, 0, NULL, NULL, FALSE );
    }
  }
  if (lpNumberOfBytesWritten != NULL)
    *lpNumberOfBytesWritten = nNumberOfBytesToWrite - i;

  LeaveCriticalSection( &CritSect );

  return (i == 0);
}


// ========== Hooking API functions
//
// References about API hooking (and dll injection):
// - Matt Pietrek ~ Windows 95 System Programming Secrets.
// - Jeffrey Richter ~ Programming Applications for Microsoft Windows 4th ed.

const char APIKernel[]		   = "kernel32.dll";
const char APIcore[]		   = "api-ms-win-core-";
const char APIConsole[] 	   = "console-";
const char APIProcessThreads[]	   = "processthreads-";
const char APIProcessEnvironment[] = "processenvironment-";
const char APILibraryLoader[]	   = "libraryloader-";
const char APIFile[]		   = "file-";
const char APIHandle[]		   = "handle-";

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
  { APIHandle,		   sizeof(APIHandle) - 1,	      NULL },
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
    BOOL kernel = EOF;
    PSTR pszModName = MakeVA( PSTR, pImportDesc->Name );
    if (ac_strnicmp( pszModName, APIKernel, 8 ) == 0 &&
	(pszModName[8] == '\0' ||
	 ac_strnicmp( pszModName+8, APIKernel+8, 5 ) == 0))
    {
      kernel = TRUE;
    }
    else if (ac_strnicmp( pszModName, APIcore, 16 ) == 0)
    {
      PAPI_DATA lib;
      for (lib = APIs; lib->name; ++lib)
      {
	if (ac_strnicmp( pszModName+16, lib->name, lib->len ) == 0)
	{
	  if (lib->base == NULL)
	  {
	    lib->base = GetModuleHandleA( pszModName );
	    for (hook = Hooks; hook->name; ++hook)
	      if (hook->lib == lib->name)
		hook->apifunc = GetProcAddress( lib->base, hook->name );
	  }
	  kernel = FALSE;
	  break;
	}
      }
    }
    if (kernel == EOF)
    {
      if (log_level & 16)
	DEBUGSTR( 2, " %s%s %s", sp, zIgnoring, pszModName );
      continue;
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
	  else if (hook->myimport == 0)
	  {
	    patch = hook->newfunc;
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
      if (*(PDWORD)((PBYTE)me.hModule + 36) == 'ISNA')
      {
	VirtualProtect( (PBYTE)me.hModule + 36, 4, PAGE_READWRITE, &pr );
	*((PBYTE)me.hModule + 36+3) = 'U';
	VirtualProtect( (PBYTE)me.hModule + 36, 4, pr, &pr );
      }
      else if (*(PDWORD)((PBYTE)me.hModule + 0x3C) >= 0x40)
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
	lstrcpyn( app, pos, MAX_DEV_PATH );
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
	MultiByteToWideChar( CP_ACP, 0, pos, -1, app, MAX_DEV_PATH );
      }
      // CreateProcess only works with surrounding quotes ('"a name"' works,
      // but 'a" "name' fails), so that's all I'll test, too.  However, it also
      // tests for a file at each separator ('a name' tries "a.exe" before
      // "a name.exe") which I won't do.
      name = ac_wcspbrk( app, term );
      if (name != NULL)
	*name = '\0';
    }
  }
  else
  {
    if (wide)
      lstrcpyn( app, lpApp, MAX_DEV_PATH );
    else
      MultiByteToWideChar( CP_ACP, 0, lpApp, -1, app, MAX_DEV_PATH );
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
  if (!ansicon && search_env( L"ANSICON_EXC", name ))
  {
    DEBUGSTR( 1, "  Excluded" );
    type = 0;
  }
  else
  {
    type = ProcessType( child_pi, &base, &gui );
    if (!ansicon && gui && type > 0)
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
#if defined(_WIN64) || defined(W32ON64)
    if (type == 32)
      *(PDWORD)DllNameType = 0x320033/*L'23'*/;
    else
      *(PDWORD)DllNameType = 0x340036/*L'46'*/;
#endif
    if (GetFileAttributes( DllName ) == INVALID_FILE_ATTRIBUTES)
      type = 0;
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
      RemoteLoad64( child_pi );
    }
#else
#ifdef W32ON64
    if (type != 32)
    {
      TCHAR args[64];
      STARTUPINFO si;
      PROCESS_INFORMATION pi;
      memcpy( DllNameType, L"CON.exe", 16 );
      ac_wprintf( args, "ansicon -P%u", child_pi->dwProcessId );
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
      memcpy( DllNameType, L"32.dll", 14 );
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
    DWORD err = GetLastError();
    DEBUGSTR( 1, "  Failed (%u)", err );
    SetLastError( err );
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
    DWORD err = GetLastError();
    DEBUGSTR( 1, "  Failed (%u)", err );
    SetLastError( err );
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
  DWORD err = GetLastError();
  DEBUGSTR( 2, "LoadLibraryA %\"s", lpFileName );
  HookAPIAllMod( Hooks, FALSE, TRUE );
  SetLastError( err );
  return hMod;
}


HMODULE WINAPI MyLoadLibraryW( LPCWSTR lpFileName )
{
  HMODULE hMod = LoadLibraryW( lpFileName );
  DWORD err = GetLastError();
  DEBUGSTR( 2, "LoadLibraryW %\"S", lpFileName );
  HookAPIAllMod( Hooks, FALSE, TRUE );
  SetLastError( err );
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
    DWORD err = GetLastError();
    DEBUGSTR( 2, "LoadLibraryExA %\"s", lpFileName );
    HookAPIAllMod( Hooks, FALSE, TRUE );
    SetLastError( err );
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
    DWORD err = GetLastError();
    DEBUGSTR( 2, "LoadLibraryExW %\"S", lpFileName );
    HookAPIAllMod( Hooks, FALSE, TRUE );
    SetLastError( err );
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

  EnterCriticalSection( &CritSect );

  for (c = 0; c < CACHE; ++c)
    if (cache[c].h == h)
    {
      if (c != 0)
      {
	struct Cache tc = cache[c];
	do cache[c] = cache[c-1]; while (--c > 0);
	cache[0] = tc;
      }
      c = (cache[0].mode & ENABLE_PROCESSED_OUTPUT);
      LeaveCriticalSection( &CritSect );
      return c;
    }

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

  c = (cache[0].mode & ENABLE_PROCESSED_OUTPUT);

  LeaveCriticalSection( &CritSect );

  return c;
}

//-----------------------------------------------------------------------------
//   MySetConsoleMode
// It seems GetConsoleMode is a relatively slow function, so call it once and
// keep track of changes directly.
//-----------------------------------------------------------------------------
BOOL
WINAPI MySetConsoleMode( HANDLE hCon, DWORD mode )
{
  BOOL rc;

  FlushBuffer();

  rc = SetConsoleMode( hCon, mode );
  if (rc)
  {
    int c;
    for (c = 0; c < CACHE; ++c)
    {
      // The mode is associated with the buffer, not the handle.
      GetConsoleMode( cache[c].h, &cache[c].mode );
    }
    if (hCon == hConOut)
      awm = (mode & ENABLE_WRAP_AT_EOL_OUTPUT) ? TRUE : FALSE;
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
		 (write_func == NULL) ? "WriteConsoleA" : write_func,
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
	    RtlMoveMemory( mb, aBuf + pos, mb_len );
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
      // I set the number of characters actually written, which may be 0 when
      // multibyte characters are split across calls.  If that causes problems,
      // restore original behaviour.
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
  if (HandleToULong( hFile ) == STD_OUTPUT_HANDLE ||
      HandleToULong( hFile ) == STD_ERROR_HANDLE)
    hFile = GetStdHandle( HandleToULong( hFile ) );
  if (nNumberOfBytesToWrite != 0 && IsConsoleHandle( hFile ))
  {
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

static void log_CreateFile( HANDLE h, LPCVOID name, BOOL wide, DWORD access,
			    DWORD dwDesiredAccess, DWORD dwCreationDisposition )
{
  DWORD err = GetLastError();

  static char log[] = "CreateFile%s: %*s, %s, %s, %\"s";
  LPCSTR acc, op;
  char state[32];
  int  len;

  if (access != dwDesiredAccess)
    acc = "w->r/w";
  else if (access == (GENERIC_READ | GENERIC_WRITE) ||
	   (access & (FILE_READ_DATA | FILE_WRITE_DATA)) == (FILE_READ_DATA |
							     FILE_WRITE_DATA))
    acc = "r/w";
  else if (access == GENERIC_WRITE ||
	   access & (FILE_WRITE_DATA | FILE_APPEND_DATA))
    acc = "write";
  else if (access == GENERIC_READ ||
	   access & FILE_READ_DATA)
    acc = "read";
  else
    acc = "access";

  switch (dwCreationDisposition)
  {
    case CREATE_ALWAYS:     op = "create";   break;
    case CREATE_NEW:	    op = "new";      break;
    case OPEN_ALWAYS:	    op = "open";     break;
    case OPEN_EXISTING:     op = "existing"; break;
    case TRUNCATE_EXISTING: op = "truncate"; break;
    default:		    op = "unknown";  break;
  }

  if (h == INVALID_HANDLE_VALUE)
    len = ac_sprintf( state, "failed (%u)", err );
  else
  {
    state[0] = 'o';
    state[1] = 'k';
    len = 2;
  }
  log[sizeof(log) - 2] = wide ? 'S' : 's';
  DEBUGSTR( 1, log, wide ? "W" : "A", len, state, op, acc, name );

  SetLastError( err );
}

HANDLE
WINAPI MyCreateFileA( LPCSTR lpFileName, DWORD dwDesiredAccess,
		      DWORD dwShareMode,
		      LPSECURITY_ATTRIBUTES lpSecurityAttributes,
		      DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes,
		      HANDLE hTemplateFile )
{
  LPCSTR name = lpFileName;
  DWORD access = dwDesiredAccess;
  HANDLE h;

  if (dwDesiredAccess == GENERIC_WRITE)
  {
    PDWORD con = (PDWORD)lpFileName;
    if ((con[0] | 0x202020) == 'noc' ||
	((con[0] | 0x20202020) == 'onoc' && (con[1] | 0x2020) == '$tu'))
    {
      lpFileName = "CONOUT$";
      dwDesiredAccess |= GENERIC_READ;
    }
  }
  h = CreateFileA( lpFileName, dwDesiredAccess, dwShareMode,
		   lpSecurityAttributes, dwCreationDisposition,
		   dwFlagsAndAttributes, hTemplateFile );
  if (log_level & 32)
    log_CreateFile( h, name, FALSE, access,
		    dwDesiredAccess, dwCreationDisposition );
  return h;
}

HANDLE
WINAPI MyCreateFileW( LPCWSTR lpFileName, DWORD dwDesiredAccess,
		      DWORD dwShareMode,
		      LPSECURITY_ATTRIBUTES lpSecurityAttributes,
		      DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes,
		      HANDLE hTemplateFile )
{
  LPCWSTR name = lpFileName;
  DWORD access = dwDesiredAccess;
  HANDLE h;

  if (dwDesiredAccess == GENERIC_WRITE)
  {
#ifdef _WIN64
    __int64* con = (__int64*)lpFileName;
    if ((con[0] | 0x2000200020) == 0x6E006F0063/*L'noc'*/ ||
	((con[0] | 0x20002000200020) == 0x6F006E006F0063/*L'onoc'*/ &&
	 (con[1] | 0x200020) == 0x2400740075/*L'$tu'*/))
#else
    PDWORD con = (PDWORD)lpFileName;
    if ((con[0] | 0x200020) == 0x6F0063/*L'oc'*/ && ((con[1] | 0x20) == 'n' ||
	((con[1] | 0x200020) == 0x6F006E/*L'on'*/ &&
	 (con[2] | 0x200020) == 0x740075/*L'tu'*/ &&
	 (con[3] == '$'))))
#endif
    {
      lpFileName = L"CONOUT$";
      dwDesiredAccess |= GENERIC_READ;
    }
  }
  h = CreateFileW( lpFileName, dwDesiredAccess, dwShareMode,
		   lpSecurityAttributes, dwCreationDisposition,
		   dwFlagsAndAttributes, hTemplateFile );
  if (log_level & 32)
    log_CreateFile( h, name, TRUE, access,
		    dwDesiredAccess, dwCreationDisposition );
  return h;
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

BOOL
WINAPI MyCloseHandle( HANDLE hObject )
{
  int c;

  EnterCriticalSection( &CritSect );

  FlushBuffer();

  for (c = 0; c < CACHE; ++c)
    if (cache[c].h == hObject)
    {
      cache[c].h = INVALID_HANDLE_VALUE;
      break;
    }

  LeaveCriticalSection( &CritSect );

  return CloseHandle( hObject );
}


//-----------------------------------------------------------------------------
//   My...
// Flush the buffer before accessing the console.
//-----------------------------------------------------------------------------

#define FLUSH2( func, arg2 ) \
  BOOL WINAPI My##func( HANDLE a1, arg2 a2 )\
  { FlushBuffer(); return func( a1, a2 ); }

#define FLUSH2X( func, arg2 ) \
  BOOL WINAPI My##func##Ex( HANDLE a1, arg2 a2 )\
  { FlushBuffer(); return func##X( a1, a2 ); }

#define FLUSH3( func, arg2, arg3 ) \
  BOOL WINAPI My##func( HANDLE a1, arg2 a2, arg3 a3 )\
  { FlushBuffer(); return func( a1, a2, a3 ); }

#define FLUSH3X( func, arg2, arg3 ) \
  BOOL WINAPI My##func##Ex( HANDLE a1, arg2 a2, arg3 a3 )\
  { FlushBuffer(); return func##X( a1, a2, a3 ); }

#define FLUSH4( func, arg2, arg3, arg4 ) \
  BOOL WINAPI My##func( HANDLE a1, arg2 a2, arg3 a3, arg4 a4 )\
  { FlushBuffer(); return func( a1, a2, a3, a4 ); }

#define FLUSH5( func, arg2, arg3, arg4, arg5 ) \
  BOOL WINAPI My##func( HANDLE a1, arg2 a2, arg3 a3, arg4 a4, arg5 a5 )\
  { FlushBuffer(); return func( a1, a2, a3, a4, a5 ); }

FLUSH5( FillConsoleOutputAttribute,  WORD, DWORD, COORD, LPDWORD )
FLUSH5( FillConsoleOutputCharacterA, CHAR, DWORD, COORD, LPDWORD )
FLUSH5( FillConsoleOutputCharacterW, WCHAR, DWORD, COORD, LPDWORD )
FLUSH2( GetConsoleScreenBufferInfo,  PCONSOLE_SCREEN_BUFFER_INFO )
FLUSH2X( GetConsoleScreenBufferInfo, PCONSOLE_SCREEN_BUFFER_INFOX )
FLUSH5( ReadFile,     LPVOID, DWORD, LPDWORD, LPOVERLAPPED )
FLUSH5( ReadConsoleA, LPVOID, DWORD, LPDWORD, LPVOID )
FLUSH5( ReadConsoleW, LPVOID, DWORD, LPDWORD, LPVOID )
FLUSH4( ReadConsoleInputA, PINPUT_RECORD, DWORD, LPDWORD )
FLUSH4( ReadConsoleInputW, PINPUT_RECORD, DWORD, LPDWORD )
FLUSH5( ReadConsoleOutputA, PCHAR_INFO, COORD, COORD, PSMALL_RECT )
FLUSH5( ReadConsoleOutputW, PCHAR_INFO, COORD, COORD, PSMALL_RECT )
FLUSH5( ReadConsoleOutputAttribute, LPWORD, DWORD, COORD, LPDWORD )
FLUSH5( ReadConsoleOutputCharacterA, LPSTR, DWORD, COORD, LPDWORD )
FLUSH5( ReadConsoleOutputCharacterW, LPWSTR, DWORD, COORD, LPDWORD )
FLUSH5( ScrollConsoleScreenBufferA, SMALL_RECT*,SMALL_RECT*, COORD, CHAR_INFO* )
FLUSH5( ScrollConsoleScreenBufferW, SMALL_RECT*,SMALL_RECT*, COORD, CHAR_INFO* )
FLUSH2( SetConsoleCursorPosition, COORD )
FLUSH2X( SetConsoleScreenBufferInfo, PCONSOLE_SCREEN_BUFFER_INFOX )
FLUSH2( SetConsoleScreenBufferSize, COORD )
FLUSH2( SetConsoleTextAttribute, WORD )
FLUSH3( SetConsoleWindowInfo, BOOL, const SMALL_RECT* )
FLUSH3X( SetCurrentConsoleFont, BOOL, PCONSOLE_FONT_INFOX )
FLUSH5( WriteConsoleOutputA, const CHAR_INFO*, COORD, COORD, PSMALL_RECT )
FLUSH5( WriteConsoleOutputW, const CHAR_INFO*, COORD, COORD, PSMALL_RECT )
FLUSH5( WriteConsoleOutputAttribute, const WORD*, DWORD, COORD, LPDWORD )
FLUSH5( WriteConsoleOutputCharacterA, LPCSTR, DWORD, COORD, LPDWORD )
FLUSH5( WriteConsoleOutputCharacterW, LPCWSTR, DWORD, COORD, LPDWORD )


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
    if (!GetConsoleScreenBufferInfo( hConOut, &csbi ))
      RtlZeroMemory( &csbi, sizeof(csbi) );
    CloseHandle( hConOut );
    pcsbi = &csbi;
  }

  ac_wprintf( buf, "%dx%d (%dx%d)",
	      pcsbi->dwSize.X, pcsbi->dwSize.Y,
	      pcsbi->srWindow.Right - pcsbi->srWindow.Left + 1,
	      pcsbi->srWindow.Bottom - pcsbi->srWindow.Top + 1 );
  SetEnvironmentVariable( L"ANSICON", buf );
}

DWORD
WINAPI MyGetEnvironmentVariableA( LPCSTR lpName, LPSTR lpBuffer, DWORD nSize )
{
  if (lstrcmpiA( lpName, "ANSICON_VER" ) == 0)
  {
    if (nSize < sizeof(PVEREA))
      return sizeof(PVEREA);
    memcpy( lpBuffer, PVEREA, sizeof(PVEREA) );
    return sizeof(PVEREA) - 1;
  }

  if (lstrcmpiA( lpName, "CLICOLOR" ) == 0)
  {
    if (nSize < 2)
      return 2;
    lpBuffer[0] = '1';
    lpBuffer[1] = '\0';
    return 1;
  }

  if (lstrcmpiA( lpName, "ANSICON" ) == 0)
    set_ansicon( NULL );

  return GetEnvironmentVariableA( lpName, lpBuffer, nSize );
}

DWORD
WINAPI MyGetEnvironmentVariableW( LPCWSTR lpName, LPWSTR lpBuffer, DWORD nSize )
{
  if (lstrcmpi( lpName, L"ANSICON_VER" ) == 0)
  {
    if (nSize < lenof(PVERE))
      return lenof(PVERE);
    memcpy( lpBuffer, PVERE, sizeof(PVERE) );
    return lenof(PVERE) - 1;
  }

  if (lstrcmpi( lpName, L"CLICOLOR" ) == 0)
  {
    if (nSize < 2)
      return 2;
    lpBuffer[0] = '1';
    lpBuffer[1] = '\0';
    return 1;
  }

  if (lstrcmpi( lpName, L"ANSICON" ) == 0)
    set_ansicon( NULL );

  return GetEnvironmentVariableW( lpName, lpBuffer, nSize );
}


// ========== Initialisation

#define HOOK( dll, name ) { dll, #name, (PROC)My##name, NULL, NULL, NULL }

HookFn Hooks[] = {
  // These two are expected first!
  HOOK( APILibraryLoader,      LoadLibraryA ),
  HOOK( APILibraryLoader,      LoadLibraryW ),
  HOOK( APIProcessThreads,     CreateProcessA ),
  HOOK( APIProcessThreads,     CreateProcessW ),
  HOOK( APIProcessEnvironment, GetEnvironmentVariableA ),
  HOOK( APIProcessEnvironment, GetEnvironmentVariableW ),
  HOOK( APILibraryLoader,      GetProcAddress ),
  HOOK( APILibraryLoader,      LoadLibraryExA ),
  HOOK( APILibraryLoader,      LoadLibraryExW ),
  HOOK( APIConsole,	       SetConsoleMode ),
  HOOK( APIConsole,	       WriteConsoleA ),
  HOOK( APIConsole,	       WriteConsoleW ),
  HOOK( APIFile,	       WriteFile ),
  HOOK( APIKernel,	       _lwrite ),
  HOOK( APIProcessThreads,     ExitProcess ),
  HOOK( APILibraryLoader,      FreeLibrary ),
  HOOK( APIFile,	       CreateFileA ),
  HOOK( APIFile,	       CreateFileW ),
  HOOK( APIConsole,	       CreateConsoleScreenBuffer ),
  HOOK( APIHandle,	       CloseHandle ),
  HOOK( APIConsole,	       FillConsoleOutputAttribute ),
  HOOK( APIConsole,	       FillConsoleOutputCharacterA ),
  HOOK( APIConsole,	       FillConsoleOutputCharacterW ),
  HOOK( APIConsole,	       GetConsoleScreenBufferInfo ),
  HOOK( APIConsole,	       GetConsoleScreenBufferInfoEx ),
  HOOK( APIFile,	       ReadFile ),
  HOOK( APIConsole,	       ReadConsoleA ),
  HOOK( APIConsole,	       ReadConsoleW ),
  HOOK( APIConsole,	       ReadConsoleInputA ),
  HOOK( APIConsole,	       ReadConsoleInputW ),
  HOOK( APIConsole,	       ReadConsoleOutputA ),
  HOOK( APIConsole,	       ReadConsoleOutputW ),
  HOOK( APIConsole,	       ReadConsoleOutputAttribute ),
  HOOK( APIConsole,	       ReadConsoleOutputCharacterA ),
  HOOK( APIConsole,	       ReadConsoleOutputCharacterW ),
  HOOK( APIConsole,	       ScrollConsoleScreenBufferA ),
  HOOK( APIConsole,	       ScrollConsoleScreenBufferW ),
  HOOK( APIConsole,	       SetConsoleCursorPosition ),
  HOOK( APIConsole,	       SetConsoleScreenBufferInfoEx ),
  HOOK( APIConsole,	       SetConsoleScreenBufferSize ),
  HOOK( APIConsole,	       SetConsoleTextAttribute ),
  HOOK( APIConsole,	       SetConsoleWindowInfo ),
  HOOK( APIConsole,	       SetCurrentConsoleFontEx ),
  HOOK( APIConsole,	       WriteConsoleOutputA ),
  HOOK( APIConsole,	       WriteConsoleOutputW ),
  HOOK( APIConsole,	       WriteConsoleOutputAttribute ),
  HOOK( APIConsole,	       WriteConsoleOutputCharacterA ),
  HOOK( APIConsole,	       WriteConsoleOutputCharacterW ),
  { NULL, NULL, NULL, NULL, NULL, NULL }
};

//-----------------------------------------------------------------------------
//   OriginalAttr()
// Determine the original attributes for use by \e[m.
//-----------------------------------------------------------------------------
void OriginalAttr( PVOID lpReserved )
{
  HANDLE hConOut;
  CONSOLE_SCREEN_BUFFER_INFO Info;
  PIMAGE_DOS_HEADER pDosHeader;
  PIMAGE_NT_HEADERS pNTHeader;
  BOOL org;

  get_state();

  pDosHeader = (PIMAGE_DOS_HEADER)GetModuleHandle( NULL );
  pNTHeader = MakeVA( PIMAGE_NT_HEADERS, pDosHeader->e_lfanew );

  // If we were loaded dynamically, remember the current attributes to restore
  // upon unloading.  However, if we're the 64-bit DLL, but the image is 32-
  // bit, then the dynamic load was due to injecting into AnyCPU.  It may also
  // be dynamic due to lack of the IAT.
  if (lpReserved == NULL)
  {
    org = TRUE;
#ifdef _WIN64
    if (pNTHeader->FileHeader.Machine == IMAGE_FILE_MACHINE_I386)
      org = FALSE;
    else
#endif
    if (pNTHeader->DATADIRS <= IMAGE_DIRECTORY_ENTRY_IAT &&
	get_os_version() >= 0x602)
      org = FALSE;
  }
  else
  {
    // We also want to restore the original attributes for ansicon.exe.
    ansicon =
    org = (pNTHeader->OptionalHeader.MajorImageVersion == 20033 && // 'AN'
	   pNTHeader->OptionalHeader.MinorImageVersion == 18771);  // 'SI'
  }
  if (org)
  {
    hConOut = CreateFile( L"CONOUT$", GENERIC_READ | GENERIC_WRITE,
				      FILE_SHARE_READ | FILE_SHARE_WRITE,
				      NULL, OPEN_EXISTING, 0, NULL );
    if (!GetConsoleScreenBufferInfo( hConOut, &Info ))
      ATTR = 7;
    orgattr = ATTR;
    orgsgr  = pState->sgr;
    GetConsoleMode( hConOut, &orgmode );
    GetConsoleCursorInfo( hConOut, &orgcci );
    CloseHandle( hConOut );
  }
}


// A Win10 process that returns (rather than calling ExitProcess) may have a 30
// second delay before terminating.  This seems due to another thread being
// created for the console.  If the primary thread is detached, wait for it to
// finish, then explicitly exit.
DWORD WINAPI exit_thread( LPVOID lpParameter )
{
  DWORD rc;
  WaitForSingleObject( lpParameter, 30000 );
  GetExitCodeThread( lpParameter, &rc );
  ExitProcess( rc );
}


//-----------------------------------------------------------------------------
//   DllMain()
// Function called by the system when processes and threads are initialized
// and terminated.
//-----------------------------------------------------------------------------

BOOL WINAPI DllMain( HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved )
{
  BOOL	  bResult = TRUE;
  PHookFn hook;
  TCHAR   logstr[4];
  typedef LONG (WINAPI *PNTQIT)( HANDLE, int, PVOID, ULONG, PULONG );
  static PNTQIT NtQueryInformationThread;
  static DWORD primary_tid;

  if (dwReason == DLL_PROCESS_ATTACH)
  {
    hHeap = HeapCreate( 0, 0, 256 * 1024 );
    hKernel = GetModuleHandleA( APIKernel );
    GetConsoleScreenBufferInfoX = (PHCSBIX)GetProcAddress(
				     hKernel, "GetConsoleScreenBufferInfoEx" );
    if (GetConsoleScreenBufferInfoX == NULL)
      GetConsoleScreenBufferInfoX = GetConsoleScreenBufferInfoEx_repl;
    SetConsoleScreenBufferInfoX = (PHCSBIX)GetProcAddress(
				     hKernel, "SetConsoleScreenBufferInfoEx" );
    SetCurrentConsoleFontX	= (PHBCFIX)GetProcAddress(
				     hKernel, "SetCurrentConsoleFontEx" );

    *logstr = '\0';
    GetEnvironmentVariable( L"ANSICON_LOG", logstr, lenof(logstr) );
    log_level = ac_wtoi( logstr );
    prog = get_program_name( NULL );
#if defined(_WIN64) || defined(W32ON64)
    DllNameType = DllName - 6 +
#endif
    GetModuleFileName( hInstance, DllName, lenof(DllName) );
    set_ansi_dll();

    hDllInstance = hInstance; // save Dll instance handle
    DEBUGSTR( 1, "hDllInstance = %p", hDllInstance );

    // Get the entry points to the original functions.
    for (hook = Hooks; hook->name; ++hook)
      hook->oldfunc = GetProcAddress( hKernel, hook->name );

    // Get my import addresses, to detect if anyone's hooked me.
    DEBUGSTR( 2, "Storing my imports" );
    HookAPIOneMod( NULL, Hooks, FALSE, "" );

    bResult = HookAPIAllMod( Hooks, FALSE, FALSE );
    OriginalAttr( lpReserved );

    if (search_env( L"ANSICON_WRAP", prog ))
      wm = TRUE;

    NtQueryInformationThread = (PNTQIT)GetProcAddress(
		 GetModuleHandle( L"ntdll.dll" ), "NtQueryInformationThread" );

    InitializeCriticalSection( &CritSect );
    hFlushTimer = CreateWaitableTimer( NULL, FALSE, NULL );

    // If it's a static load, assume this is the primary thread.
    if (lpReserved)
      primary_tid = GetCurrentThreadId();

    if (NtQueryInformationThread == NULL && primary_tid == 0)
      DisableThreadLibraryCalls( hInstance );
  }
  else if (dwReason == DLL_PROCESS_DETACH)
  {
    CloseHandle( hFlushTimer );
    FlushBuffer();
    DeleteCriticalSection( &CritSect );
    if (hFlush != NULL)
    {
      TerminateThread( hFlush, 0 );
      CloseHandle( hFlush );
    }
    if (lpReserved == NULL)
    {
      DEBUGSTR( 1, "Unloading" );
      if (winmm != NULL)
	FreeLibrary( winmm );
    }
    else
    {
      DEBUGSTR( 1, "Terminating" );
    }
    HookAPIAllMod( Hooks, TRUE, FALSE );
    if (orgattr != 0)
    {
      hConOut = CreateFile( L"CONOUT$", GENERIC_READ | GENERIC_WRITE,
					FILE_SHARE_READ | FILE_SHARE_WRITE,
					NULL, OPEN_EXISTING, 0, NULL );
      SetConsoleTextAttribute( hConOut, orgattr );
      SetConsoleMode( hConOut, orgmode );
      SetConsoleCursorInfo( hConOut, &orgcci );
      CloseHandle( hConOut );
      pState->sgr = orgsgr;
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
    if (primary_tid && GetCurrentThreadId() == primary_tid)
    {
      HANDLE hThread;
      DuplicateHandle( GetCurrentProcess(), GetCurrentThread(),
		       GetCurrentProcess(), &hThread,
		       0, FALSE, DUPLICATE_SAME_ACCESS );
      CloseHandle( CreateThread( NULL, 4096, exit_thread, hThread, 0, NULL ) );
      DEBUGSTR( 1, "Primary thread detached, exiting process" );
    }
    else if (NtQueryInformationThread &&
	     NtQueryInformationThread( GetCurrentThread(),
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
