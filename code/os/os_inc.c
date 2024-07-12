#ifndef OS_INC_C
#define OS_INC_C

#ifdef _WIN32
# include "./os/win32/win32_os_impl.c"
#else
# error OS not supported currently os_inc.h
#endif

#endif // OS_INC_C
