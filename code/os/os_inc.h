#ifndef OS_INC_H
#define OS_INC_H

#include "./os/os.h"

#ifdef _WIN32
# include "./win32/win32_inc.h"
# include "./os/win32/win32_os_impl.h"
#else
# error OS not supported currently os_inc.h
#endif

#endif // OS_INC_H
