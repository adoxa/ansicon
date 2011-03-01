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
*/

#include "ansicon.h"
#include <tlhelp32.h>

#define isdigit(c) ('0' <= (c) && (c) <= '9')


// ========== Global variables and constants

// Macro for adding pointers/DWORDs together without C arithmetic interfering
#define MakePtr( cast, ptr, addValue ) (cast)((DWORD_PTR)(ptr)+(DWORD)(addValue))


const char APIKernel[]		   = "kernel32.dll";
const char APIKernelBase[]	   = "kernelbase.dll";
const char APIConsole[] 	   = "API-MS-Win-Core-Console-L1-1-0.dll";
const char APIProcessThreads[]	   = "API-MS-Win-Core-ProcessThreads-L1-1-0.dll";
const char APIProcessEnvironment[] = "API-MS-Win-Core-ProcessEnvironment-L1-1-0.dll";
const char APILibraryLoader[]	   = "API-MS-Win-Core-LibraryLoader-L1-1-0.dll";
const char APIFile[]		   = "API-MS-Win-Core-File-L1-1-0.dll";

PCSTR APIs[] =
{
  APIKernel,
  APIConsole,
  APIProcessThreads,
  APIProcessEnvironment,
  APILibraryLoader,
  APIFile,
  NULL
};


HMODULE   hKernel;		// Kernel32 module handle
HINSTANCE hDllInstance; 	// Dll instance handle
HANDLE	  hConOut;		// handle to CONOUT$

#define ESC	'\x1B'          // ESCape character
#define BEL	'\x07'
#define SO	'\x0E'          // Shift Out
#define SI	'\x0F'          // Shift In

#define MAX_ARG 16		// max number of args in an escape sequence
int   state;			// automata state
TCHAR prefix;			// escape sequence prefix ( '[', ']' or '(' );
TCHAR prefix2;			// secondary prefix ( '?' or '>' );
TCHAR suffix;			// escape sequence suffix
int   es_argc;			// escape sequence args count
int   es_argv[MAX_ARG]; 	// escape sequence args
TCHAR Pt_arg[MAX_PATH*2];	// text parameter for Operating System Command
int   Pt_len;
BOOL  shifted;


// DEC Special Graphics Character Set from
// http://vt100.net/docs/vt220-rm/table2-4.html
// Some of these may not look right, depending on the font and code page (in
// particular, the Control Pictures probably won't work at all).
const WCHAR G1[] =
{
  ' ',          // _ - blank
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

WORD foregroundcolor[8] =
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

WORD backgroundcolor[8] =
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


// screen attributes
WORD org_fg, org_bg, org_bold, org_ul;	// original attributes
WORD foreground;
WORD background;
WORD bold;
WORD underline;
WORD rvideo;
WORD concealed;

// saved cursor position
COORD SavePos;


// ========== Hooking API functions
//
// References about API hooking (and dll injection):
// - Matt Pietrek ~ Windows 95 System Programming Secrets.
// - Jeffrey Richter ~ Programming Applications for Microsoft Windows 4th ed.

typedef struct
{
  PCSTR lib;
  PSTR	name;
  PROC	newfunc;
  PROC	oldfunc;
  PROC	apifunc;
} HookFn, *PHookFn;

HookFn Hooks[];

//-----------------------------------------------------------------------------
//   HookAPIOneMod
// Substitute a new function in the Import Address Table (IAT) of the
// specified module.
// Return FALSE on error and TRUE on success.
//-----------------------------------------------------------------------------

BOOL HookAPIOneMod(
    HMODULE hFromModule,	// Handle of the module to intercept calls from
    PHookFn Hooks,		// Functions to replace
    BOOL    restore		// Restore the original functions
    )
{
  PIMAGE_DOS_HEADER	   pDosHeader;
  PIMAGE_NT_HEADERS	   pNTHeader;
  PIMAGE_IMPORT_DESCRIPTOR pImportDesc;
  PIMAGE_THUNK_DATA	   pThunk;
  PHookFn		   hook;

  // Tests to make sure we're looking at a module image (the 'MZ' header)
  pDosHeader = (PIMAGE_DOS_HEADER)hFromModule;
  if (pDosHeader->e_magic != IMAGE_DOS_SIGNATURE)
  {
    DEBUGSTR( L"error: %S(%d)", __FILE__, __LINE__ );
    return FALSE;
  }

  // The MZ header has a pointer to the PE header
  pNTHeader = MakePtr( PIMAGE_NT_HEADERS, pDosHeader, pDosHeader->e_lfanew );

  // One more test to make sure we're looking at a "PE" image
  if (pNTHeader->Signature != IMAGE_NT_SIGNATURE)
  {
    DEBUGSTR( L"error: %S(%d)", __FILE__, __LINE__ );
    return FALSE;
  }

  // We now have a valid pointer to the module's PE header.
  // Get a pointer to its imports section.
  pImportDesc = MakePtr( PIMAGE_IMPORT_DESCRIPTOR,
			 pDosHeader,
			 pNTHeader->OptionalHeader.
			  DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].
			   VirtualAddress );

  // Bail out if the RVA of the imports section is 0 (it doesn't exist)
  if (pImportDesc == (PIMAGE_IMPORT_DESCRIPTOR)pDosHeader)
  {
    return TRUE;
  }

  // Iterate through the array of imported module descriptors, looking
  // for the module whose name matches the pszFunctionModule parameter.
  for (; pImportDesc->Name; pImportDesc++)
  {
    PCSTR* lib;
    PSTR pszModName = MakePtr( PSTR, pDosHeader, pImportDesc->Name );
    for (lib = APIs; *lib; ++lib)
      if (_stricmp( pszModName, *lib ) == 0)
	break;
    if (*lib == NULL)
      continue;

    // Get a pointer to the found module's import address table (IAT).
    pThunk = MakePtr( PIMAGE_THUNK_DATA, pDosHeader, pImportDesc->FirstThunk );

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
	    patch = (lib == APIs) ? hook->oldfunc : hook->apifunc;
	}
	else if ((PROC)pThunk->u1.Function == hook->oldfunc ||
		 (PROC)pThunk->u1.Function == hook->apifunc)
	{
	  patch = hook->newfunc;
	}
	if (patch)
	{
	  DWORD flOldProtect, flNewProtect, flDummy;
	  MEMORY_BASIC_INFORMATION mbi;

	  DEBUGSTR( L"  %S", hook->name );
	  // Get the current protection attributes.
	  VirtualQuery( &pThunk->u1.Function, &mbi, sizeof(mbi) );
	  // Take the access protection flags.
	  flNewProtect = mbi.Protect;
	  // Remove ReadOnly and ExecuteRead flags.
	  flNewProtect &= ~(PAGE_READONLY | PAGE_EXECUTE_READ);
	  // Add on ReadWrite flag
	  flNewProtect |= (PAGE_READWRITE);
	  // Change the access protection on the region of committed pages in the
	  // virtual address space of the current process.
	  VirtualProtect( &pThunk->u1.Function, sizeof(PVOID),
			  flNewProtect, &flOldProtect );

	  // Overwrite the original address with the address of the new function.
	  if (!WriteProcessMemory( GetCurrentProcess(),
				   &pThunk->u1.Function,
				   &patch, sizeof(patch), NULL ))
	  {
	    DEBUGSTR( L"error: %S(%d)", __FILE__, __LINE__ );
	    return FALSE;
	  }

	  // Put the page attributes back the way they were.
	  VirtualProtect( &pThunk->u1.Function, sizeof(PVOID),
			  flOldProtect, &flDummy );
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

BOOL HookAPIAllMod( PHookFn Hooks, BOOL restore )
{
  HANDLE	hModuleSnap;
  MODULEENTRY32 me;
  BOOL		fOk;

  // Take a snapshot of all modules in the current process.
  hModuleSnap = CreateToolhelp32Snapshot( TH32CS_SNAPMODULE,
					  GetCurrentProcessId() );

  if (hModuleSnap == INVALID_HANDLE_VALUE)
  {
    DEBUGSTR( L"error: %S(%d)", __FILE__, __LINE__ );
    return FALSE;
  }

  // Fill the size of the structure before using it.
  me.dwSize = sizeof(MODULEENTRY32);

  // Walk the module list of the modules.
  for (fOk = Module32First( hModuleSnap, &me ); fOk;
       fOk = Module32Next( hModuleSnap, &me ))
  {
    // We don't hook functions in our own module.
    if (me.hModule != hDllInstance && me.hModule != hKernel)
    {
      DEBUGSTR( (restore) ? L"Unhooking from %s" : L"Hooking in %s",
		me.szModule );
      // Hook this function in this module.
      if (!HookAPIOneMod( me.hModule, Hooks, restore ))
      {
	CloseHandle( hModuleSnap );
	return FALSE;
      }
    }
  }
  CloseHandle( hModuleSnap );
  return TRUE;
}

// ========== Print Buffer functions

#define BUFFER_SIZE (278+512)		// fill out the section

int   nCharInBuffer;
WCHAR ChBuffer[BUFFER_SIZE];

//-----------------------------------------------------------------------------
//   FlushBuffer()
// Writes the buffer to the console and empties it.
//-----------------------------------------------------------------------------

void FlushBuffer( void )
{
  DWORD nWritten;
  if (nCharInBuffer <= 0) return;
  WriteConsole( hConOut, ChBuffer, nCharInBuffer, &nWritten, NULL );
  nCharInBuffer = 0;
}

//-----------------------------------------------------------------------------
//   PushBuffer( WCHAR c )
// Adds a character in the buffer.
//-----------------------------------------------------------------------------

void PushBuffer( WCHAR c )
{
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
  INPUT_RECORD in;
  HANDLE hStdIn = GetStdHandle( STD_INPUT_HANDLE );

  in.EventType = KEY_EVENT;
  in.Event.KeyEvent.bKeyDown = TRUE;
  in.Event.KeyEvent.wRepeatCount = 1;
  in.Event.KeyEvent.wVirtualKeyCode = 0;
  in.Event.KeyEvent.wVirtualScanCode = 0;
  in.Event.KeyEvent.dwControlKeyState = 0;
  for (; *seq; ++seq)
  {
    in.Event.KeyEvent.uChar.UnicodeChar = *seq;
    WriteConsoleInput( hStdIn, &in, 1, &out );
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
  WORD attribut;
  CONSOLE_SCREEN_BUFFER_INFO Info;
  CONSOLE_CURSOR_INFO CursInfo;
  DWORD len, NumberOfCharsWritten;
  COORD Pos;
  SMALL_RECT Rect;
  CHAR_INFO  CharInfo;

  if (prefix == '[')
  {
    if (prefix2 == '?' && (suffix == 'h' || suffix == 'l'))
    {
      if (es_argc == 1 && es_argv[0] == 25)
      {
	GetConsoleCursorInfo( hConOut, &CursInfo );
	CursInfo.bVisible = (suffix == 'h');
	SetConsoleCursorInfo( hConOut, &CursInfo );
	return;
      }
    }
    // Ignore any other \e[? or \e[> sequences.
    if (prefix2 != 0)
      return;

    GetConsoleScreenBufferInfo( hConOut, &Info );
    switch (suffix)
    {
      case 'm':
	if (es_argc == 0) es_argv[es_argc++] = 0;
	for (i = 0; i < es_argc; i++)
	{
	  switch (es_argv[i])
	  {
	    case 0:
	      foreground = org_fg;
	      background = org_bg;
	      bold	 = (es_argc == 1) ? org_bold : 0;
	      underline  = (es_argc == 1) ? org_ul   : 0;
	      rvideo	 = 0;
	      concealed  = 0;
	    break;
	    case  1: bold      = FOREGROUND_INTENSITY; break;
	    case  5: /* blink */
	    case  4: underline = BACKGROUND_INTENSITY; break;
	    case  7: rvideo    = 1; break;
	    case  8: concealed = 1; break;
	    case 21: bold      = 0; break;
	    case 25:
	    case 24: underline = 0; break;
	    case 27: rvideo    = 0; break;
	    case 28: concealed = 0; break;
	  }
	  if (30 <= es_argv[i] && es_argv[i] <= 37) foreground = es_argv[i]-30;
	  if (40 <= es_argv[i] && es_argv[i] <= 47) background = es_argv[i]-40;
	}
	if (concealed)
	{
	  if (rvideo)
	  {
	    attribut = foregroundcolor[foreground]
		     | backgroundcolor[foreground];
	    if (bold)
	      attribut |= FOREGROUND_INTENSITY | BACKGROUND_INTENSITY;
	  }
	  else
	  {
	    attribut = foregroundcolor[background]
		     | backgroundcolor[background];
	    if (underline)
	      attribut |= FOREGROUND_INTENSITY | BACKGROUND_INTENSITY;
	  }
	}
	else if (rvideo)
	{
	  attribut = foregroundcolor[background] | backgroundcolor[foreground];
	  if (bold)
	    attribut |= BACKGROUND_INTENSITY;
	  if (underline)
	    attribut |= FOREGROUND_INTENSITY;
	}
	else
	  attribut = foregroundcolor[foreground] | backgroundcolor[background]
		   | bold | underline;
	SetConsoleTextAttribute( hConOut, attribut );
      return;

      case 'J':
	if (es_argc == 0) es_argv[es_argc++] = 0; // ESC[J == ESC[0J
	if (es_argc != 1) return;
	switch (es_argv[0])
	{
	  case 0:		// ESC[0J erase from cursor to end of display
	    len = (Info.dwSize.Y - Info.dwCursorPosition.Y - 1) * Info.dwSize.X
		  + Info.dwSize.X - Info.dwCursorPosition.X - 1;
	    FillConsoleOutputCharacter( hConOut, ' ', len,
					Info.dwCursorPosition,
					&NumberOfCharsWritten );
	    FillConsoleOutputAttribute( hConOut, Info.wAttributes, len,
					Info.dwCursorPosition,
					&NumberOfCharsWritten );
	  return;

	  case 1:		// ESC[1J erase from start to cursor.
	    Pos.X = 0;
	    Pos.Y = 0;
	    len   = Info.dwCursorPosition.Y * Info.dwSize.X
		    + Info.dwCursorPosition.X + 1;
	    FillConsoleOutputCharacter( hConOut, ' ', len, Pos,
					&NumberOfCharsWritten );
	    FillConsoleOutputAttribute( hConOut, Info.wAttributes, len, Pos,
					&NumberOfCharsWritten );
	    return;

	  case 2:		// ESC[2J Clear screen and home cursor
	    Pos.X = 0;
	    Pos.Y = 0;
	    len   = Info.dwSize.X * Info.dwSize.Y;
	    FillConsoleOutputCharacter( hConOut, ' ', len, Pos,
					&NumberOfCharsWritten );
	    FillConsoleOutputAttribute( hConOut, Info.wAttributes, len, Pos,
					&NumberOfCharsWritten );
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
	  case 0:		// ESC[0K Clear to end of line
	    len = Info.srWindow.Right - Info.dwCursorPosition.X + 1;
	    FillConsoleOutputCharacter( hConOut, ' ', len,
					Info.dwCursorPosition,
					&NumberOfCharsWritten );
	    FillConsoleOutputAttribute( hConOut, Info.wAttributes, len,
					Info.dwCursorPosition,
					&NumberOfCharsWritten );
	  return;

	  case 1:		// ESC[1K Clear from start of line to cursor
	    Pos.X = 0;
	    Pos.Y = Info.dwCursorPosition.Y;
	    FillConsoleOutputCharacter( hConOut, ' ',
					Info.dwCursorPosition.X + 1, Pos,
					&NumberOfCharsWritten );
	    FillConsoleOutputAttribute( hConOut, Info.wAttributes,
					Info.dwCursorPosition.X + 1, Pos,
					&NumberOfCharsWritten );
	  return;

	  case 2:		// ESC[2K Clear whole line.
	    Pos.X = 0;
	    Pos.Y = Info.dwCursorPosition.Y;
	    FillConsoleOutputCharacter( hConOut, ' ', Info.dwSize.X, Pos,
					&NumberOfCharsWritten );
	    FillConsoleOutputAttribute( hConOut, Info.wAttributes,
					Info.dwSize.X, Pos,
					&NumberOfCharsWritten );
	  return;

	  default:
	  return;
	}

      case 'L':                 // ESC[#L Insert # blank lines.
	if (es_argc == 0) es_argv[es_argc++] = 1; // ESC[L == ESC[1L
	if (es_argc != 1) return;
	Rect.Left   = 0;
	Rect.Top    = Info.dwCursorPosition.Y;
	Rect.Right  = Info.dwSize.X - 1;
	Rect.Bottom = Info.dwSize.Y - 1;
	Pos.X = 0;
	Pos.Y = Info.dwCursorPosition.Y + es_argv[0];
	CharInfo.Char.UnicodeChar = ' ';
	CharInfo.Attributes = Info.wAttributes;
	ScrollConsoleScreenBuffer( hConOut, &Rect, NULL, Pos, &CharInfo );
      return;

      case 'M':                 // ESC[#M Delete # lines.
	if (es_argc == 0) es_argv[es_argc++] = 1; // ESC[M == ESC[1M
	if (es_argc != 1) return;
	if (es_argv[0] > Info.dwSize.Y - Info.dwCursorPosition.Y)
	  es_argv[0] = Info.dwSize.Y - Info.dwCursorPosition.Y;
	Rect.Left   = 0;
	Rect.Top    = Info.dwCursorPosition.Y + es_argv[0];
	Rect.Right  = Info.dwSize.X - 1;
	Rect.Bottom = Info.dwSize.Y - 1;
	Pos.X = 0;
	Pos.Y = Info.dwCursorPosition.Y;
	CharInfo.Char.UnicodeChar = ' ';
	CharInfo.Attributes = Info.wAttributes;
	ScrollConsoleScreenBuffer( hConOut, &Rect, NULL, Pos, &CharInfo );
      return;

      case 'P':                 // ESC[#P Delete # characters.
	if (es_argc == 0) es_argv[es_argc++] = 1; // ESC[P == ESC[1P
	if (es_argc != 1) return;
	if (Info.dwCursorPosition.X + es_argv[0] > Info.dwSize.X - 1)
	  es_argv[0] = Info.dwSize.X - Info.dwCursorPosition.X;
	Rect.Left   = Info.dwCursorPosition.X + es_argv[0];
	Rect.Top    = Info.dwCursorPosition.Y;
	Rect.Right  = Info.dwSize.X - 1;
	Rect.Bottom = Info.dwCursorPosition.Y;
	CharInfo.Char.UnicodeChar = ' ';
	CharInfo.Attributes = Info.wAttributes;
	ScrollConsoleScreenBuffer( hConOut, &Rect, NULL, Info.dwCursorPosition,
				   &CharInfo );
      return;

      case '@':                 // ESC[#@ Insert # blank characters.
	if (es_argc == 0) es_argv[es_argc++] = 1; // ESC[@ == ESC[1@
	if (es_argc != 1) return;
	if (Info.dwCursorPosition.X + es_argv[0] > Info.dwSize.X - 1)
	  es_argv[0] = Info.dwSize.X - Info.dwCursorPosition.X;
	Rect.Left   = Info.dwCursorPosition.X;
	Rect.Top    = Info.dwCursorPosition.Y;
	Rect.Right  = Info.dwSize.X - 1 - es_argv[0];
	Rect.Bottom = Info.dwCursorPosition.Y;
	Pos.X = Info.dwCursorPosition.X + es_argv[0];
	Pos.Y = Info.dwCursorPosition.Y;
	CharInfo.Char.UnicodeChar = ' ';
	CharInfo.Attributes = Info.wAttributes;
	ScrollConsoleScreenBuffer( hConOut, &Rect, NULL, Pos, &CharInfo );
      return;

      case 'A':                 // ESC[#A Moves cursor up # lines
	if (es_argc == 0) es_argv[es_argc++] = 1; // ESC[A == ESC[1A
	if (es_argc != 1) return;
	Pos.Y = Info.dwCursorPosition.Y - es_argv[0];
	if (Pos.Y < 0) Pos.Y = 0;
	Pos.X = Info.dwCursorPosition.X;
	SetConsoleCursorPosition( hConOut, Pos );
      return;

      case 'B':                 // ESC[#B Moves cursor down # lines
	if (es_argc == 0) es_argv[es_argc++] = 1; // ESC[B == ESC[1B
	if (es_argc != 1) return;
	Pos.Y = Info.dwCursorPosition.Y + es_argv[0];
	if (Pos.Y >= Info.dwSize.Y) Pos.Y = Info.dwSize.Y - 1;
	Pos.X = Info.dwCursorPosition.X;
	SetConsoleCursorPosition( hConOut, Pos );
      return;

      case 'C':                 // ESC[#C Moves cursor forward # spaces
	if (es_argc == 0) es_argv[es_argc++] = 1; // ESC[C == ESC[1C
	if (es_argc != 1) return;
	Pos.X = Info.dwCursorPosition.X + es_argv[0];
	if (Pos.X >= Info.dwSize.X) Pos.X = Info.dwSize.X - 1;
	Pos.Y = Info.dwCursorPosition.Y;
	SetConsoleCursorPosition( hConOut, Pos );
      return;

      case 'D':                 // ESC[#D Moves cursor back # spaces
	if (es_argc == 0) es_argv[es_argc++] = 1; // ESC[D == ESC[1D
	if (es_argc != 1) return;
	Pos.X = Info.dwCursorPosition.X - es_argv[0];
	if (Pos.X < 0) Pos.X = 0;
	Pos.Y = Info.dwCursorPosition.Y;
	SetConsoleCursorPosition( hConOut, Pos );
      return;

      case 'E':                 // ESC[#E Moves cursor down # lines, column 1.
	if (es_argc == 0) es_argv[es_argc++] = 1; // ESC[E == ESC[1E
	if (es_argc != 1) return;
	Pos.Y = Info.dwCursorPosition.Y + es_argv[0];
	if (Pos.Y >= Info.dwSize.Y) Pos.Y = Info.dwSize.Y - 1;
	Pos.X = 0;
	SetConsoleCursorPosition( hConOut, Pos );
      return;

      case 'F':                 // ESC[#F Moves cursor up # lines, column 1.
	if (es_argc == 0) es_argv[es_argc++] = 1; // ESC[F == ESC[1F
	if (es_argc != 1) return;
	Pos.Y = Info.dwCursorPosition.Y - es_argv[0];
	if (Pos.Y < 0) Pos.Y = 0;
	Pos.X = 0;
	SetConsoleCursorPosition( hConOut, Pos );
      return;

      case 'G':                 // ESC[#G Moves cursor column # in current row.
	if (es_argc == 0) es_argv[es_argc++] = 1; // ESC[G == ESC[1G
	if (es_argc != 1) return;
	Pos.X = es_argv[0] - 1;
	if (Pos.X >= Info.dwSize.X) Pos.X = Info.dwSize.X - 1;
	if (Pos.X < 0) Pos.X = 0;
	Pos.Y = Info.dwCursorPosition.Y;
	SetConsoleCursorPosition( hConOut, Pos );
      return;

      case 'f':                 // ESC[#;#f
      case 'H':                 // ESC[#;#H Moves cursor to line #, column #
	if (es_argc == 0)
	  es_argv[es_argc++] = 1; // ESC[H == ESC[1;1H
	if (es_argc == 1)
	  es_argv[es_argc++] = 1; // ESC[#H == ESC[#;1H
	if (es_argc > 2) return;
	Pos.X = es_argv[1] - 1;
	if (Pos.X < 0) Pos.X = 0;
	if (Pos.X >= Info.dwSize.X) Pos.X = Info.dwSize.X - 1;
	Pos.Y = es_argv[0] - 1;
	if (Pos.Y < 0) Pos.Y = 0;
	if (Pos.Y >= Info.dwSize.Y) Pos.Y = Info.dwSize.Y - 1;
	SetConsoleCursorPosition( hConOut, Pos );
      return;

      case 's':                 // ESC[s Saves cursor position for recall later
	if (es_argc != 0) return;
	SavePos = Info.dwCursorPosition;
      return;

      case 'u':                 // ESC[u Return to saved cursor position
	if (es_argc != 0) return;
	SetConsoleCursorPosition( hConOut, SavePos );
      return;

      case 'n':                 // ESC[#n Device status report
	if (es_argc != 1) return; // ESC[n == ESC[0n -> ignored
	switch (es_argv[0])
	{
	  case 5:		// ESC[5n Report status
	    SendSequence( L"\33[0n" ); // "OK"
	  return;

	  case 6:		// ESC[6n Report cursor position
	  {
	    TCHAR buf[32];
	    wsprintf( buf, L"\33[%d;%dR", Info.dwCursorPosition.Y + 1,
					  Info.dwCursorPosition.X + 1 );
	    SendSequence( buf );
	  }
	  return;

	  default:
	  return;
	}

      case 't':                 // ESC[#t Window manipulation
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

      default:
      return;
    }
  }
  else // (prefix == ']')
  {
    // Ignore any \e]? or \e]> sequences.
    if (prefix2 != 0)
      return;

    if (es_argc == 1 && es_argv[0] == 0) // ESC]0;titleST
    {
      SetConsoleTitle( Pt_arg );
    }
  }
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
    if (state == 1)
    {
      if (*s == ESC) state = 2;
      else if (*s == SO) shifted = TRUE;
      else if (*s == SI) shifted = FALSE;
      else PushBuffer( *s );
    }
    else if (state == 2)
    {
      if (*s == ESC) ;	// \e\e...\e == \e
      else if ((*s == '[') || (*s == ']'))
      {
	FlushBuffer();
	prefix = *s;
	prefix2 = 0;
	state = 3;
	Pt_len = 0;
	*Pt_arg = '\0';
      }
      else if (*s == ')' || *s == '(') state = 6;
      else state = 1;
    }
    else if (state == 3)
    {
      if (isdigit( *s ))
      {
        es_argc = 0;
	es_argv[0] = *s - '0';
        state = 4;
      }
      else if (*s == ';')
      {
        es_argc = 1;
        es_argv[0] = 0;
	es_argv[1] = 0;
        state = 4;
      }
      else if (*s == '?' || *s == '>')
      {
	prefix2 = *s;
      }
      else
      {
        es_argc = 0;
        suffix = *s;
        InterpretEscSeq();
        state = 1;
      }
    }
    else if (state == 4)
    {
      if (isdigit( *s ))
      {
	es_argv[es_argc] = 10 * es_argv[es_argc] + (*s - '0');
      }
      else if (*s == ';')
      {
        if (es_argc < MAX_ARG-1) es_argc++;
	es_argv[es_argc] = 0;
	if (prefix == ']')
	  state = 5;
      }
      else
      {
	es_argc++;
        suffix = *s;
        InterpretEscSeq();
        state = 1;
      }
    }
    else if (state == 5)
    {
      if (*s == BEL)
      {
	Pt_arg[Pt_len] = '\0';
        InterpretEscSeq();
        state = 1;
      }
      else if (*s == '\\' && Pt_len > 0 && Pt_arg[Pt_len-1] == ESC)
      {
	Pt_arg[--Pt_len] = '\0';
        InterpretEscSeq();
        state = 1;
      }
      else if (Pt_len < lenof(Pt_arg)-1)
	Pt_arg[Pt_len++] = *s;
    }
    else if (state == 6)
    {
      // Ignore it (ESC ) 0 is implicit; nothing else is supported).
      state = 1;
    }
  }
  FlushBuffer();
  if (lpNumberOfBytesWritten != NULL)
    *lpNumberOfBytesWritten = nNumberOfBytesToWrite - i;
  return( i == 0 );
}


// ========== Child process injection

// Inject code into the target process to load our DLL.
void Inject( LPPROCESS_INFORMATION pinfo, LPPROCESS_INFORMATION lpi,
	     DWORD dwCreationFlags )
{
  int type = ProcessType( pinfo );
  if (type != 0)
  {
    WCHAR dll[MAX_PATH];
#ifdef _WIN64
    DWORD len = GetModuleFileName( GetModuleHandleA( "ANSI64.dll" ),
				   dll, lenof(dll) );
    if (type == 32)
    {
      dll[len-6] = '3';
      dll[len-5] = '2';
      InjectDLL32( pinfo, dll );
    }
    else
    {
      InjectDLL64( pinfo, dll );
    }
#else
    GetModuleFileName( GetModuleHandleA( "ANSI32.dll" ), dll, lenof(dll) );
    InjectDLL32( pinfo, dll );
#endif
  }

  if (!(dwCreationFlags & CREATE_SUSPENDED))
    ResumeThread( pinfo->hThread );

  if (lpi)
  {
    memcpy( lpi, pinfo, sizeof(PROCESS_INFORMATION) );
  }
  else
  {
    CloseHandle( pinfo->hProcess );
    CloseHandle( pinfo->hThread );
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
  PROCESS_INFORMATION pi;

  if (!CreateProcessA( lpApplicationName,
		       lpCommandLine,
		       lpThreadAttributes,
		       lpProcessAttributes,
		       bInheritHandles,
		       dwCreationFlags | CREATE_SUSPENDED,
		       lpEnvironment,
		       lpCurrentDirectory,
		       lpStartupInfo,
		       &pi ))
    return FALSE;

  DEBUGSTR( L"CreateProcessA: \"%S\", \"%S\"",
	    (lpApplicationName == NULL) ? "" : lpApplicationName,
	    (lpCommandLine == NULL) ? "" : lpCommandLine );
  Inject( &pi, lpProcessInformation, dwCreationFlags );

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
  PROCESS_INFORMATION pi;

  if (!CreateProcessW( lpApplicationName,
		       lpCommandLine,
		       lpThreadAttributes,
		       lpProcessAttributes,
		       bInheritHandles,
		       dwCreationFlags | CREATE_SUSPENDED,
		       lpEnvironment,
		       lpCurrentDirectory,
		       lpStartupInfo,
		       &pi ))
    return FALSE;

  DEBUGSTR( L"CreateProcessW: \"%s\", \"%s\"",
	    (lpApplicationName == NULL) ? L"" : lpApplicationName,
	    (lpCommandLine == NULL) ? L"" : lpCommandLine );
  Inject( &pi, lpProcessInformation, dwCreationFlags );

  return TRUE;
}


FARPROC WINAPI MyGetProcAddress( HMODULE hModule, LPCSTR lpProcName )
{
  PHookFn hook;
  FARPROC proc;

  proc = GetProcAddress( hModule, lpProcName );

  if (proc)
  {
    if (hModule == hKernel)
    {
      // Ignore LoadLibrary so other hooks continue to work (our version
      // might end up at a different address).
      if (proc == Hooks[0].oldfunc || proc == Hooks[1].oldfunc)
	return proc;

      for (hook = Hooks+2; hook->name; ++hook)
      {
	if (proc == hook->oldfunc)
	{
	  DEBUGSTR( L"GetProcAddress: %S", lpProcName );
	  return hook->newfunc;
	}
      }
    }
    else if (Hooks[0].apifunc) // assume if one is defined, all are
    {
      if (proc == Hooks[0].apifunc || proc == Hooks[1].apifunc)
	return proc;

      for (hook = Hooks+2; hook->name; ++hook)
      {
	if (proc == hook->apifunc)
	{
	  DEBUGSTR( L"GetProcAddress: %S", lpProcName );
	  return hook->newfunc;
	}
      }
    }
  }

  return proc;
}


HMODULE WINAPI MyLoadLibraryA( LPCSTR lpFileName )
{
  HMODULE hMod = LoadLibraryA( lpFileName );
  if (hMod && hMod != hKernel)
  {
    DEBUGSTR( L"Hooking in %S (LoadLibraryA)", lpFileName );
    HookAPIOneMod( hMod, Hooks, FALSE );
  }
  return hMod;
}


HMODULE WINAPI MyLoadLibraryW( LPCWSTR lpFileName )
{
  HMODULE hMod = LoadLibraryW( lpFileName );
  if (hMod && hMod != hKernel)
  {
    DEBUGSTR( L"Hooking in %s (LoadLibraryW)", lpFileName );
    HookAPIOneMod( hMod, Hooks, FALSE );
  }
  return hMod;
}


HMODULE WINAPI MyLoadLibraryExA( LPCSTR lpFileName, HANDLE hFile,
				 DWORD dwFlags )
{
  HMODULE hMod = LoadLibraryExA( lpFileName, hFile, dwFlags );
  if (hMod && hMod != hKernel && !(dwFlags & LOAD_LIBRARY_AS_DATAFILE))
  {
    DEBUGSTR( L"Hooking in %S (LoadLibraryExA)", lpFileName );
    HookAPIOneMod( hMod, Hooks, FALSE );
  }
  return hMod;
}


HMODULE WINAPI MyLoadLibraryExW( LPCWSTR lpFileName, HANDLE hFile,
				 DWORD dwFlags )
{
  HMODULE hMod = LoadLibraryExW( lpFileName, hFile, dwFlags );
  if (hMod && hMod != hKernel && !(dwFlags & LOAD_LIBRARY_AS_DATAFILE))
  {
    DEBUGSTR( L"Hooking in %s (LoadLibraryExW)", lpFileName );
    HookAPIOneMod( hMod, Hooks, FALSE );
  }
  return hMod;
}


//-----------------------------------------------------------------------------
//   MyWrite...
// It is the new function that must replace the original Write... function.
// This function have exactly the same signature as the original one.
//-----------------------------------------------------------------------------

BOOL
WINAPI MyWriteConsoleA( HANDLE hCon, LPCVOID lpBuffer,
			DWORD nNumberOfCharsToWrite,
			LPDWORD lpNumberOfCharsWritten, LPVOID lpReserved )
{
  DWORD  Mode;
  LPWSTR buf;
  DWORD  len;
  BOOL	 rc = TRUE;

  // if we write in a console buffer with processed output
  if (GetConsoleMode( hCon, &Mode ) && (Mode & ENABLE_PROCESSED_OUTPUT))
  {
    UINT cp = GetConsoleOutputCP();
    DEBUGSTR( L"\33WriteConsoleA: %lu \"%.*S\"",
	      nNumberOfCharsToWrite, nNumberOfCharsToWrite, lpBuffer );
    len = MultiByteToWideChar( cp, 0, lpBuffer, nNumberOfCharsToWrite, NULL, 0 );
    buf = malloc( len * sizeof(WCHAR) );
    if (buf == NULL)
    {
      if (lpNumberOfCharsWritten != NULL)
	*lpNumberOfCharsWritten = 0;
      return (nNumberOfCharsToWrite == 0);
    }
    MultiByteToWideChar( cp, 0, lpBuffer, nNumberOfCharsToWrite, buf, len );
    rc = ParseAndPrintString( hCon, buf, len, lpNumberOfCharsWritten );
    free( buf );
    if (rc && lpNumberOfCharsWritten != NULL &&
	      *lpNumberOfCharsWritten != nNumberOfCharsToWrite)
    {
      // Converting a multibyte character to Unicode results in a different
      // "character" count.  This causes some programs to think not everything
      // was written, so the difference is sent again.	Fudge the (presumably)
      // correct count.
      TCHAR env[2048];
      if (GetEnvironmentVariable( L"ANSICON_API", env, lenof(env) ))
      {
	BOOL not;

	not = (*env == '!');
	if (not && env[1] == '\0')
	{
	  *lpNumberOfCharsWritten = nNumberOfCharsToWrite;
	}
	else
	{
	  TCHAR  path[MAX_PATH];
	  LPTSTR name, exe;

	  GetModuleFileName( NULL, path, lenof(path) );
	  name = wcsrchr( path, '\\' );
	  if (name == NULL)
	    name = path;
	  else
	    ++name;
	  exe = wcsrchr( name, '.' );
	  if (exe != NULL && exe != name)
	    *exe = '\0';
	  for (exe = wcstok( env + not, L";" ); exe; exe = wcstok( NULL, L";" ))
	  {
	    if (_wcsicmp( name, exe ) == 0)
	      break;
	  }
	  if ((exe && !not) || (!exe && not))
	    *lpNumberOfCharsWritten = nNumberOfCharsToWrite;
	}
      }
    }
    return rc;
  }
  else
  {
    return WriteConsoleA( hCon, lpBuffer,
			  nNumberOfCharsToWrite,
			  lpNumberOfCharsWritten,
			  lpReserved );
  }
}

BOOL
WINAPI MyWriteConsoleW( HANDLE hCon, LPCVOID lpBuffer,
			DWORD nNumberOfCharsToWrite,
			LPDWORD lpNumberOfCharsWritten, LPVOID lpReserved )
{
  DWORD Mode;
  if (GetConsoleMode( hCon, &Mode ) && (Mode & ENABLE_PROCESSED_OUTPUT))
  {
    DEBUGSTR( L"\33WriteConsoleW: %lu \"%.*s\"",
	      nNumberOfCharsToWrite, nNumberOfCharsToWrite, lpBuffer );
    return ParseAndPrintString( hCon, lpBuffer,
				nNumberOfCharsToWrite,
				lpNumberOfCharsWritten );
  }
  else
  {
    return WriteConsoleW( hCon, lpBuffer,
			  nNumberOfCharsToWrite,
			  lpNumberOfCharsWritten,
			  lpReserved );
  }
}

BOOL
WINAPI MyWriteFile( HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite,
		    LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED lpOverlapped )
{
  DWORD Mode;
  if (GetConsoleMode( hFile, &Mode ) && (Mode & ENABLE_PROCESSED_OUTPUT))
  {
    DEBUGSTR( L"WriteFile->" );
    return MyWriteConsoleA( hFile, lpBuffer,
			    nNumberOfBytesToWrite,
			    lpNumberOfBytesWritten,
			    lpOverlapped );
  }
  else	    // here, WriteFile is the old function (this module is not hooked)
  {
    return WriteFile( hFile, lpBuffer,
		      nNumberOfBytesToWrite,
		      lpNumberOfBytesWritten,
		      lpOverlapped );
  }
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
				      NULL, OPEN_EXISTING, 0, 0 );
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
  if (lstrcmpiA( lpName, "ANSICON" ) == 0)
    set_ansicon( NULL );
  return GetEnvironmentVariableA( lpName, lpBuffer, nSize );
}

DWORD
WINAPI MyGetEnvironmentVariableW( LPCWSTR lpName, LPWSTR lpBuffer, DWORD nSize )
{
  if (lstrcmpi( lpName, L"ANSICON" ) == 0)
    set_ansicon( NULL );
  return GetEnvironmentVariableW( lpName, lpBuffer, nSize );
}


// ========== Initialisation

HookFn Hooks[] = {
  // These two are expected first!
  { APILibraryLoader,	   "LoadLibraryA",            (PROC)MyLoadLibraryA,            NULL, NULL },
  { APILibraryLoader,	   "LoadLibraryW",            (PROC)MyLoadLibraryW,            NULL, NULL },
  { APIProcessThreads,	   "CreateProcessA",          (PROC)MyCreateProcessA,          NULL, NULL },
  { APIProcessThreads,	   "CreateProcessW",          (PROC)MyCreateProcessW,          NULL, NULL },
  { APIProcessEnvironment, "GetEnvironmentVariableA", (PROC)MyGetEnvironmentVariableA, NULL, NULL },
  { APIProcessEnvironment, "GetEnvironmentVariableW", (PROC)MyGetEnvironmentVariableW, NULL, NULL },
  { APILibraryLoader,	   "GetProcAddress",          (PROC)MyGetProcAddress,          NULL, NULL },
  { APILibraryLoader,	   "LoadLibraryExA",          (PROC)MyLoadLibraryExA,          NULL, NULL },
  { APILibraryLoader,	   "LoadLibraryExW",          (PROC)MyLoadLibraryExW,          NULL, NULL },
  { APIConsole, 	   "WriteConsoleA",           (PROC)MyWriteConsoleA,           NULL, NULL },
  { APIConsole, 	   "WriteConsoleW",           (PROC)MyWriteConsoleW,           NULL, NULL },
  { APIFile,		   "WriteFile",               (PROC)MyWriteFile,               NULL, NULL },
  { NULL, NULL, NULL, NULL }
};

//-----------------------------------------------------------------------------
//   OriginalAttr()
// Determine the original attributes for use by \e[m.
//-----------------------------------------------------------------------------
void OriginalAttr( void )
{
  static const char attr2ansi[8] =	// map console attribute to ANSI number
  {
    0, 4, 2, 6, 1, 5, 3, 7
  };
  HANDLE hConOut;
  CONSOLE_SCREEN_BUFFER_INFO csbi;

  hConOut = CreateFile( L"CONOUT$", GENERIC_READ | GENERIC_WRITE,
				    FILE_SHARE_READ | FILE_SHARE_WRITE,
				    NULL, OPEN_EXISTING, 0, 0 );
  if (!GetConsoleScreenBufferInfo( hConOut, &csbi ))
    csbi.wAttributes = 7;
  CloseHandle( hConOut );
  foreground = org_fg	= attr2ansi[csbi.wAttributes & 7];
  background = org_bg	= attr2ansi[(csbi.wAttributes >> 4) & 7];
  bold	     = org_bold = csbi.wAttributes & FOREGROUND_INTENSITY;
  underline  = org_ul	= csbi.wAttributes & BACKGROUND_INTENSITY;

  set_ansicon( &csbi );
}


//-----------------------------------------------------------------------------
//   DllMain()
// Function called by the system when processes and threads are initialized
// and terminated.
//-----------------------------------------------------------------------------

__declspec(dllexport) // to stop MinGW exporting everything
BOOL WINAPI DllMain( HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved )
{
  BOOL	  bResult = TRUE;
  HMODULE api;
  PHookFn hook;

  if (dwReason == DLL_PROCESS_ATTACH)
  {
    hDllInstance = hInstance; // save Dll instance handle
    DEBUGSTR( L"hDllInstance = %p", hDllInstance );

    // Get the entry points to the original functions.
    hKernel = GetModuleHandleA( APIKernel );
    for (hook = Hooks; hook->name; ++hook)
    {
      hook->oldfunc = GetProcAddress( hKernel, hook->name );
      api = GetModuleHandleA( hook->lib );
      if (api)
	hook->apifunc = GetProcAddress( api, hook->name );
    }

    bResult = HookAPIAllMod( Hooks, FALSE );
    OriginalAttr();
    DisableThreadLibraryCalls( hInstance );
  }
  else if (dwReason == DLL_PROCESS_DETACH)
  {
    if (lpReserved == NULL)
    {
      DEBUGSTR( L"Unloading" );
      HookAPIAllMod( Hooks, TRUE );
    }
    else
    {
      DEBUGSTR( L"Terminating" );
    }
  }

  return( bResult );
}
