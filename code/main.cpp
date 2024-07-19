/* @Note: This file is a test for font rendering
 * ideally we want something that just creates a texture atlas with all
 * glyphs and returns that as a texture. We just want a textured quad for now, nothing
 * too fancy.
 *
 * For rasterisation we could look into DWrite but its API and docs are kind of a pain
 * to go through.
 */
#define R_BACKEND_D3D11 1

#define FPS 60
#define FRAME_MS (1000.0/FPS)

#define WIDTH 1280
#define HEIGHT 720

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stdio.h>
#include <stb_image_write.h>

#include <HandmadeMath.h>
#include <optick.h>

// @Note: For some reason these _have to_ be declared at top level
// otherwise we get C2208 compile error...
#define FONT_USE_FREETYPE 1
#include <ft2build.h>
#include <freetype/freetype.h>

#include "./base/base_inc.h"
#include "./os/os_inc.h"
#include "./gfx/gfx_inc.h"
#include "./render/render_inc.h"
#include "./font/font_inc.h" // @Note: Include font after render, maybe there's a way to de-couple those...

#include "./base/base_inc.c"
#include "./os/os_inc.c"
#include "./gfx/gfx_inc.c"
#include "./render/render_inc.c"
#include "./font/font_inc.c"

typedef struct Graph_Data Graph_Data;
struct Graph_Data
{
    u32 data_size;
    f32 *xs;
    f32 *ys;
};

typedef struct Camera Camera;
struct Camera
{
    HMM_Vec2 offset;
    f32 scale;
};

internal void screen_to_camera(Camera *camera, f32 x, f32 y, f32 *ox, f32 *oy)
{
    *ox = (x + camera->offset.X) * camera->scale;
    *oy = (y + camera->offset.Y) * camera->scale;
}

internal void camera_to_screen(Camera *camera, f32 x, f32 y, f32 *ox, f32 *oy)
{
    *ox = (x/camera->scale) - camera->offset.X;
    *oy = (y/camera->scale) - camera->offset.Y;
}

internal f32 lerpf(f32 a, f32 b, f32 t)
{
    return(a*(1.0f - t) + b*t);
}

internal f32 ilerpf(f32 a, f32 b, f32 v)
{
    return((v - a)/(b - a));
}

internal void r_grid(R_Ctx *ctx, R_Ctx *font_ctx, Font *font, HMM_Vec2 origin, HMM_Vec2 window_size, f32 line_spacing)
{
    const f32 line_width = 2.0f;
    const f32 padding = 10.0f;
    HMM_Vec2 start = {0};
    
    const f32 a = (f32) ((s32) (origin.X/line_spacing));
    const f32 b = (f32) ((s32) (origin.Y/line_spacing));
    
    start = { origin.X - a*line_spacing, 0.0f };
    while (start.X <= window_size.X) {    
        RectF32 rect = {
            start.X - line_width*.5f, 0.0f,
            start.X + line_width*.5f, window_size.Y
        };
    
        String8 str = str8("xn");
        HMM_Vec2 text_pos = { start.X + padding, origin.Y + font->font_size + padding };
        
        r_rect(ctx, rect, 0x262626FF, 0.0f);
        font_r_text(font_ctx, font, text_pos, str);
        start.X += line_spacing;
    }

    start = { 0.0f, origin.Y - b*line_spacing };
    while (start.Y <= window_size.Y) {    
        RectF32 rect = {
            0.0f, start.Y - line_width*.5f,
            window_size.X, start.Y + line_width*.5f
        };

        String8 str = str8("yn");
        f32 w = font_text_width(font, str);
        HMM_Vec2 text_pos = { origin.X - w - padding, start.Y + font->font_size + padding };
            
        r_rect(ctx, rect, 0x262626FF, 0.0f);
        font_r_text(font_ctx, font, text_pos, str);
        start.Y += line_spacing;
    }

    RectF32 axis_x = {
        0.0f, origin.Y - line_width, 
        window_size.X, origin.Y + line_width
    };
            
    RectF32 axis_y = {
        origin.X - line_width, 0.0f,
        origin.X + line_width, window_size.Y
    };
    
    r_rect(ctx, axis_y, 0x4A4A4AFF, 0.0f);
    r_rect(ctx, axis_x, 0x4A4A4AFF, 0.0f);
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prev_instance, LPSTR cmd_line, int cmd_show)
{
    UNUSED(instance);
    UNUSED(prev_instance);
    UNUSED(cmd_line);
    UNUSED(cmd_show);
    
    // @Note: Init modules
    {
        os_main_init();
        gfx_init();
        r_backend_init();
    }
    
    Arena *arena = arena_make();
    Arena *frame_arena = arena_make();
    
    GFX_Window *window = gfx_window_create(str8("A window"), WIDTH, HEIGHT);
    gfx_window_set_resizable(window, 1);
    gfx_window_set_visible(window, 1);
    gfx_window_set_destroy_func(window, r_window_unequip);
    
    r_window_equip(window);
    
    Font font = font_init(arena, str8("./Inconsolata-Regular.ttf"), 16, 96);
    
    Camera camera = {0};
    b32 track_mouse = 0;
    HMM_Vec2 mouse = {0};
    camera.scale = 1.0f;
    
    const f32 controls_size = 30.0f;
    const f32 padding = 30.0f;
    
    const u32 data_size = 5;
    f32 xs[data_size] = { 0.0f, 1.0f, 2.5f, 5.0f, 10.0f };
    f32 ys[data_size] = { 0.0f, 1.0f, 4.0f, 5.0f, 10.0f };

    const f32 scale_step = 0.1f;
    const f32 scale_max = 3.0f;
    const f32 scale_min = 0.4f;
    
    // f32 max_x = 5.0f;
    // f32 max_y = 5.0f;
    
    b32 should_quit = 0;
    f64 frame_prev = os_ticks_now();
    
    while (!should_quit) {
        OPTICK_FRAME("Main");
        
        arena_clear(frame_arena);
        
#ifndef NDEBUG
        er_accum_start();
#endif
        
        f64 frame_start = os_ticks_now();
        // f64 dt = (frame_start - frame_prev)/1000.0f;
        frame_prev = frame_start;
        
        GFX_Event_List event_list = gfx_process_input(frame_arena);
        for (GFX_Event *event = event_list.first; event != 0; event = event->next) {
            gfx_events_eat(&event_list);
            
            if (event->kind == GFX_EVENT_QUIT) {
                should_quit = 1;
                goto frame_end;
            }

            switch (event->kind) {
                case GFX_EVENT_MBUTTONDOWN:
                case GFX_EVENT_LBUTTONDOWN: {
                    gfx_mouse_set_capture(window, 1);
                    mouse = event->mouse;
                    track_mouse = 1;
                } break;

                case GFX_EVENT_MBUTTONUP:
                case GFX_EVENT_LBUTTONUP: {
                    gfx_mouse_set_capture(window, 0);
                    track_mouse = 0;
                } break;

                case GFX_EVENT_MOUSEMOVE: {
                    if (track_mouse) {
                        camera.offset.X += (event->mouse.X - mouse.X)/camera.scale;
                        camera.offset.Y += (event->mouse.Y - mouse.Y)/camera.scale;
                    }
                    mouse = event->mouse;
                } break;

                case GFX_EVENT_MOUSEWHEEL: {
                    HMM_Vec2 before = {0};
                    HMM_Vec2 after = {0};
                    camera_to_screen(&camera, mouse.X, mouse.Y, &before.X, &before.Y);
                    {
                        if (event->mouse_wheel > 0.0f) {
                            camera.scale = MIN(camera.scale + scale_step, scale_max);
                        } else {
                            camera.scale = MAX(camera.scale - scale_step, scale_min);
                        }
                    }
                    camera_to_screen(&camera, mouse.X, mouse.Y, &after.X, &after.Y);
                    camera.offset.X += after.X - before.X;
                    camera.offset.Y += after.Y - before.Y;
                } break; 
            }
        }

        HMM_Vec2 window_size = {0};
        gfx_window_get_rect(window, &window_size.X, &window_size.Y);
        
        R_List list = {0};
        R_Ctx ctx = r_make_context(frame_arena, &list);

        R_List font_list = {0};
        R_Ctx font_ctx = r_make_context(frame_arena, &font_list);
        
        const f32 line_spacing = 80.0f*camera.scale;
        
        r_frame_begin(window);

        // @Note: Rendering graph
        {
            HMM_Vec2 origin = { padding, window_size.Y - padding };
            screen_to_camera(&camera, origin.X, origin.Y, &origin.X, &origin.Y);
            
            f32 t = ilerpf(scale_min, scale_max, camera.scale);
            f32 grid_split = (f32) ((s32) lerpf(3.0f, 1.0f, t));
            
            r_grid(&ctx, &font_ctx, &font, origin, window_size, line_spacing*grid_split);
            
            const HMM_Vec2 step = { 1.0f, 1.0f };
            for (u32 i = 0; i < data_size; ++i) {
                HMM_Vec2 point_pos = {
                    origin.X + ((xs[i]/step.X)*line_spacing),
                    origin.Y - ((ys[i]/step.Y)*line_spacing)
                };
                r_circ(&ctx, point_pos, 6.0f, 0xFF0000FF);
            }
        }

        // @Note: Rendering and handling ui
        {
            RectF32 controls = {
                0.0f, 0.0f,
                window_size.X, controls_size
            };
            r_rect(&ctx, controls, 0x333333FF, 0.0f);
        }
        
        r_flush_batches(window, &font_list);
        r_flush_batches(window, &list);

        r_frame_end(window);
        // u8 *pixels =  r_frame_end_get_backbuffer(frame_arena, window, (s32) window_size.X, (s32) window_size.Y);
        // stbi_write_jpg("ss.png", (s32) window_size.X, (s32) window_size.Y, 4, pixels, 100);
        
    frame_end:
        {
#ifndef NDEBUG
            Arena_Temp temp = arena_temp_begin(arena);
            String8 error = er_accum_end(temp.arena);
            if (error.size != 0) {
                gfx_error_display(window, error, str8("Error"));
                os_exit_process(1);
            }
            arena_temp_end(&temp);
#endif
        }
        
        f64 frame_time = os_ticks_now() - frame_start;
        if (frame_time < FRAME_MS) {
            os_wait(FRAME_MS - frame_time);
        }
    }
    
    gfx_window_destroy(window);
    r_backend_end();
    
    return 0;
}
