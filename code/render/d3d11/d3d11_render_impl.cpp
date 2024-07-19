// @Note: We don't really check HRESULT in here because the DEBUG
// flags provide some nice error messages so there's no need to do that manually.

global D3D11_State d3d11_state = {0};
global D3D11_Window d3d11_windows[GFX_MAX_WINDOW_COUNT] = {0};
global b32 d3d11_is_init = 0;

#define d3d11_window_from_opaque(w) (d3d11_windows + (u64)(w))

// @ToDo: Clean-up all this mess in the shader.
global u8 hlsl[] =
"struct VS_INPUT {\n"
"    float2 pos   : POS;\n"
"    float4 uv    : UV;\n"
"    float4 quad  : QUAD;\n"
"    float4 col   : COL;\n"
"    float radius : RADIUS;\n"
"    float theta  : THETA;\n"
"};\n"
"\n"
"struct PS_INPUT {\n"
"    float4 pos         : SV_POSITION;\n"
"    float2 uv          : UV;\n"
"    float4 col         : COL;\n"
"    float radius       : RADIUS;\n"
"    float2 quad_center : QUAD_CENTER;\n"
"    float2 quad_half   : QUAD_HALF;\n"
"    float2 quad_pos    : QUAD_POS;\n"
"};\n"
"\n"
"cbuffer VS_CBUFFER : register(b0) {\n"
"    float2 resolution;\n"
"};\n"
"\n"
"float sdf_rect(float2 p, float2 half_dim, float radius) {\n"
"    return(length(max(abs(p) - half_dim + radius, 0.0f)) - radius);\n"
"}\n"
"\n"
"PS_INPUT main_vs(VS_INPUT input) {\n"
"    float s_theta = sin(-input.theta);\n"
"    float c_theta = cos(-input.theta);\n"
"    PS_INPUT output;\n"
"    output.col = input.col;\n"
"    output.radius = input.radius;\n"
"    output.quad_center = (input.quad.xy + input.quad.zw)/2.0f;\n"
"    // @Note: Treat rect bigger than actually, makes for a nicer smoothstep.\n"
"    output.quad_half = (input.quad.zw - input.quad.xy)/2.0f + input.radius;\n"
"    output.quad_pos = output.quad_center + input.pos*output.quad_half;\n"
"    float2 qr = output.quad_pos - output.quad_center;\n"
"    float2 pr = float2(qr.x*c_theta - qr.y*s_theta, qr.x*s_theta + qr.y*c_theta);\n"
"    float2 p = output.quad_center + pr;\n"
"    float2 norm_pos = (input.pos + 1.0f)/2.0f;\n"
"    output.uv = float2(lerp(input.uv.x, input.uv.z, norm_pos.x), lerp(input.uv.y, input.uv.w, norm_pos.y));\n"
"    float2 screen_pos = (p/resolution)*2.0f - 1.0f;\n"
"    output.pos = float4(screen_pos.x, -screen_pos.y, 0.0f, 1.0f);\n"
"    return(output);\n"
"}\n"
"\n"
"Texture2D tex            : TEXTURE : register(t0);\n"
"SamplerState tex_sampler : SAMPLER : register(s0);\n"
"float4 main_ps(PS_INPUT input) : SV_TARGET {\n"
"    float2 coord = input.quad_pos - input.quad_center;\n"
"    float sdf = sdf_rect(coord, input.quad_half - input.radius, input.radius);\n"
"    // @Note: second parameter in smoothstep() controls the actual smoothness.\n"
"    float s = 1.0f - smoothstep(0.0f, 1.75f, sdf);\n"
"    // @Note: we flip because we submit the colours in rgba format\n"
"    float4 pixel_color = float4(tex.Sample(tex_sampler, input.uv) * input.col).abgr;\n"
"    return(float4(pixel_color.rgb, s * pixel_color.a));\n"
"}\n";

internal b32 r_is_init(void)
{
    return(d3d11_is_init);
}

internal b32 r_backend_init(void)
{
    b32 error = 0;
    if (d3d11_is_init) {
        er_push(str8("D3D11 is already initialized"));
        error = 1;
    }
    
    if (!error) {
        d3d11_state.arena = arena_make();
    }
    
    if (!error) {
        UINT flags = 0;
#ifndef NDEBUG
        flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
        D3D_FEATURE_LEVEL features[] = { D3D_FEATURE_LEVEL_11_0 };
        HRESULT res = D3D11CreateDevice(0, D3D_DRIVER_TYPE_HARDWARE, 0, flags,
                                        features, ARRAY_SIZE(features), D3D11_SDK_VERSION,
                                        &d3d11_state.device, 0, &d3d11_state.context);
        
        if (res != S_OK) {
            er_push(str8("Failed to create d3d11 device and context"));
            error = 1;
        }
    }
    
#ifndef NDEBUG
    if (!error) {
        ID3D11InfoQueue *info;
        d3d11_state.device->QueryInterface(__uuidof(ID3D11InfoQueue), (void **) &info);
        info->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, 1);
        info->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, 1);
        info->Release();
    }
#endif
    
    ID3DBlob *vshader = 0;
    ID3DBlob *pshader = 0;
    if (!error) {
        UINT flags = D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR | D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS;
#ifndef NDEBUG
        flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
        flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif
        ID3DBlob *d3d11_verror = 0;
        ID3DBlob *d3d11_perror = 0;
        HRESULT v_res = D3DCompile(hlsl, sizeof(hlsl), 0, 0, 0, "main_vs", "vs_5_0", flags, 0, &vshader, &d3d11_verror);
        HRESULT p_res = D3DCompile(hlsl, sizeof(hlsl), 0, 0, 0, "main_ps", "ps_5_0", flags, 0, &pshader, &d3d11_perror);
        
        if (v_res != S_OK || p_res != S_OK) {
#ifndef NDEBUG
            const char *v_msg = (const char *) d3d11_verror->GetBufferPointer();
            const char *p_msg = (const char *) d3d11_perror->GetBufferPointer();
            OutputDebugString(v_msg);
            OutputDebugString(p_msg);
#endif
            er_push(str8("Error creating shaders"));
            error = 1;
        }
        
        if (d3d11_verror) {
            d3d11_verror->Release();
        }
        
        if (d3d11_perror) {
            d3d11_perror->Release();
        }
    }
    
    if (!error) {
        D3D11_INPUT_ELEMENT_DESC desc[] = {
            // Template
            { "POS", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            
            // Instancing
            { "QUAD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, offsetof(R_Quad, pos), D3D11_INPUT_PER_INSTANCE_DATA, 1 },
            { "UV", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, offsetof(R_Quad, uv), D3D11_INPUT_PER_INSTANCE_DATA, 1 },
            { "COL", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 1, offsetof(R_Quad, col), D3D11_INPUT_PER_INSTANCE_DATA, 1 },
            { "RADIUS", 0, DXGI_FORMAT_R32_FLOAT, 1, offsetof(R_Quad, radius), D3D11_INPUT_PER_INSTANCE_DATA, 1 },
            { "THETA", 0, DXGI_FORMAT_R32_FLOAT, 1, offsetof(R_Quad, theta), D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        };
        
        d3d11_state.device->CreateVertexShader(vshader->GetBufferPointer(), vshader->GetBufferSize(), NULL, &d3d11_state.vertex_shader);
        d3d11_state.device->CreatePixelShader(pshader->GetBufferPointer(), pshader->GetBufferSize(), NULL, &d3d11_state.pixel_shader);
        d3d11_state.device->CreateInputLayout(desc, ARRAY_SIZE(desc), vshader->GetBufferPointer(), vshader->GetBufferSize(), &d3d11_state.layout);
        
        vshader->Release();
        pshader->Release();
    }
    
    if (!error) {
        // @Note: This has to be defined in a specific way because we're using
        // triangle-strip as a primitive + back-face culling.
        R_Vertex v_data[] = {
            { { -1.0f, +1.0f } },
            { { -1.0f, -1.0f } },
            { { +1.0f, +1.0f } },
            { { +1.0f, -1.0f } },
        };
        D3D11_SUBRESOURCE_DATA vertex_data = { v_data };
        
        D3D11_BUFFER_DESC desc = {0};
        desc.ByteWidth      = sizeof(v_data);
        desc.Usage          = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
        desc.CPUAccessFlags = 0;
        d3d11_state.device->CreateBuffer(&desc, &vertex_data, &d3d11_state.buffer[V_BUFFER]);

        // @ToDo: Grow buffer when it becomes full.
        desc.ByteWidth      = R_D3D11_BUFFER_INIT_CAP;
        desc.Usage          = D3D11_USAGE_DYNAMIC;
        desc.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        d3d11_state.device->CreateBuffer(&desc, 0, &d3d11_state.buffer[I_BUFFER]);
        
        desc.ByteWidth      = sizeof(D3D11_Cbuffer) + 0xF & 0xFFFFFFF0; // @Note: This _has to_ be a multiple of 16
        desc.Usage          = D3D11_USAGE_DYNAMIC;
        desc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        d3d11_state.device->CreateBuffer(&desc, 0, &d3d11_state.buffer[C_BUFFER]);
    }
    
    if (!error) {
        D3D11_RASTERIZER_DESC desc = {0};
        desc.FillMode = D3D11_FILL_SOLID;
        desc.CullMode = D3D11_CULL_BACK;
        desc.ScissorEnable = 1;
        
        d3d11_state.device->CreateRasterizerState(&desc, &d3d11_state.rasterizer);
    }
    
    if (!error) {
        D3D11_BLEND_DESC desc = {0};
        desc.RenderTarget[0].BlendEnable = 1;
        desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
        desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        
        d3d11_state.device->CreateBlendState(&desc, &d3d11_state.blend_state);
    }
    
    if (!error) {
        D3D11_SAMPLER_DESC desc = {0};
        desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
        desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
        desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
        desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        desc.MipLODBias = 0;
        desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        desc.MinLOD = 0;
        desc.MaxLOD = D3D11_FLOAT32_MAX;
        
        d3d11_state.device->CreateSamplerState(&desc, &d3d11_state.sampler_state);
    }
    
    // @Note: It's better not to use 'r_create_texture' here because 
    // we are treating this entire thing slightly differently and want more control
    if (!error) {
        D3D11_TEXTURE2D_DESC tex_desc = {0};
        tex_desc.Width = 1;
        tex_desc.Height = 1;
        tex_desc.MipLevels = 1;
        tex_desc.ArraySize = 1;
        tex_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        tex_desc.SampleDesc.Count = 1;
        tex_desc.SampleDesc.Quality = 0;
        tex_desc.Usage = D3D11_USAGE_DYNAMIC;
        tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        tex_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        tex_desc.MiscFlags = 0;
        
        u32 data[] = { 0xFFFFFFFF };
        D3D11_SUBRESOURCE_DATA tex_data = {0};
        tex_data.pSysMem = data;
        tex_data.SysMemPitch = sizeof(u32);
        tex_data.SysMemSlicePitch = 0; // @Note: Only used in 3D textures according to docs
        
        d3d11_state.device->CreateTexture2D(&tex_desc, &tex_data, &d3d11_state.dummy_texture.data);
        d3d11_state.device->CreateShaderResourceView(d3d11_state.dummy_texture.data, 0, &d3d11_state.dummy_texture.view);
    }
    
    if (!error) {
        d3d11_is_init = 1;
    }
    
    b32 result = !error;
    return(result);
}

internal void r_backend_end(void)
{
    if (d3d11_state.device) d3d11_state.device->Release();
    if (d3d11_state.context) d3d11_state.context->Release();
    
    if (d3d11_state.buffer[V_BUFFER]) d3d11_state.buffer[V_BUFFER]->Release();
    if (d3d11_state.buffer[I_BUFFER]) d3d11_state.buffer[I_BUFFER]->Release();
    if (d3d11_state.buffer[C_BUFFER]) d3d11_state.buffer[C_BUFFER]->Release();
    
    if (d3d11_state.layout) d3d11_state.layout->Release();
    if (d3d11_state.vertex_shader) d3d11_state.vertex_shader->Release();
    if (d3d11_state.pixel_shader) d3d11_state.pixel_shader->Release();
    
    if (d3d11_state.rasterizer) d3d11_state.rasterizer->Release();
    if (d3d11_state.blend_state) d3d11_state.blend_state->Release();
    if (d3d11_state.sampler_state) d3d11_state.sampler_state->Release();
    
    if (d3d11_state.arena) arena_release(d3d11_state.arena);
    if (d3d11_state.dummy_texture.data) d3d11_state.dummy_texture.data->Release();
    if (d3d11_state.dummy_texture.view) d3d11_state.dummy_texture.view->Release();
}

internal b32 r_window_equip(GFX_Window *window)
{
    b32 error = 0;
    if (!r_is_init()) {
        er_push(str8("render backend not initialized"));
        error = 1;
    }
    
    if (!gfx_window_is_valid(window)) {
        er_push(str8("Invalid window provided"));
        error = 1;
    }    
    
    D3D11_Window *w = 0;
    if (!error) {
        w = d3d11_window_from_opaque(window);
        if (w->swap_chain) {
            er_push(str8("Window already equipped, swap_chain already assigned"));
            error = 1;
        }
    }
    
    if (!error) {
        Win32_Window *win32_window = gfx_win32_window_from_opaque(window);
        IDXGIFactory *factory = 0;
        CreateDXGIFactory(__uuidof(IDXGIFactory), (void **) &factory);
        
        // @ToDo: Change to dxgi_swap_chain_desc1
        DXGI_SWAP_CHAIN_DESC desc = {0};
        desc.BufferDesc.Width            = 0;
        desc.BufferDesc.Height           = 0;
        desc.BufferDesc.RefreshRate      = {0};
        desc.BufferDesc.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
        desc.BufferDesc.Scaling          = DXGI_MODE_SCALING_UNSPECIFIED;
        desc.SampleDesc.Count            = 1;
        desc.SampleDesc.Quality          = 0;
        desc.BufferUsage                 = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount                 = 2;
        desc.OutputWindow                = win32_window->handle;
        desc.Windowed                    = 1;
        desc.SwapEffect                  = DXGI_SWAP_EFFECT_DISCARD; // @Note: Compatibility with Windows 7
        desc.Flags                       = 0;
        
        factory->CreateSwapChain(d3d11_state.device, &desc, &w->swap_chain);
        factory->Release();
        
        if (w->swap_chain == 0) {
            er_push(str8("Error creating swap chain"));
            error = 1;
        }
    }
    
    if (!error) {
        f32 width = 0.0f;
        f32 height = 0.0f;
        gfx_window_get_rect(window, &width, &height);
        
        w->viewport.TopLeftX = 0;
        w->viewport.TopLeftY = 0;
        w->viewport.Width = width;
        w->viewport.Height = height;
        w->viewport.MinDepth = 0.0f;
        w->viewport.MaxDepth = 1.0f;
    }
    
    b32 result = !error;
    return(result);
}

internal void r_window_unequip(GFX_Window *window)
{
    if (!r_is_init()) {
        er_push(str8("render backend not initialized"));
    } else {
        if (gfx_window_is_valid(window)) {
            D3D11_Window *w = d3d11_window_from_opaque(window);
            if (w->swap_chain) {
                w->swap_chain->Release();
                
                // @Note: d3d11 waits before it can free elements possibly because
                // of some optimizations, so we need to 'force' it to clear state.
                d3d11_state.context->ClearState();
                d3d11_state.context->Flush();
                
                MemoryZero(w, sizeof(D3D11_Window));
            }
        }
    }
}

internal void r_frame_begin(GFX_Window *window)
{
    OPTICK_EVENT();
    
    if (!r_is_init()) {
        er_push(str8("render backend not initialized"));
    } else {
        if (gfx_window_is_valid(window)) {
            D3D11_Window *w = d3d11_window_from_opaque(window);
            if (w->swap_chain) {
                f32 width, height;
                gfx_window_get_rect(window, &width, &height);
                
                if (width > 0 && height > 0) {
                    w->swap_chain->ResizeBuffers(0, (UINT) width, (UINT) height, DXGI_FORMAT_UNKNOWN, 0);
                }
                
                ID3D11Texture2D *back_buff = 0;
                w->swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **) &back_buff);
                d3d11_state.device->CreateRenderTargetView(back_buff, 0, &w->target);
                back_buff->Release();
                
                const FLOAT clear_color[] = { 18.0f/255.0f, 18.0f/255.0f, 18.0f/255.0f, 1.0f };
                d3d11_state.context->ClearRenderTargetView(w->target, clear_color);
            }
        }
    }
}

internal b32 r_submit_quads(GFX_Window *window, R_Quad_Node *draw_data, usize total_quad_count, R_Texture2D *texture)
{
    OPTICK_EVENT();
    
    b32 error = 0;
    if (!r_is_init()) {
        er_push(str8("render backend not initialized"));
        error = 1;
    }
    
    if (!gfx_window_is_valid(window)) {
        er_push(str8("provided window is invalid"));
        error = 1;
    }
    
    if (!error) {
        f32 width, height;
        gfx_window_get_rect(window, &width, &height);
        
        D3D11_Window *w = d3d11_window_from_opaque(window);
        w->viewport.Width = width;
        w->viewport.Height = height;
        
        w->scissors.top = 0;
        w->scissors.left = 0;
        w->scissors.bottom = (LONG) height;
        w->scissors.right = (LONG) width;
        
        d3d11_state.cbuffer.res.X = width;
        d3d11_state.cbuffer.res.Y = height;
        
        D3D11_MAPPED_SUBRESOURCE cbuffer_resource = {0};
        d3d11_state.context->Map(d3d11_state.buffer[C_BUFFER], 0, D3D11_MAP_WRITE_DISCARD, 0, &cbuffer_resource);
        MemoryCopy(cbuffer_resource.pData, &d3d11_state.cbuffer, sizeof(D3D11_Cbuffer));
        d3d11_state.context->Unmap(d3d11_state.buffer[C_BUFFER], 0);
        
        D3D11_MAPPED_SUBRESOURCE vertex_resource = {0};
        d3d11_state.context->Map(d3d11_state.buffer[I_BUFFER], 0, D3D11_MAP_WRITE_DISCARD, 0, &vertex_resource);
        {
            u8 *dst_ptr = (u8 *) vertex_resource.pData;
            usize offset = 0;
            for (R_Quad_Node *node = draw_data; node != 0; node = node->next) {
                usize bytes = node->count * sizeof(R_Quad);
                MemoryCopy(dst_ptr, node->quads, bytes);
                offset += bytes;
            }
        }
        d3d11_state.context->Unmap(d3d11_state.buffer[I_BUFFER], 0);
        
        D3D11_Texture *d3d11_texture = 0;
        if (texture == 0) {
            d3d11_texture = &d3d11_state.dummy_texture;
        } else {
            d3d11_texture = (D3D11_Texture *) texture;
        }
        
        ID3D11Buffer *buffers[] = { d3d11_state.buffer[V_BUFFER], d3d11_state.buffer[I_BUFFER] };
        UINT strides[] = { sizeof(R_Vertex), sizeof(R_Quad) };
        UINT offsets[] = { 0, 0 };
        
        d3d11_state.context->IASetInputLayout(d3d11_state.layout);
        d3d11_state.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        d3d11_state.context->IASetVertexBuffers(0, ARRAY_SIZE(buffers), buffers, strides, offsets);
        
        d3d11_state.context->VSSetConstantBuffers(0, 1, &d3d11_state.buffer[C_BUFFER]);
        d3d11_state.context->VSSetShader(d3d11_state.vertex_shader, 0, 0);
        
        d3d11_state.context->RSSetState(d3d11_state.rasterizer);
        d3d11_state.context->RSSetViewports(1, &w->viewport);
        d3d11_state.context->RSSetScissorRects(1, &w->scissors);
        
        d3d11_state.context->PSSetShader(d3d11_state.pixel_shader, 0, 0);
        d3d11_state.context->PSSetSamplers(0, 1, &d3d11_state.sampler_state);
        d3d11_state.context->PSSetShaderResources(0, 1, &d3d11_texture->view);
        
        d3d11_state.context->OMSetRenderTargets(1, &w->target, 0);
        d3d11_state.context->OMSetBlendState(d3d11_state.blend_state, 0, 0xFFFFFFFF);
        
        d3d11_state.context->DrawInstanced(4, (UINT) total_quad_count, 0, 0);
    }
    
    b32 result = !error;
    return(result);
}

internal void r_frame_end(GFX_Window *window)
{
    OPTICK_EVENT();
    
    if (!r_is_init()) {
        er_push(str8("render backend not initialized"));
    } else {
        if (gfx_window_is_valid(window)) {
            D3D11_Window *w = d3d11_window_from_opaque(window);
            if (w->swap_chain && w->target) {
                w->swap_chain->Present(0, 0);
                w->target->Release();
            }
        }
    }
}

internal u8 *r_frame_end_get_backbuffer(GFX_Window *window, Arena *arena)
{
    u8 *result = 0;
    if (!r_is_init()) {
        er_push(str8("render backend not initialized"));
    } else {
        if (gfx_window_is_valid(window)) {
            D3D11_Window *w = d3d11_window_from_opaque(window);
            if (w->swap_chain && w->target) {
                w->swap_chain->Present(0, 0);

                f32 width, height;
                gfx_window_get_rect(window, &width, &height);
                
                u32 bytes = (u32) (width*height*4);
                result = arena_push_array(arena, u8, bytes);

                ID3D11Texture2D *surface = 0;
                w->swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **) &surface);

                D3D11_TEXTURE2D_DESC desc = {0};
                surface->GetDesc(&desc);
                desc.BindFlags = 0;
                desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
                desc.Usage = D3D11_USAGE_STAGING;

                ID3D11Texture2D *tex = 0;
                d3d11_state.device->CreateTexture2D(&desc, 0, &tex);
                d3d11_state.context->CopyResource(tex, surface);
                
                D3D11_MAPPED_SUBRESOURCE texture_resource = {0};
                d3d11_state.context->Map(tex, 0, D3D11_MAP_READ_WRITE, 0, &texture_resource);

                u8 *dst = result;
                u8 *src = ((u8 *) texture_resource.pData);
                for (u32 i = 0; i < (u32) height; ++i) {
                    MemoryCopy(dst, src, (u32) (width*4));
                    
                    dst += (u32) (width*4);
                    src += texture_resource.RowPitch;
                }
                
                d3d11_state.context->Unmap(tex, 0);
                    
                tex->Release();
                surface->Release();
                
                w->target->Release();
            }
        }
    }

    return(result);
}

internal R_Texture2D *r_texture_create(void *data, u32 width, u32 height)
{
    Assert(IS_POW2(width) && IS_POW2(height) && width != 0 && height != 0);
    
    R_Texture2D *result = 0;
    
    if (!r_is_init()) {
        er_push(str8("render backend not initialized"));
    } else {
        D3D11_Texture *texture = d3d11_state.first_free_texture;
        if (texture == 0) {
            texture = arena_push_array(d3d11_state.arena, D3D11_Texture, 1);
        } else {
            SLLStackPop(d3d11_state.first_free_texture);
            MemoryZero(texture, sizeof(D3D11_Texture));
        }
        
        D3D11_TEXTURE2D_DESC tex_desc = {0};
        tex_desc.Width = width;
        tex_desc.Height = height;
        tex_desc.MipLevels = 1;
        tex_desc.ArraySize = 1;
        tex_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        tex_desc.SampleDesc.Count = 1;
        tex_desc.SampleDesc.Quality = 0;
        tex_desc.Usage = D3D11_USAGE_DEFAULT;
        tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        tex_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        tex_desc.MiscFlags = 0;
        
        D3D11_SUBRESOURCE_DATA tex_data = {0};
        tex_data.pSysMem = data;
        tex_data.SysMemPitch = sizeof(u32)*width;
        tex_data.SysMemSlicePitch = 0; // @Note: Only used in 3D textures according to docs
        
        d3d11_state.device->CreateTexture2D(&tex_desc, &tex_data, &texture->data);
        d3d11_state.device->CreateShaderResourceView(texture->data, 0, &texture->view);
        
        result = (R_Texture2D *) texture;
    }
    
    return(result);
}

internal b32 r_texture_destroy(R_Texture2D *texture)
{
    b32 error = 0;
    if (!r_is_init()) {
        er_push(str8("render backend not initialized"));
        error = 1;
    }
    
    if (texture == 0) {
        er_push(str8("Provided texture was null"));
        error = 1;
    }
    
    if (!error) {
        D3D11_Texture *d3d11_texture = (D3D11_Texture *) texture;
        d3d11_texture->data->Release();
        d3d11_texture->view->Release();
        
        SLLStackPush(d3d11_state.first_free_texture, d3d11_texture);
        d3d11_texture = 0;
    }
    
    b32 result = !error;
    return(result);
}

// @ToDo: Update a sub-region of a texture
internal b32 r_texture_update(R_Texture2D *texture, void *data, u32 width, u32 height)
{
    OPTICK_EVENT();
    
    b32 error = 0;
    if (!r_is_init()) {
        er_push(str8("render backend not initialized"));
        error = 1;
    }
    
    if (texture == 0) {
        er_push(str8("Provided texture was null"));
        error = 1;
    }
    
    if (!error) {
        D3D11_Texture *d3d11_texture = (D3D11_Texture *) texture;
        
        D3D11_BOX box = {0};
        box.front = 0;
        box.back = 1;
        box.left = 0;
        box.right = width;
        box.top = 0;
        box.bottom = height;
        
        d3d11_state.context->UpdateSubresource(d3d11_texture->data, 0, &box, data, sizeof(u32)*width, sizeof(u32)*width*height);
        
        // @ToDo: This is causing 'use after poison' sometimes, should investigate
#if 0
        D3D11_MAPPED_SUBRESOURCE texture_resource = {0};
        d3d11_state.context->Map(d3d11_texture->data, 0, D3D11_MAP_WRITE_DISCARD, 0, &texture_resource);
        MemoryCopy(texture_resource.pData, data, sizeof(u32)*width*height);
        d3d11_state.context->Unmap(d3d11_texture->data, 0);
#endif
    }
    
    b32 result = !error;
    return(result);
}
