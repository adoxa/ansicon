/*
  version.h - Version defines.
*/

#define PVERS	L"1.81-wip"     // wide string
#define PVERSA	 "1.81-wip"     // ANSI string (windres 2.16.91 didn't like L)
#define PVERE	L"181"          // wide environment string
#define PVEREA	 "181"          // ANSI environment string
#define PVERB	1,8,1,0 	// binary (resource)

#ifdef _WIN64
# define BITS L"64"
# define BITSA "64"
#else
# define BITS L"32"
# define BITSA "32"
#endif

#define ANSIDLL L"ANSI" BITS L".dll"
