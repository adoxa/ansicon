# ANSICON [![Latest release](http://img.shields.io/github/release/adoxa/ansicon.svg)](https://github.com/adoxa/ansicon/releases)

ANSICON provides ANSI escape sequences for Windows console programs.  It
provides much the same functionality as `ANSI.SYS` does for MS-DOS.

## Requirements

* 32-bit: Windows 2000 Professional or later (it won't work with NT or 9X).
* 64-bit: Vista or later (it won't work with XP64).

## Installation

Add `x86` (if your OS is 32-bit) or `x64` (if 64-bit) to your `PATH`, or copy
the relevant files to a directory already in the `PATH`.

Alternatively, use option `-i` (or `-I`, if permitted) to install it
permanently, by adding an entry to `CMD.EXE`'s AutoRun registry value (current
user or local machine, respectively).

### Upgrading

- Delete `ANSI.dll`, it has been replaced with `ANSI32.dll`.
- Delete `ANSI-LLA.exe` and `ANSI-LLW.exe`, they are no longer used.
- Uninstall a pre-1.50 version and reinstall with this version.

### Uninstalling

Uninstall simply involves closing any programs that are currently using it;
running with `-u` (and/or `-U`) to remove it from AutoRun, removing the
directory from PATH, and deleting the files.  No other changes are made
(although you may have created custom environment variables).


## Usage

Running ANSICON with no arguments will start a new instance of the command
processor (the program defined by the `ComSpec` environment variable, typically
`CMD.EXE`), or display standard input if it is redirected.  Any argument will be
treated as a program and its arguments.

Options (case-sensitive):

    -l  Log to `%TEMP%\ansicon.log`.

    -p  Enable the parent process (i.e. the command shell used to run
        ANSICON) to recognise escapes.

    -m  Set the current (and default) attribute to grey on black
        (`monochrome`), or the attribute following the `m` (please
        use `COLOR /?` for attribute values).

    -e  Echo the command line - a space or tab after the `e` is
        ignored, the remainder is displayed verbatim.

    -E  Like `e`, but no newline is added.

    -t  Display ("type") each file (or standard input if none or the
        name is `-`) as though they are a single file.

    -T  Display `==> FILE NAME <==`, a blank line (or an error
        message), the file and another blank line.

For example, to display `file.ans` using black on cyan as the default color:

    ansicon -m30 -t file.ans

The attribute may start with `-` to permanently reverse the foreground and
background colors (but not when using `-p`).

For example, to use reversed black on white as the default (i.e. white on black,
with foreground sequences changing the background):

    ansicon -m-f0 -t file.log

If you experience trouble with certain programs, the log may help in finding the
cause; it can be found at `%TEMP%\ansicon.log`.  A number should follow the `l`:

    0   No logging
    1   Log process start and end
    2   Above, plus log modules used by the process
    3   Above, plus log functions that are hooked
    4   Log console output (add to any of the above)
    8   Append to the existing file (add to any of the above)
    16  Log all imported modules (add to any of the above)

The log option will not work with `-p`; set the environment variable
`ANSICON_LOG` instead.  The variable is only read once when a new process is
started; changing it won't affect running processes.  If you identify a module
that causes problems, add it to the `ANSICON_EXC` environment variable (see
`ANSICON_API` below, but the extension is required).

E.g.: `ansicon -l5` will start a new command processor, logging every process it
starts along with their output.

Once installed, the ANSICON environment variable will be created.  This variable
is of the form `WxH (wxh)`, where `W` & `H` are the width and height of the
buffer and `w` & `h` are the width and height of the window.  The variable is
updated whenever a program reads it directly (i.e. as an individual request, not
as part of the entire environment block).  For example, `set an` will not update
it, but `echo %ansicon%` will.

Also created is `ANSICON_VER`, which contains the version without the point
(`1.50` becomes `150`).  This variable does not exist as part of the environment
block (`set an` will not show it).

If installed, GUI programs will not be hooked.  Either start the program
directly with `ansicon`, or add it to the `ANSICON_GUI` variable (see
`ANSICON_API` below).

Using `ansicon` after install will always start with the default attributes,
restoring the originals on exit; all other programs will use the current
attributes.  The shift state is always reset for a new process.

The Windows API `WriteFile` and `WriteConsoleA` functions will set the number of
characters written, not the number of bytes.  When using a multibyte character
set, this results in a smaller number (since multiple bytes are used to
represent a single character).  Some programs recognise this as a reduced write
and will inadvertently repeat previous characters.  If you discover such a
program, use the `ANSICON_API` environment variable to record it and override
the API, returning the original byte count.  Ruby (prior to 1.9.3) is an example
of such a program, so use `set ANSICON_API=ruby` to avoid the repitition.  The
full syntax is:

    ANSICON_API=[!]program;program;program...

PROGRAM is the name of the program, with no path and extension.  The leading
exclamation inverts the usage, meaning the API will always be overridden, unless
the program is in the list.  The variable can be made permanent by going to
_System Properties_, selecting the _Advanced_ tab (with Vista onwards, this can
be done by running _"SystemPropertiesAdvanced"_) and clicking _Environment
Variables_.


## Limitations

- The entire console buffer is used, not just the visible window.
- There's a conflict with NVIDIA's drivers, requiring the setting of the
  Environment Variable:

        ANSICON_EXC=nvd3d9wrap.dll;nvd3d9wrapx.dll


## Sequences

### Recognized Sequences

The following escape sequences are recognised.

    \e]0;titleBEL   Set (xterm) window's title (and icon)
    \e[21t          Report (xterm) window's title
    \e[s            Save Cursor
    \e[u            Restore Cursor
    \e[#G           CHA Cursor Character Absolute
    \e[#E           CNL Cursor Next Line
    \e[#F           CPL Cursor Preceding Line
    \e[#D           CUB Cursor Left
    \e[#B           CUD Cursor Down
    \e[#C           CUF Cursor Right
    \e[#;#H         CUP Cursor Position
    \e[#A           CUU Cursor Up
    \e[#P           DCH Delete Character
    \e[?25h         DECTCEM DEC Text Cursor Enable Mode (show cursor)
    \e[?25l         DECTCEM DEC Text Cursor Enable Mode (hide cursor)
    \e[#M           DL  Delete Line
    \e[#n           DSR Device Status Report
    \e[#X           ECH Erase Character
    \e[#J           ED  Erase In Page
    \e[#K           EL  Erase In Line
    \e[#`           HPA Character Position Absolute
    \e[#j           HPB Character Position Backward
    \e[#a           HPR Character Position Forward
    \e[#;#f         HVP Character And Line Position
    \e[#@           ICH Insert Character
    \e[#L           IL  Insert Line
    SI              LS0 Locking-shift Zero (see below)
    SO              LS1 Locking-shift One
    \e[#;#;#m       SGR Select Graphic Rendition
    \e[#d           VPA Line Position Absolute
    \e[#k           VPB Line Position Backward
    \e[#e           VPR Line Position Forward

- `\e` represents the escape character (ASCII 27).
- `#` represents a decimal number (optional, in most cases defaulting to 1).
- BEL, SO, and SI are ASCII 7, 14 and 15.
- Regarding SGR: bold will set the foreground intensity; blink and underline
  will set the background intensity; conceal uses background as foreground.
  See `sequences.txt` for a more complete description.

I make a distinction between `\e[m` and `\e[0;...m`.  Both will restore the
original foreground/background colors (so `0` should be the first parameter);
the former will also restore the original bold and underline attributes, whilst
the latter will explicitly reset them.  The environment variable `ANSICON_DEF`
can be used to change the default colors (same value as `-m`; setting the
variable does not change the current colors).


### Ignored Sequences

The following escape sequences are explicitly ignored.

    \e(?        Designate G0 character set ('?' is any character).
    \e)?        Designate G1 character set ('?' is any character).
    \e[?...     Private sequence
    \e[>...     Private sequence

The G0 character set is always ASCII; the G1 character set is always the
DEC Special Graphics Character Set.


### DEC Special Graphics Character Set

This is my interpretation of the set, as shown by
http://vt100.net/docs/vt220-rm/table2-4.html.

    Char    Unicode Code Point & Name
    ----    -------------------------
    _       U+0020  Space (blank)
    `       U+2666  Black Diamond Suit
    a       U+2592  Medium Shade
    b       U+2409  Symbol For Horizontal Tabulation
    c       U+240C  Symbol For Form Feed
    d       U+240D  Symbol For Carriage Return
    e       U+240A  Symbol For Line Feed
    f       U+00B0  Degree Sign
    g       U+00B1  Plus-Minus Sign
    h       U+2424  Symbol For Newline
    i       U+240B  Symbol For Vertical Tabulation
    j       U+2518  Box Drawings Light Up And Left
    k       U+2510  Box Drawings Light Down And Left
    l       U+250C  Box Drawings Light Down And Right
    m       U+2514  Box Drawings Light Up And Right
    n       U+253C  Box Drawings Light Vertical And Horizontal
    o       U+00AF  Macron (SCAN 1)
    p       U+25AC  Black Rectangle (SCAN 3)
    q       U+2500  Box Drawings Light Horizontal (SCAN 5)
    r       U+005F  Low Line (SCAN 7)
    s       U+005F  Low Line (SCAN 9)
    t       U+251C  Box Drawings Light Vertical And Right
    u       U+2524  Box Drawings Light Vertical And Left
    v       U+2534  Box Drawings Light Up And Horizontal
    w       U+252C  Box Drawings Light Down And Horizontal
    x       U+2502  Box Drawings Light Vertical
    y       U+2264  Less-Than Or Equal To
    z       U+2265  Greater-Than Or Equal To
    {       U+03C0  Greek Small Letter Pi
    |       U+2260  Not Equal To
    }       U+00A3  Pound Sign
    ~       U+00B7  Middle Dot

`G1.txt` is a Unicode file to view the glyphs "externally".  `G1.bat` is a batch
file (using `x86\ansicon`) to show the glyphs in the console.  The characters
will appear as they should using Lucida (other than the Symbols), but code page
will influence them when using a raster font (but of particular interest, 437
and 850 both show the Box Drawings).


## Acknowledgments

- Jean-Louis Morel, for his Perl package `Win32::Console::ANSI`.  It provided
  the basis of `ANSI.dll`.

- Sergey Oblomov (hoopoepg), for Console Manager.  It provided the basis of
  `ansicon.exe`.

- Anton Bassov's article _"Process-wide API spying - an ultimate hack"_ in _"The
  Code Project"_.

- Richard Quadling - his persistence in finding bugs has made ANSICON what it is
  today.

- Dmitry Menshikov, Marko Bozikovic and Philippe Villiers, for their assistance
  in making the 64-bit version a reality.

- Luis Lavena and the Ruby people for additional improvements.

- Leigh Hebblethwaite for documentation tweaks.

- Vincent Fatica for pointing out `\e[K` was not right.


## Contact

mailto:jadoxa@yahoo.com.au  
http://ansicon.adoxa.vze.com/  
https://github.com/adoxa/ansicon  


## Distribution

The original zipfile can be freely distributed, by any means.  However, I would
like to be informed if it is placed on a CD-ROM (other than an archive
compilation; permission is granted, I'd just like to know).

Modified versions may be distributed, provided it is indicated as such in the
version text and a source diff is made available.

In particular, the supplied binaries are freely redistributable.

A formal license (zlib) is available in `LICENSE.txt`.

---
Copyright 2005-2014 Jason Hood
