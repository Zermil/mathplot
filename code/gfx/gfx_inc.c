#ifndef GFX_INC_C
#define GFX_INC_C

#ifdef _WIN32
# include "./gfx/win32/win32_gfx_impl.c"
#else
# error no backend for GFX on this operating system
#endif

#endif // GFX_INC_C
