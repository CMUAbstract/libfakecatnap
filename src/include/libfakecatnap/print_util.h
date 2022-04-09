#ifndef _PRINT_UTIL_H_
#define _PRINT_UTIL_H_



#ifdef LFCN_DBG
  #define LFCN_DBG_PRINTF PRINTF
#else
  #define LFCN_DBG_PRINTF(...)
#endif


#endif
