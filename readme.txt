
				    ANSICON

			 Copyright 2005-2019 Jason Hood

			    Version 1.89.  Freeware


Description
===========

    ANSICON provides ANSI escape sequences for Windows console programs.  It
    provides much the same functionality as 'ANSI.SYS' does for MS-DOS.


Requirements
============

    32-bit: Windows 2000 Professional and later (it won't work with NT or 9X).
    64-bit: AMD64 (IA64 could work with a little modification).


Installation
============

    There are three ways to install, depending on your usage.

    * Add "x86" (if your OS is 32-bit) or "x64" (if 64-bit) to your PATH, or
      copy the relevant files to a directory already on the PATH (but NOT to
      "System32" on a 64-bit system).  This means you explicitly run 'ansicon'
      whenever you want to use it.

    * Use option '-i' (or '-I', if permitted) to add an entry to CMD.EXE's
      AutoRun registry value (current user or local machine, respectively).
      This means "Command Prompt" and any program started by CMD.EXE will
      automatically have sequences.

    * Add "d:\path\to\ansicon.exe -p" to your Startup group (run minimized to
      avoid the console window flashing).  This means any console program
      started by Explorer will automatically have sequences.

    Uninstall involves closing any programs that are currently using it; using
    the Run dialog to run "d:\path\to\ansicon.exe -pu" to remove it from
    Explorer; running with '-u' (and/or '-U') to remove it from AutoRun; remov-
    ing the directory from PATH; and deleting the files.  No other changes are
    made (unless you created environment variables).

Upgrading
---------

    Delete ANSICON_API - it has switched from WriteFile to WriteConsoleA.


Usage
=====

    Options (case sensitive):

	-l	Log to "%TEMP%\ansicon.log".

	-p	Enable the parent process (i.e. the command shell used to run
		ANSICON) to recognise escapes.

	-pu	Unload from the parent process, restoring it.

	-m	Set the current (and default) attribute to grey on black
		("monochrome"), or the attribute following the 'm' (please
		use 'COLOR /?' for attribute values).

	-e	Echo the command line - a space or tab after the 'e' is
		ignored, the remainder is displayed verbatim.

	-E	As above, but no newline is added.

	-t	Display ("type") each file (or standard input if none or the
		name is "-") as though they are a single file.

	-T	Display "==> FILE NAME <==", a blank line (or an error
		message), the file and another blank line.

    Running ANSICON with no arguments will start a new instance of the command
    processor (the program defined by the 'ComSpec' environment variable, typ-
    ically 'CMD.EXE'), or display standard input if it is redirected.  Any arg-
    ument will be treated as a program and its arguments.
    
    E.g.: 'ansicon -m30 -t file.ans' will display "file.ans" using black on
    cyan as the default color.

    The attribute may start with '-' to permanently reverse the foreground and
    background colors (but not when using '-p').  E.g.: 'ansicon -m-f0 -t
    file.log' will use reversed black on white as the default (i.e. white on
    black, with foreground sequences changing the background).

    If you experience trouble with certain programs, the log may help in find-
    ing the cause; it can be found at "%TEMP%\ansicon.log".  A number should
    immediately follow the 'l':

	0	No logging
	1	Log process start and end
	2	Above, plus log modules used by the process
	3	Above, plus log functions that are hooked
	4	Log console output (add to any of the above)
	8	Append to the existing file (add to any of the above)
       16	Log all imported modules (add to any of the above)
       32	Log CreateFile (add to any of the above)

    The log option will not work with '-p'; set the environment variable
    ANSICON_LOG (to the number) instead.  The variable is only read once when a
    process is started; changing it won't affect running processes.  If you
    identify a program or module that causes problems, add it to the
    ANSICON_EXC environment variable (see ANSICON_API below; add the extension
    to exclude a single module).  Be aware that excluding a program will also
    exclude any programs it creates (alghough excluding "program.exe" may still
    hook created programs run through its DLLs).

    E.g.: 'ansicon -l5' will start a new command processor, logging every pro-
    cess it starts along with their output.

    Once installed, the ANSICON environment variable will be created.  This
    variable is of the form "WxH (wxh)", where 'W' & 'H' are the width and
    height of the buffer and 'w' & 'h' are the width and height of the window.
    The variable is updated whenever a program reads it directly (i.e. as an
    individual request, not as part of the entire environment block).  For
    example, 'set an' will not update it, but 'echo %ansicon%' will.  Also
    created are ANSICON_VER, which contains the version without the point (1.80
    becomes "180"), and CLICOLOR (see http://bixense.com/clicolors/), which
    contains "1".  These variables do not exist as part of the environment
    block (e.g. 'set an' will not show ANSICON_VER).

    If installed, GUI programs will not be hooked.  Either start the program
    directly with 'ansicon', or add it to the ANSICON_GUI variable (see
    ANSICON_API below).

    Using 'ansicon' after install will always start with the default attrib-
    utes, restoring the originals on exit; all other programs will use the cur-
    rent attributes.  The shift state and insert mode are always reset for a
    new process.

    ANSICON will detect when a line wraps at the right margin and suppress a
    following newline.	Some programs detect the wrap themselves and so the
    following newline is actually for a blank line; use the ANSICON_WRAP
    variable to indicate as such (see ANSICON_API below).

    My version of WriteConsoleA will always set the number of characters writt-
    en, not the number of bytes.  This means writing a double-byte character as
    two bytes will set 0 the first write (nothing was written) and 1 the second
    (when the character was actually written); Windows normally sets 1 for both
    writes.  Similarly, writing the individual bytes of a multibyte character
    will set 0 for all but the last byte, then 1 on the last; Windows normally
    sets 1 for each byte, writing the undefined character.  However, my
    WriteFile (and _lwrite/_hwrite) will always set what was received; Windows,
    using a multibyte character set (but not DBCS), would set the characters.
    You can have WriteConsoleA return the original byte count by using the
    ANSICON_API environment variable:

	ANSICON_API=[!]program;program;program...

    PROGRAM is the name of the program, with no path and extension.  The lead-
    ing exclamation inverts the usage, meaning the API will always be over-
    ridden, unless the program is in the list.	The variable can be made perm-
    anent by going to System Properties, selecting the Advanced tab (with Vista
    onwards, this can be done by running "SystemPropertiesAdvanced") and click-
    ing Environment Variables.


Sequences Recognised
====================

    The following escape sequences are recognised (see "sequences.txt" for a
    more complete description).

	\e]0;titleBEL		xterm: Set window's title (and icon, ignored)
	\e]2;titleBEL		xterm: Set window's title
	\e]4;...BEL		xterm: Change color(s)
	\e]104;...BEL		xterm: Reset color(s)
	\e[21t			xterm: Report window's title
	\e[s			ANSI.SYS: Save Cursor Position
	\e[u			ANSI.SYS: Restore Cursor Position
	\e[1+h		ACFM	Flush Mode (flush immediately)
	\e[1+l		ACFM	Flush Mode (flush when necessary)
	BEL		BEL	Bell
	\e[#Z		CBT	Cursor Backward Tabulation
	\e[#G		CHA	Cursor Character Absolute
	\e[#I		CHT	Cursor Forward Tabulation
	\e[#E		CNL	Cursor Next Line
	\e[#F		CPL	Cursor Preceding Line
	\e[3h		CRM	Control Representation Mode (display controls)
	\e[3l		CRM	Control Representation Mode (perform controls)
	\e[#D		CUB	Cursor Left
	\e[#B		CUD	Cursor Down
	\e[#C		CUF	Cursor Right
	\e[#;#H 	CUP	Cursor Position
	\e[#A		CUU	Cursor Up
	\e[c		DA	Device Attributes
	\e[#P		DCH	Delete Character
	\e[?7h		DECAWM	Autowrap Mode (autowrap)
	\e[?7l		DECAWM	Autowrap Mode (no autowrap)
	\e[?3h		DECCOLM Selecting 80 or 132 Columns per Page (132)
	\e[?3l		DECCOLM Selecting 80 or 132 Columns per Page (prior)
	\e[?95h 	DECNCSM No Clearing Screen On Column Change Mode (keep)
	\e[?95l 	DECNCSM No Clearing Screen On Column Change Mode (clear)
	\e[?6h		DECOM	Origin Mode (top margin)
	\e[?6l		DECOM	Origin Mode (top line)
	\e[#;#;#...,~	DECPS	Play Sound
	\e8		DECRC	Restore Cursor
	\e7		DECSC	Save Cursor
	\e[?5W		DECST8C Set Tab at Every 8 Columns
	\e[?5;#W	DECST8C Set Tab at Every # Columns (ANSICON extension)
	\e[#;#r 	DECSTBM Set Top and Bottom Margins
	\e[!p		DECSTR	Soft Terminal Reset
	\e[?25h 	DECTCEM Text Cursor Enable Mode (show cursor)
	\e[?25l 	DECTCEM Text Cursor Enable Mode (hide cursor)
	\e[#M		DL	Delete Line
	\e[#n		DSR	Device Status Report
	\e[#X		ECH	Erase Character
	\e[#J		ED	Erase In Page
	\e[#K		EL	Erase In Line
	\e[#`		HPA	Character Position Absolute
	\e[#j		HPB	Character Position Backward
	\e[#a		HPR	Character Position Forward
	HT		HT	Character Tabulation
	\eH		HTS	Character Tabulation Set
	\e[#;#f 	HVP	Character And Line Position
	\e[#@		ICH	Insert Character
	\e[#L		IL	Insert Line
	\eD		IND	Index
	\e[4h		IRM	Insertion Replacement Mode (insert)
	\e[4l		IRM	Insertion Replacement Mode (replace)
	SI		LS0	Locking-shift Zero (see below)
	SO		LS1	Locking-shift One
	\eE		NEL	Next Line
	\e[#b		REP	Repeat
	\eM		RI	Reverse Index
	\ec		RIS	Reset to Initial State
	\e(0		SCS	Select Character Set (DEC special graphics)
	\e(B		SCS	Select Character Set (ASCII)
	\e[#;#;#m	SGR	Select Graphic Rendition
	\e[#T		SD	Scroll Down/Pan Up
	\e[#S		SU	Scroll Up/Pan Down
	\e[#g		TBC	Tabulation Clear
	\e[#d		VPA	Line Position Absolute
	\e[#k		VPB	Line Position Backward
	\e[#e		VPR	Line Position Forward

    '\e' represents the escape character (ASCII 27); '#' represents a decimal
    number (optional, in most cases defaulting to 1); BEL, HT, SO and SI are
    ASCII 7, 9, 14 and 15.

    Escape followed by a control character will display that character, not
    perform its function; an unrecognised character will preserve the escape.

    SO will select the G1 character set; SI will select the G0 set.  The G0
    character set is set by SCS; the G1 character set is always the DEC Special
    Graphics Character Set.

    I make a distinction between '\e[m' and '\e[0;...m'.  Both will restore the
    original foreground/background colors (and so '0' should be the first para-
    meter); the former will also restore the original bold and underline attri-
    butes, whilst the latter will explicitly reset them.  The environment var-
    iable ANSICON_DEF can be used to change the default colors (same value as
    '-m'; setting the variable does not change the current colors).

    The first time a program clears the screen ('\e[2J') will actually scroll
    in a new window (assuming the buffer is bigger than the window, of course).
    Subsequent clears will then blank the window.  However, if the window has
    scrolled, or the cursor is on the last line of the buffer, it will again
    scroll in a new window.


DEC Special Graphics Character Set
==================================

    This is my interpretation of the set, as shown by
    http://vt100.net/docs/vt220-rm/table2-4.html.


	Char	Unicode Code Point & Name
	----	-------------------------
	_	U+00A0	No-Break Space
	`	U+2666	Black Diamond Suit
	a	U+2592	Medium Shade
	b	U+2409	Symbol For Horizontal Tabulation
	c	U+240C	Symbol For Form Feed
	d	U+240D	Symbol For Carriage Return
	e	U+240A	Symbol For Line Feed
	f	U+00B0	Degree Sign
	g	U+00B1	Plus-Minus Sign
	h	U+2424	Symbol For Newline
	i	U+240B	Symbol For Vertical Tabulation
	j	U+2518	Box Drawings Light Up And Left
	k	U+2510	Box Drawings Light Down And Left
	l	U+250C	Box Drawings Light Down And Right
	m	U+2514	Box Drawings Light Up And Right
	n	U+253C	Box Drawings Light Vertical And Horizontal
	o	U+23BA	Horizontal Scan Line-1
	p	U+23BB	Horizontal Scan Line-3
	q	U+2500	Box Drawings Light Horizontal (SCAN 5)
	r	U+23BC	Horizontal Scan Line-7
	s	U+23BD	Horizontal Scan Line-9
	t	U+251C	Box Drawings Light Vertical And Right
	u	U+2524	Box Drawings Light Vertical And Left
	v	U+2534	Box Drawings Light Up And Horizontal
	w	U+252C	Box Drawings Light Down And Horizontal
	x	U+2502	Box Drawings Light Vertical
	y	U+2264	Less-Than Or Equal To
	z	U+2265	Greater-Than Or Equal To
	{	U+03C0	Greek Small Letter Pi
	|	U+2260	Not Equal To
	}	U+00A3	Pound Sign
	~	U+00B7	Middle Dot

    G1.txt is a Unicode file to view the glyphs "externally".  G1.bat is a
    batch file (using 'x86\ansicon') to show the glyphs in the console.  The
    characters will appear as they should using Lucida (other than the Sym-
    bols), but code page will influence them when using a raster font (but of
    particular interest, 437 and 850 both show the Box Drawings).


Limitations
===========

    Tabs can only be set up to column 2048.
    The saved position will not be restored correctly if the buffer scrolls.
    Palette sequences only work from Vista.

    There may be a conflict with NVIDIA's drivers, requiring the setting of the
    Environment Variable:

	ANSICON_EXC=nvd3d9wrap.dll;nvd3d9wrapx.dll

    An application using multiple screen buffers will not have separate
    attributes in each buffer.

    Console input that is echoed will not be processed (which is probably a
    good thing for escapes, but not so good for margins).


Version History
===============

    Legend: + added, - bug-fixed, * changed.

    1.89 - 29 April, 2019:
    - fix occasional freeze on startup (bug converting 8-digit window handle).

    1.88 - 1 March, 2019:
    - fix ANSICON environment variable when there is no console.

    1.87 - 3 February, 2019:
    - fix crash when some programs start (bug during hooking);
    - properly hook SetCurrentConsoleFontEx.

    1.86 - 4 November, 2018:
    - check the DLL exists before importing it (allows renaming to update);
    - unhook on terminate, as well (fixes issues with Vista and MinGW).

    1.85 - 23 August, 2018:
    - fix wrap issues with a buffer bigger than the window;
    - fix -e et al when redirecting to NUL;
    - prevent -p from injecting when already injected;
    - fix running directly via ansicon (hook even if it's GUI or excluded);
    - preserve last error;
    + add log level 32 to monitor CreateFile.

    1.84 - 11 May, 2018:
    - close the flush handles on detach;
    - WriteFile wasn't properly testing if its handle was for a console;
    - use remote load on Win8+ if the process has no IAT;
    - fix logging really long command lines;
    - default to 7 or -7 if ANSICON_DEF could not be parsed;
    - workaround for a Windows 10 1803 console bug (doubled CMD prompt);
    * remove dependency on CRT & USER32, dynamically load WINMM;
    * exit process if the primary thread is detached (for processes on Win10
      that return, rather than call ExitProcess);
    * ansicon.exe statically loads the DLL;
    * scrolling will use the default attribute for new lines.

    1.83 - 16 February, 2018:
    - create the flush thread on first use.

    1.82 - 13 February, 2018:
    - add ANSICON_WRAP for programs that expect the wrap at right margin;
    - make IsConsoleHandle a critical section, for multithreaded processes;
    - use APIConsole for all console functions (Windows 10).

    1.81 - 28 December, 2017:
    - fix multiple CRs before LF (including preventing an immediate flush);
    - fix CR, BS and partial RM during CRM;
    - fix buffer overflow caused by incorrect critical section;
    * support the entire 256-color palette;
    * setting color by index or RGB will use the nearest console color;
    * setting color by index will leave bold/underline unchanged.

    1.80 - 24 December, 2017:
    - fix unloading;
    - fix -e et al when redirecting to CON;
    - hook CreateFile and CreateConsoleScreenBuffer to force read/write access
      (fixes redirecting to CON and Microsoft's conio);
    - fix cursor report with duplicated digits (e.g. "11" was only writing "1");
    - fix issues with CRM;
    - fix explicit zero parameters not defaulting to 1;
    - set color by index (also setting bold/underline);
    - fix processes that start without a window;
    - hide the cursor when moving (prevent it displaying on the active buffer
      when moving on another);
    * use the system default sound for the bell;
    * limit parameters to a maximum value of 32767;
    * go back to saving the buffer cursor position;
    * preserve escape that isn't part of a sequence;
    * escaped control characters will display the control;
    * change the graphics SCAN characters to their Unicode equivalents;
    * BS/CR/CUB/HVP after wrap will move back to the previous line(s);
    * improve speed by only flushing when necessary, adding a mode to restore
      flushing immediately;
    + added DA, DECCOLM, DECNCSM, DECOM, DECPS, DECRC, DECSC, DECST8C, DECSTBM,
      DECSTR, HT, HTS, IND, IRM, NEL, RI, RIS, SCS (only G0 as Special/ASCII),
      SD, SU and TBC;
    + added '+' intermediate byte to use the buffer, rather than the window;
    + added set/get palette sequences;
    + added the bright SGR colors;
    + added -pu to unload from the parent.

    1.72 - 24 December, 2015:
    - handle STD_OUTPUT_HANDLE & STD_ERROR_HANDLE in WriteFile;
    - better handling of unusual PE files;
    * cache GetConsoleMode for an improvement in speed;
    * files writing to the console always succeed (should mostly remove the
      need for ANSICON_API);
    * log: add a blank line between processes;
	   remove the separate line for WriteFile & _lwrite;
	   write byte strings as-is, wide strings using the current code page;
	   use caret notation for control characters, with hexadecimal "^xNN"
	    and "^uNNNN" for characters not in the code page (custom printf);
    * join multibyte characters split across separate writes;
    * remove wcstok, avoiding potential interference with the host program;
    * similarly, remove malloc & friends, using a private heap;
    + add CLICOLOR dynamic environment variable.

    1.71 - 23 October, 2015:
    + add _CRT_NON_CONFORMING_WCSTOK define for VS2015.

    1.70 - 26 February, 2014:
    - don't hook again if using LoadLibrary or LoadLibraryEx;
    - update the LoadLibraryEx flags that shouldn't hook;
    - restore original attributes on detach (for LoadLibrary/FreeLibrary usage);
    - ansicon.exe will start with ANSICON_DEF (if defined and -m not used);
    - an installed ansicon.exe will restore current (not default) attributes;
    - attributes and saved position are local to each console window;
    - improved recognition of unsupported sequences;
    - restore cursor to bounds, if size reduced;
    - stop \e[K from erasing first character of next line;
    - restore cursor visibility on unload;
    * inject into a created process by modifying the import descriptor table
      (-p will use CreateRemoteThread);
    * log: remove the quotes around the CreateProcess command line;
	   add an underscore in 64-bit addresses to distinguish 8-digit groups;
    * ANSICON_EXC can exclude entire programs;
    * switch G1 blank from space (U+0020) to No-Break Space (U+00A0);
    * use window height, not buffer;
    * remove newline after wrap;
    + recognise more sequences.

    1.66 - 20 September, 2013:
    - fix 32-bit process trying to detect 64-bit process.

    1.65 - 4 September, 2013:
    - fix finding 32-bit LoadLibraryW address from 64-bit;
    - fix \e[K (was using window, not buffer).

    1.64 - 2 August, 2013:
    - improved detection of console output.

    1.63 - 25 July, 2013:
    - don't write the reset sequence (when it's already installed) if output is
      redirected.

    1.62 - 18 July, 2013:
    - indicate if opening HKLM failed;
    * removed ANSI-LLW.exe again, properly this time;
    * add the architecture (32- or 64-bit) to the log.

    1.61 - 14 February, 2013:
    * revert back to using ANSI-LLW.exe, as the new method was unreliable.

    1.60 - 24 November, 2012:
    * new method to get the 32-bit LoadLibraryW address from 64-bit code.
      This removes the need for ANSI-LLW.exe, which caused lots of virus
      warnings, for some reason.
    - set the code page to display some file names properly;
    + expand wildcards for -t (ignoring directories and hidden/binary files).

    1.53 - 12 June, 2012:
    - fix for multiple simultaneous process creation (e.g. "cl /MP ...").

    1.52 - 2 June, 2012:
    + 32-bit processes can inject into 64-bit processes;
    + implemented \e[39m & \e[49m (set default foreground/background color);
    + added \e[#X, \e[#`, \e[#a, \e[#d, \e[#e, \[e#j and \e[#k;
    * changed sequence descriptions to those in ECMA-48, ordered by acronym.

    1.51 - 24 February, 2012:
    - fixed installing into a piped/redirected CMD.EXE;
    - fixed 32-bit process trying to identify a 64-bit process;
    - ignore version within core API DLL names (now Win8 works);
    + hook _lwrite & _hwrite (now Silverfrost FTN95 v6.20 works).

    1.50 - 14 December, 2011:
    - -u does not imply -p;
    - return the program's exit code;
    - -p by itself will not restore original color;
    - output error messages to stderr;
    * logging is always available, with various levels; include the pid;
    * don't automatically hook GUI programs, use 'ansicon' or ANSICON_GUI;
    * always place first in AutoRun; don't run if already installed;
    + global reverse video capability;
    + added ANSICON_VER to provide version/install test;
    + added ANSICON_EXC to exclude selected modules;
    + added ANSICON_DEF to explicitly set the default SGM.

    1.40 - 1 March, 2011:
    - hook GetProcAddress (now PowerShell works);
    + add SO/SI, using the DEC Special Graphics Character Set for G1;
    + add DECTCEM to show/hide the cursor.

    1.32 - 22 December, 2010:
    - fixed crash due to NULL lpNumberOfBytesWritten/lpNumberOfCharsWritten;
    - -p will test the parent process for validity;
    * hook into GUI processes;
    + recognise DSR and xterm window title sequences;
    - fixed MinGW32 binaries (LLW was wrong).

    1.31 - 19 November, 2010:
    - fixed multibyte support (no extra junk with UTF-8 files);
    * provide workaround for API byte/character differences;
    * fixed potential problem if install path uses Unicode.

    1.30 - 7 September, 2010:
    + x64 version.

    1.25 - 22 July, 2010:
    - hook LoadLibraryEx (now CScript works);
    - fixed -i when AutoRun existed, but was empty;
    + support for Windows 7;
    + -I (and -U) use HKEY_LOCAL_MACHINE.

    1.24 - 7 January, 2010:
    - fix -t and -e when ANSICON was already running;
    + read standard input if redirected with no arguments, if -t has no
      files, or if the name is "-" (which also serves as a workaround for
      programs that don't get hooked, such as CScript).

    1.23 - 11 November, 2009:
    - restore hooked functions when unloading;
    - reverse the "bold" and "underline" settings;
    * conceal characters by making foreground color same as background.

    1.22 - 5 October, 2009:
    - hook LoadLibrary to inject into applications started via association.

    1.21 - 23 September, 2009:
    + -i (and -u) option to add (remove) entry to AutoRun value.

    1.20 - 21 June, 2009:
    * use another injection method;
    + create ANSICON environment variable;
    + -e (and -E) option to echo the command line (without newline);
    + -t (and -T) option to type (display) files (with file name).

    1.15 - 17 May, 2009:
    - fix output corruption for long (over 8192 characters) ANSI strings.

    1.14 - 3 April, 2009:
    - fix the test for an empty import section (eg. XCOPY now works).

    1.13 - 21 & 27 March, 2009:
    * use a new injection method (to work with DEP);
    * use Unicode.

    1.12 - 9 March, 2009:
    - fix processing child programs (generate a relocatable DLL).

    1.11 - 28 February, 2009:
    - fix processing child programs (only use for console executables).

    1.10 - 22 February, 2009:
    - fix output corruption (buffer overflow in MyConsoleWriteW);
    - recognise current screen attributes as current ANSI atrributes;
    - ignore Ctrl+C and Ctrl+Break;
    + process child programs.

    1.01 - 12 March, 2006:
    * \e[m will restore original color, not set grey on black;
    + -m option to set default (and initial) color;
    - restore original color on exit;
    - disable escape processing when console has disabled processed output;
    + \e[5m (blink) is the same as \e[4m (underline);
    - do not conceal control characters (0 to 31).

    1.00 - 23 October, 2005:
    + initial release.


Acknowledgments
===============

    Jean-Louis Morel, for his Perl package Win32::Console::ANSI.  It provided
    the basis of 'ANSI.dll'.

    Sergey Oblomov (hoopoepg), for Console Manager.  It provided the basis of
    'ansicon.exe'.

    Anton Bassov's article "Process-wide API spying - an ultimate hack" in "The
    Code Project".

    Richard Quadling - his persistence in finding bugs has made ANSICON what it
    is today.

    Dmitry Menshikov, Marko Bozikovic and Philippe Villiers, for their assis-
    tance in making the 64-bit version a reality.

    Luis Lavena and the Ruby people for additional improvements.

    Leigh Hebblethwaite for documentation tweaks.

    Vincent Fatica for pointing out \e[K was not right.
    Nat Kuhn for pointing out the problem with report cursor position.
    Michel Kempeneers for discovering the buffer wrap issue.
    Jean-Luc Gautier for pointing out the problem with redirecting -e to NUL.

    Thiadmer Riemersma for the nearest color algorithm.


Contact
=======

    mailto:jadoxa@yahoo.com.au
    http://ansicon.adoxa.vze.com/
    https://github.com/adoxa/ansicon


Distribution
============

    The original zipfile can be freely distributed, by any means.  However, I
    would like to be informed if it is placed on a CD-ROM (other than an arch-
    ive compilation; permission is granted, I'd just like to know).  Modified
    versions may be distributed, provided it is indicated as such in the ver-
    sion text and a source diff is made available.  In particular, the supplied
    binaries are freely redistributable.  A formal license (zlib) is available
    in LICENSE.txt.


===========================
Jason Hood, 29 April, 2019.
