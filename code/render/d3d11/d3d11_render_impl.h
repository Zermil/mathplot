#ifndef D3D11_RENDER_IMPL_H
#define D3D11_RENDER_IMPL_H

#include <d3d11.h>
#include <dxgi.h>
#include <d3dcompiler.h>

#ifndef R_D3D11_BUFFER_INIT_CAP
# define R_D3D11_BUFFER_INIT_CAP MB(16)
#endif

typedef enum {
    V_BUFFER = 0,
    I_BUFFER,
    C_BUFFER,
    BUFFER_COUNT,
} D3D11_Buffer_Type;

typedef struct {
    HMM_Vec2 res;
} D3D11_Cbuffer;

typedef struct D3D11_Texture {
    struct D3D11_Texture *next;
    
    ID3D11Texture2D *data;
    ID3D11ShaderResourceView *view;
} D3D11_Texture;

typedef struct {
    ID3D11Device *device;
    ID3D11DeviceContext *context;
    
    ID3D11Buffer *buffer[BUFFER_COUNT];
    
    ID3D11InputLayout *layout;
    ID3D11VertexShader *vertex_shader;
    ID3D11PixelShader *pixel_shader;
    
    ID3D11RasterizerState *rasterizer;
    ID3D11BlendState *blend_state;
    ID3D11SamplerState *sampler_state;
    
    D3D11_Cbuffer cbuffer;
    
    Arena *arena;
    D3D11_Texture *first_free_texture;
    D3D11_Texture dummy_texture;
} D3D11_State;

typedef struct {
    IDXGISwapChain *swap_chain;
    ID3D11RenderTargetView *target;
    D3D11_VIEWPORT viewport;
    D3D11_RECT scissors;
} D3D11_Window;

#endif // D3D11_RENDER_IMPL_H
