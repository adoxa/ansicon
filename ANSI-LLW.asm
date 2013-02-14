; ANSI-LLW.asm - Output the 32-bit address of LoadLibraryW.
;
; Jason Hood, 1 February, 2013.
;
; A FASM (flatassembler.net) version of the C code, which virus scanners didn't
; like for some reason.


format PE Console 4.0

include 'win32a.inc'

	mov	eax, [LoadLibraryW]
	ret

data import

 library kernel32,'KERNEL32.DLL'

 import kernel32,\
	LoadLibraryW,'LoadLibraryW'

end data
