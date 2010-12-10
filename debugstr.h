// ========== Auxiliary debug function

#define MYDEBUG 0     // no debugging
//#define MYDEBUG 1     // use OutputDebugString
//#define MYDEBUG 2     // use %temp%\ansicon.log

#if (MYDEBUG > 0)
#   if (MYDEBUG > 1)
    char tempfile[MAX_PATH];
#   endif
    void DEBUGSTR( LPTSTR szFormat, ... );
#else
#   if defined(_MSC_VER) && _MSC_VER <= 1400
      void DEBUGSTR() { }
#   else
#     define DEBUGSTR(...)
#   endif
#endif
