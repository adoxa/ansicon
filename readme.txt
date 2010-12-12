
				    ANSICON

			 Copyright 2005-2010 Jason Hood

			    Version 1.32.  Freeware


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

    Running ANSICON with no arguments will start a new instance of the com-
    mand processor (the program defined by the `ComSpec' environment var-
    iable, typically `CMD.EXE'), or display standard input if it is redir-
    ected.  Passing the option `-p' (case sensitive) will enable the parent
    process to recognise escapes (i.e. the command shell used to run ANSI-
    CON).  Use `-m' to set the current (and default) attribute to grey on
    black ("monochrome"), or the attribute following the `m' (please use
    `COLOR /?' for attribute values).  The option `-e' will echo the command
    line - the character after the `e' is ignored, the remainder is display-
    ed verbatim; use `-E' to prevent a newline being written.  The option
    `-t' will display each file (or standard input if none or it is "-"), as
    though they are a single file; `-T' will display the file name (in be-
    tween "==> " and " <=="), a blank line (or an error message), the file
    and another blank line.  Anything else will be treated as a program and
    its arguments.  Eg: `ansicon -m30 -t file.ans' will display `file.ans'
    using black on cyan as the default color.

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


    =========
    Sequences
    =========

    The following escape sequences are recognised.

	\e[#A	    CUU: CUrsor Up
	\e[#B	    CUD: CUrsor Down
	\e[#C	    CUF: CUrsor Forward
	\e[#D	    CUB: CUrsor Backward
	\e[#E	    CNL: Cursor Next Line
	\e[#F	    CPL: Cursor Preceding Line
	\e[#G	    CHA: Cursor Horizontal Absolute
	\e[#;#H     CUP: CUrsor Position
	\e[#;#f     HVP: Horizontal and Vertical Position
	\e[s	    SCP: Save Cursor Position
	\e[u	    RCP: Restore Cursor Position
	\e[#J	    ED:  Erase Display
	\e[#K	    EL:  Erase Line
	\e[#L	    IL:  Insert Lines
	\e[#M	    DL:  Delete Lines
	\e[#@	    ICH: Insert CHaracter
	\e[#P	    DCH: Delete CHaracter
	\e[#;#;#m   SGM: Set Graphics Mode
	\e[#n	    DSR: Device Status Report
	\e[21t		 Report (xterm) window's title
	\e]0;titleBEL	 Set (xterm) window's title (and icon)

    `\e' represents the escape character (ASCII 27); `#' represents a
    decimal number (optional, in most cases defaulting to 1).  Regarding
    SGM: bold will set the foreground intensity; underline and blink will
    set the background intensity; conceal uses background as foreground.

    I make a distinction between "\e[m" and "\e[0;...m".  Both will restore
    the original foreground/background colors (and so "0" should be the
    first parameter); the former will also restore the original bold and
    underline attributes, whilst the latter will explicitly reset them.


    ===========
    Limitations
    ===========

    The entire console buffer is used, not just the visible window.

    If running CMD.EXE, its own COLOR will be the initial color.

    The 64-bit version can inject into a 32-bit process, but the 32-bit
    version will not inject into a 64-bit process.


    ===============
    Version History
    ===============

    Legend: + added, - bug-fixed, * changed.

    1.32 - 12 December, 2010:
    - fixed crash due to NULL lpNumberOfBytesWritten/lpNumberOfCharsWritten;
    - -p will test the parent process for validity;
    * hook into GUI processes;
    + recognise DSR and xterm window title sequences.

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


    ==============================
    Jason Hood, 12 December, 2010.
