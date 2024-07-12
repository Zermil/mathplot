#ifndef RENDER_INC_C
#define RENDER_INC_C

#include "./render/render_helper.c"

#if R_BACKEND_D3D11
# if _WIN32
#  include "./render/d3d11/d3d11_render_impl.cpp"
# else
#  error d3d11 backend is only available on windows for now render_inc.c
# endif
#elif R_BACKEND_OPENGL
# error opengl backend not implemented render_inc.c
#else
# error no backend provided for rendering in render_inc.c
#endif

#endif // RENDER_INC_C
