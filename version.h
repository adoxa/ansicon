/*
  version.h - Version defines.
*/

#define PVERS	L"1.64"         // wide string
#define PVERSA	 "1.64"         // ANSI string (windres 2.16.91 didn't like L)
#define PVERE	L"164"          // wide environment string
#define PVEREA	 "164"          // ANSI environment string
#define PVERB	1,6,4,0 	// binary (resource)

#ifdef _WIN64
# define BITS L"64"
# define BITSA "64"
#else
# define BITS L"32"
# define BITSA "32"
#endif

#define ANSIDLL L"ANSI" BITS L".dll"
