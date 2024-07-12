#ifndef FONT_INC_C
#define FONT_INC_C

#include "./font/font_helper.c"

#ifdef FONT_USE_FREETYPE
# include "./font/freetype/freetype_font_impl.c"
#else
# error font provider not specified font_inc.c
#endif

#endif // FONT_INC_C
