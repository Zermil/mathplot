#ifndef RENDER_CONTEXT_H
#define RENDER_CONTEXT_H

#ifndef R_BACKEND_OPENGL
# define R_BACKEND_OPENGL 0
#endif

#ifndef R_BACKEND_D3D11
# define R_BACKEND_D3D11 0
#endif

#define R_BACKEND_MIX (AS_BOOL(R_BACKEND_OPENGL) + AS_BOOL(R_BACKEND_D3D11))

#if R_BACKEND_MIX == 0
# error no backend selected, please #define R_BACKEND_XXX 1
#endif

#if R_BACKEND_MIX > 1
# error more than one render backend is selected
# if R_BACKEND_D3D11
#  error d3d11 backend selected
# endif
# if R_BACKEND_OPENGL
#  error d3d11 backend selected
# endif
#endif

#endif // RENDER_CONTEXT_H
