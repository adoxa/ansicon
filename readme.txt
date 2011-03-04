
				    ANSICON

			 Copyright 2005-2011 Jason Hood

			    Version 1.40.  Freeware


    ===========
    Description
    ===========

    ANSICON provides ANSI escape sequences for Windows console programs.  It
    provides much the same functionality as `ANSI.SYS' does for MS-DOS.


    ============
    Requirements
    ============

    Windows 2000 Professional and later (it won't work with NT or 9X).


    ============
    Installation
    ============

    Add x86 (if your OS is 32-bit) or x64 (if 64-bit) to your PATH, or copy
    the relevant files to a directory already on the PATH.  Alternatively,
    use option `-i' (or `-I') to install it permanently, by adding an entry
    to CMD.EXE's AutoRun registry value (current user or local machine,
    respectively).  Uninstall simply involves closing any programs that are
    currently using it, running with `-u' (and again with `-U') to remove
    the AutoRun entry/ies, then removing the directory from PATH or deleting
    the files.	No other changes are made.

    ---------
    Upgrading
    ---------

    Delete ANSI.dll, it has been replaced with ANSI32.dll.
    Delete ANSI-LLA.dll, it has been replaced with ANSI-LLW.dll.


    =====
    Usage
    =====

    Options (case sensitive):

	-p	Enable the parent process (i.e. the command shell used to
		run ANSICON) to recognise escapes.

	-m	Set the current (and default) attribute to grey on black
		("monochrome"), or the attribute following the `m' (please
		use `COLOR /?' for attribute values).

	-e	Echo the command line - the character after the `e' is
		ignored, the remainder is displayed verbatim.

	-E	As above, but no newline is added.

	-t	Display ("type") each file (or standard input if none or the
		name is "-") as though they are a single file.

	-T	Display "==> FILE NAME <==", a blank line (or an error
		message), the file and another blank line.

    Running ANSICON with no arguments will start a new instance of the com-
    mand processor (the program defined by the `ComSpec' environment var-
    iable, typically `CMD.EXE'), or display standard input if it is redir-
    ected.  Any argument will be treated as a program and its arguments.
    
    Eg: `ansicon -m30 -t file.ans' will display `file.ans' using black on
    cyan as the default color.

    Once installed, the ANSICON environment variable will be created.  This
    variable is of the form "WxH (wxh)", where W & H are the width and
    height of the buffer and w & h are the width and height of the window.
    The variable is updated whenever a program reads it directly (i.e. as
    an individual request, not as part of the entire environment block).
    For example, "set an" will not update it, but "echo %ansicon%" will.

    The Windows API WriteFile and WriteConsoleA functions will set the num-
    ber of characters written, not the number of bytes.  When using a multi-
    byte character set, this results in a smaller number (since multiple
    bytes are used to represent a single character).  Some programs recog-
    nise this as a reduced write and will inadvertently repeat previous
    characters.  If you discover such a program, use the ANSICON_API envir-
    onment variable to record it and override the API, returning the origin-
    al byte count.  Ruby is an example of such a program (at least, up till
    1.9.2p0), so use "set ANSICON_API=ruby" to avoid the repitition.  The
    full syntax of the variable is:

	ANSICON_API=[!]program;program;program...

    PROGRAM is the name of the program, with no path and extension.  The
    leading exclamation inverts the usage, meaning the API will always be
    overridden, unless the program is in the list.  The variable can be made
    permanent by going to System Properties, selecting the Advanced tab and
    clicking Environment Variables (using XP; Vista/7 may be different).


    ====================
    Sequences Recognised
    ====================

    The following escape sequences are recognised.

	\e[#A		CUU	CUrsor Up
	\e[#B		CUD	CUrsor Down
	\e[#C		CUF	CUrsor Forward
	\e[#D		CUB	CUrsor Backward
	\e[#E		CNL	Cursor Next Line
	\e[#F		CPL	Cursor Preceding Line
	\e[#G		CHA	Cursor Horizontal Absolute
	\e[#;#H 	CUP	CUrsor Position
	\e[#;#f 	HVP	Horizontal and Vertical Position
	\e[s		SCP	Save Cursor Position
	\e[u		RCP	Restore Cursor Position
	\e[#J		ED	Erase Display
	\e[#K		EL	Erase Line
	\e[#L		IL	Insert Lines
	\e[#M		DL	Delete Lines
	\e[#@		ICH	Insert CHaracter
	\e[#P		DCH	Delete CHaracter
	\e[#;#;#m	SGM	Set Graphics Mode
	\e[#n		DSR	Device Status Report
	\e[21t			Report (xterm) window's title
	\e]0;titleBEL		Set (xterm) window's title (and icon)
	\e[?25h 	DECTCEM DEC Text Cursor Enable Mode (show cursor)
	\e[?25l 	DECTCEM DEC Text Cursor Enable Mode (hide cursor)
	SO		LS1	Lock shift G1 (see below)
	SI		LS0	Lock shift G0

    `\e' represents the escape character (ASCII 27); `#' represents a
    decimal number (optional, in most cases defaulting to 1); BEL, SO and
    SI are ASCII 7, 14 and 15.	Regarding SGM: bold will set the foreground
    intensity; underline and blink will set the background intensity;
    conceal uses background as foreground.

    I make a distinction between "\e[m" and "\e[0;...m".  Both will restore
    the original foreground/background colors (and so "0" should be the
    first parameter); the former will also restore the original bold and
    underline attributes, whilst the latter will explicitly reset them.


    =================
    Sequences Ignored
    =================

    The following escape sequences are explicitly ignored.

	\e(?		Designate G0 character set (`?' is anything).
	\e)?		Designate G1 character set (`?' is anything).
	\e[?... 	Private sequence
	\e[>... 	Private sequence

    The G0 character set is always ASCII; the G1 character set is always
    the DEC Special Graphics Character Set.


    ==================================
    DEC Special Graphics Character Set
    ==================================

    This is my interpretation of the set, as shown by
    http://vt100.net/docs/vt220-rm/table2-4.html.


	Char	Unicode Code Point & Name
	----	-------------------------
	_	U+0020	Space (blank)
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
	o	U+00AF	Macron (SCAN 1)
	p	U+25AC	Black Rectangle (SCAN 3)
	q	U+2500	Box Drawings Light Horizontal (SCAN 5)
	r	U+005F	Low Line (SCAN 7)
	s	U+005F	Low Line (SCAN 9)
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
    batch file (using `x86\ansicon') to show the glyphs in the console.  The
    characters will appear as they should using Lucida (other than the Sym-
    bols), but code page will influence them when using a raster font (but
    of particular interest, 437 and 850 both show the Box Drawings).


    ===========
    Limitations
    ===========

    The entire console buffer is used, not just the visible window.

    If running CMD.EXE, its own COLOR will be the initial color.

    The 64-bit version can inject into a 32-bit process, but the 32-bit
    version will not inject into a 64-bit process.

    Building rubyinstaller on Win7 crashes (XP is fine).


    ===============
    Version History
    ===============

    Legend: + added, - bug-fixed, * changed.

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


    ===============
    Acknowledgments
    ===============

    Jean-Louis Morel, for his Perl package Win32::Console::ANSI.  It
    provided the basis of `ANSI.dll'.

    Sergey Oblomov (hoopoepg), for Console Manager.  It provided the basis
    of `ansicon.exe'.

    Anton Bassov's article "Process-wide API spying - an ultimate hack" in
    "The Code Project".

    Richard Quadling - his persistence in finding bugs has made ANSICON
    what it is today.

    Dmitry Menshikov, Marko Bozikovic and Philippe Villiers, for their
    assistance in making the 64-bit version a reality.

    Luis Lavena and the Ruby people for additional improvements.

    Leigh Hebblethwaite for documentation tweaks.


    =======
    Contact
    =======

    mailto:jadoxa@yahoo.com.au
    http://ansicon.adoxa.cjb.net/
    https://github.com/adoxa/ansicon

    Jason Hood
    11 Buckle Street
    North Rockhampton
    Qld 4701
    Australia


    ============
    Distribution
    ============

    The original zipfile can be freely distributed, by any means.  However,
    I would like to be informed if it is placed on a CD-ROM (other than an
    archive compilation; permission is granted, I'd just like to know).
    Modified versions may be distributed, provided it is indicated as such
    in the version text and a source diff is included.


    ==========================
    Jason Hood, 3 March, 2011.
