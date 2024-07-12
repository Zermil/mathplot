#ifndef RENDER_INC_H
#define RENDER_INC_H

#include "./render/render_context.h"
#include "./render/render.h"

#include "./render/render_helper.h"

#if R_BACKEND_D3D11
# if _WIN32
#  include "./render/d3d11/d3d11_render_impl.h"
# else
#  error d3d11 backend is only available on windows for now render_inc.h
# endif
#elif R_BACKEND_OPENGL
# error opengl backend not implemented render_inc.h
#else
# error no backend provided for rendering in render_inc.h
#endif

#endif // RENDER_INC_H
