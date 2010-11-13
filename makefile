# Simple makefile for ANSICON.
# Jason Hood, 11 March, 2006.  Updated 20 June, 2009.

# I've used TDM64 (gcc 4.5.0), building the 32-bit version in the x86 directory
# and the 64-bit version in the x64 directory.	MinGW32 (gcc 3.4.5) will also
# build the 32-bit version, but will of course fail on the 64-bit.

CC = gcc
CFLAGS = -O2 -Wall

x86/%.o: %.c
	$(CC) -m32 -c $(CFLAGS) $(CPPFLAGS) $< -o $@

x86/%v.o: %.rc
	windres -U _WIN64 -F pe-i386 $< $@

x64/%.o: %.c
	$(CC) -c $(CFLAGS) $(CPPFLAGS) $< -o $@

x64/%v.o: %.rc
	windres $< $@

all: ansicon32 ansicon64

ansicon32: x86 x86/ansicon.exe x86/ANSI32.dll

ansicon64: x64 x64/ansicon.exe x64/ANSI64.dll x64/ANSI32.dll x64/ANSI-LLW.exe

x86:
	mkdir x86

x86/ansicon.exe: x86/ansicon.o x86/injdll32.o x86/ansiconv.o
	$(CC) -m32 $+ -s -o $@

x86/ANSI32.dll: x86/ANSI.o x86/injdll32.o x86/ansiv.o
	$(CC) -m32 $+ -s -o $@ -mdll -Wl,-shared

x64:
	mkdir x64

x64/ansicon.exe: x64/ansicon.o x64/injdll64.o x64/ansiconv.o
	$(CC) $+ -s -o $@

x64/ANSI64.dll: x64/ANSI.o x64/injdll64.o x64/injdll32.o x64/ansiv.o
	$(CC) $+ -s -o $@ -mdll -Wl,-shared

x64/ANSI32.dll: x86/ANSI32.dll
	cp -p x86/ANSI32.dll x64/ANSI32.dll

x64/ANSI-LLW.exe: ANSI-LLW.c
	$(CC) -m32 $(CFLAGS) $< -s -o $@

x86/ansiconv.o: ansicon.rc
x86/ansiv.o:	ansi.rc
x64/ansiconv.o: ansicon.rc
x64/ansiv.o:	ansi.rc

clean:
	-rm x86/*.o
	-rm x64/*.o
