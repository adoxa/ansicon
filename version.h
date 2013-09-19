/*
  version.h - Version defines.
*/

#define PVERS	L"1.66"         // wide string
#define PVERSA	 "1.66"         // ANSI string (windres 2.16.91 didn't like L)
#define PVERE	L"166"          // wide environment string
#define PVEREA	 "166"          // ANSI environment string
#define PVERB	1,6,6,0 	// binary (resource)

#ifdef _WIN64
# define BITS L"64"
# define BITSA "64"
#else
# define BITS L"32"
# define BITSA "32"
#endif

#define ANSIDLL L"ANSI" BITS L".dll"
