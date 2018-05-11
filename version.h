/*
  version.h - Version defines.
*/

#define PVERS	L"1.84"         // wide string
#define PVERSA	 "1.84"         // ANSI string (windres 2.16.91 didn't like L)
#define PVERE	L"184"          // wide environment string
#define PVEREA	 "184"          // ANSI environment string
#define PVERB	1,8,4,0 	// binary (resource)

#ifdef _WIN64
# define BITS L"64"
# define BITSA "64"
#else
# define BITS L"32"
# define BITSA "32"
#endif

#define ANSIDLL L"ANSI" BITS L".dll"
