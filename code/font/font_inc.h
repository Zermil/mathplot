#ifndef FONT_INC_H
#define FONT_INC_H

#include "./font/font.h"
#include "./font/font_helper.h"

#ifdef FONT_USE_FREETYPE
# include "./font/freetype/freetype_font_impl.h"
#else
# error font provider not specified font_inc.h
#endif

#endif // FONT_INC_H
