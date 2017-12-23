# ANSICON [![Latest release](http://img.shields.io/github/release/adoxa/ansicon.svg)](https://github.com/adoxa/ansicon/releases)

ANSICON provides ANSI escape sequences for Windows console programs. It
provides much the same functionality as `ANSI.SYS` does for MS-DOS.

## Requirements

* 32-bit: Windows 2000 Professional and later (it won't work with NT or 9X).
* 64-bit: AMD64 (it won't work with IA64).

## How it Works

ANSICON *injects* a DLL into a process, *hooking* its functions.

### Injection

One of three methods is used to inject the DLL.

* `LoadLibrary` via `CreateRemoteThread` for a running process.

* `LdrLoadDll` via `CreateRemoteThread` for a 64-bit .NET AnyCPU process.

* Adding the DLL directly to the import table, otherwise.

### Hooking

Hooking is achieved by modifying import addresses, or the return value of
`GetProcAddress`.
