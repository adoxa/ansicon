# Makefile for ANSICON.
# Jason Hood, 11 March, 2006.  Updated 20 June, 2009.

# I've used TDM64 (gcc 4.6.1), building the 32-bit version in the x86 directory
# and the 64-bit version in the x64 directory.	MinGW32 (gcc 3.4.5) will also
# build the 32-bit version.

# 19 November, 2010:
#   explicitly use 64-bit flags, in case the compiler isn't.
#
# 13 December, 2011:
#   use CMD for file operations, not programs from fileutils.
#
# 23 November, 2012:
#   set the base address of the DLLs to AC0000[00] (AnsiCon).

CC = gcc
CFLAGS = -O2 -Wall

X86OBJS = x86/proctype.o x86/injdll32.o x86/util.o
X64OBJS = x64/proctype.o x64/injdll64.o x64/injdll32.o x64/util.o

x86/%.o: %.c ansicon.h
	$(CC) -m32 -c $(CFLAGS) $< -o $@

x86/%v.o: %.rc version.h
	windres -U _WIN64 -F pe-i386 $< $@

x64/%.o: %.c ansicon.h
	$(CC) -m64 -c $(CFLAGS) $< -o $@

x64/%v.o: %.rc version.h
	windres -F pe-x86-64 $< $@

all: ansicon32 ansicon64

ansicon32: x86 x86/ansicon.exe x86/ANSI32.dll

ansicon64: x64 x64/ansicon.exe x64/ANSI64.dll x64/ANSI32.dll

x86:
	cmd /c "mkdir x86"

x86/ansicon.exe: x86/ansicon.o $(X86OBJS) x86/ansiconv.o
	$(CC) -m32 $+ -s -o $@

x86/ANSI32.dll: x86/ANSI.o $(X86OBJS) x86/ansiv.o
	$(CC) -m32 $+ -s -o $@ -mdll -Wl,-shared,--image-base,0xAC0000

x64:
	cmd /c "mkdir x64"

x64/ansicon.exe: x64/ansicon.o $(X64OBJS) x64/ansiconv.o
	$(CC) -m64 $+ -s -o $@

x64/ANSI64.dll: x64/ANSI.o $(X64OBJS) x64/ansiv.o
	$(CC) -m64 $+ -s -o $@ -mdll -Wl,-shared,--image-base,0xAC000000

x64/ANSI32.dll: x64/ANSI32.o x64/proctype32.o x86/injdll32.o x86/util.o x86/ansiv.o
	$(CC) -m32 $+ -s -o $@ -mdll -Wl,-shared,--image-base,0xAC0000

x86/ansicon.o:	version.h
x86/ANSI.o:	version.h
x64/ansicon.o:	version.h
x64/ANSI.o:	version.h
x86/util.o:	version.h
x64/util.o:	version.h
x86/ansiconv.o: ansicon.rc
x86/ansiv.o:	ansi.rc
x64/ansiconv.o: ansicon.rc
x64/ansiv.o:	ansi.rc

x64/ANSI32.o: ANSI.c
	$(CC) -m32 -DW32ON64 $(CFLAGS) $< -c -o $@
x64/proctype32.o: proctype.c
	$(CC) -m32 -DW32ON64 $(CFLAGS) $< -c -o $@

# Need two commands, because if the directory doesn't exist, it won't delete
# anything at all.
clean:
	-cmd /c "del x86\*.o 2>nul"
	-cmd /c "del x64\*.o 2>nul"
