#ifndef GFX_INC_H
#define GFX_INC_H

#include "./gfx/gfx.h"

#ifdef _WIN32
# include "./win32/win32_inc.h"
# include "./gfx/win32/win32_gfx_impl.h"
#else
# error no backend for GFX on this operating system
#endif

#endif // GFX_INC_H
