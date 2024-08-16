#define R_BACKEND_D3D11 1
#define FONT_USE_FREETYPE 1

#define FPS 60
#define FRAME_MS (1000.0/FPS)

#define WIDTH 1280
#define HEIGHT 720

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <HandmadeMath.h>
#include <optick.h>

// @Note: For some reason these _have to_ be declared at top level
// otherwise we get C2208 compile error...
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
    f32 *xs;
    f32 *ys;
    u32 size;
};

typedef struct Camera Camera;
struct Camera
{
    HMM_Vec2 offset;
    f32 scale;    
    
    f32 scale_step;
    f32 scale_min;
    f32 scale_max;
};

typedef struct State State;
struct State
{
    Font font;
    b32 light_mode;
    b32 auto_scale;
    b32 show_slider_control;
    Graph_Data graph_data;
    
    HMM_Vec2 x_range;
    HMM_Vec2 y_range;
    HMM_Vec2 graph_step;
    HMM_Vec2 pixels_per_unit;
    HMM_Vec2 graph_origin;

    b32 track_mouse;
    HMM_Vec2 mouse;

    Camera camera;
};

global State state = {0};

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

internal void graph_fit_limits(GFX_Window *window)
{
    HMM_Vec2 window_size = {0};
    gfx_window_get_rect(window, &window_size.X, &window_size.Y);
    
    if (window_size.X > 0.0f && window_size.Y > 0.0f) {    
        HMM_Vec2 origin_point = {0};
        HMM_Vec2 top_left = {0};
        HMM_Vec2 bottom_right = { window_size.X, window_size.Y };
        screen_to_camera(&state.camera, state.graph_origin.X, state.graph_origin.Y, &origin_point.X, &origin_point.Y);
        camera_to_screen(&state.camera, top_left.X, top_left.Y, &top_left.X, &top_left.Y);
        camera_to_screen(&state.camera, bottom_right.X, bottom_right.Y, &bottom_right.X, &bottom_right.Y);
    
        state.x_range.X = (top_left.X - state.graph_origin.X)/state.pixels_per_unit.X;
        state.x_range.Y = (bottom_right.X - state.graph_origin.X)/state.pixels_per_unit.X;
        state.y_range.X = (state.graph_origin.Y - bottom_right.Y)/state.pixels_per_unit.Y;
        state.y_range.Y = (state.graph_origin.Y - top_left.Y)/state.pixels_per_unit.Y;

        if (state.auto_scale) {
            f32 current_x_fit = (state.x_range.Y - state.x_range.X) / state.graph_step.X;
            f32 current_y_fit = (state.y_range.Y - state.y_range.X) / state.graph_step.Y;
            
            while (current_x_fit > 12.0f) {
                state.graph_step.X *= 2.0f;
                current_x_fit = (state.x_range.Y - state.x_range.X) / state.graph_step.X;
            }
            while (current_x_fit < 4.0f) {
                state.graph_step.X /= 2.0f;
                current_x_fit = (state.x_range.Y - state.x_range.X) / state.graph_step.X;
            }
            while (current_y_fit > 12.0f) {
                state.graph_step.Y *= 2.0f;
                current_y_fit = (state.y_range.Y - state.y_range.X) / state.graph_step.Y;
            }
            while (current_y_fit < 4.0f) {
                state.graph_step.Y /= 2.0f;
                current_y_fit = (state.y_range.Y - state.y_range.X) / state.graph_step.Y;
            }
            
            state.x_range.Y /= state.graph_step.X;
            state.x_range.X /= state.graph_step.X;
            state.y_range.Y /= state.graph_step.Y;
            state.y_range.X /= state.graph_step.Y;
        }
    }
}
    
internal void r_graph(GFX_Window *window, R_Ctx *ctx, R_Ctx *ui_ctx, b32 light_mode)
{
    OPTICK_EVENT();

    const u32 text_col[] = { 0xFFFFFFFF, 0x121212FF };
    const u32 grid_col[] = { 0x262626FF, 0xBFBFBFFF };
    const u32 line_col[] = { 0x4A4A4AFF, 0x0F0F0FFF };
    
    HMM_Vec2 window_size;
    gfx_window_get_rect(window, &window_size.X, &window_size.Y);

    HMM_Vec2 origin_point = {0};
    screen_to_camera(&state.camera, state.graph_origin.X, state.graph_origin.Y, &origin_point.X, &origin_point.Y);
 
    const f32 line_width = 2.0f;
    const f32 padding = 10.0f;
    const HMM_Vec2 ppu = {
        state.camera.scale*state.graph_step.X*state.pixels_per_unit.X,
        state.camera.scale*state.graph_step.Y*state.pixels_per_unit.Y
    };

    for (s32 i = (s32) state.x_range.X; i <= (s32) state.x_range.Y; ++i) {
        if (i == 0) continue;

        HMM_Vec2 start = {
            origin_point.X + i*ppu.X,
            0.0f
        };

        RectF32 rect = {
            start.X - line_width*.5f, 0.0f,
            start.X + line_width*.5f, window_size.Y
        };
        
        // @ToDo: Find a better way to create a string
        char buff[32] = {0};
        snprintf(buff, 32, "%.2f", i * state.graph_step.X);
        String8 str = str8_from_cstr(buff);
        
        f32 w = font_text_width(&state.font, str);
        HMM_Vec2 text_pos = { start.X - w*.5f, origin_point.Y + state.font.font_size + padding };
        
        // @ToDo: font->font_size*2.0f is hardcoded for now because of the bar at the top.
        f32 offset = light_mode ? 0.0f : state.font.font_size*2.0f;
        if (text_pos.Y <= 2.0f*padding + offset) { 
            text_pos.Y = 2.0f*padding + offset;
        } else if (text_pos.Y + padding >= window_size.Y) {
            text_pos.Y = window_size.Y - padding;
        }
        
        r_rect(ctx, rect, grid_col[light_mode], 0.0f);
        font_r_text(ui_ctx, &state.font, text_pos, text_col[light_mode], str);
    }

    for (s32 i = (s32) state.y_range.X; i <= (s32) state.y_range.Y; ++i) {
        if (i == 0) continue;

        HMM_Vec2 start = {
            0.0f,
            origin_point.Y - i*ppu.Y
        };

        RectF32 rect = {
            0.0f, start.Y - line_width*.5f,
            window_size.X, start.Y + line_width*.5f
        };

        // @ToDo: Find a better way to create a string
        char buff[32] = {0};
        snprintf(buff, 32, "%.2f", i * state.graph_step.Y);
        String8 str = str8_from_cstr(buff);
        
        f32 w = font_text_width(&state.font, str);
        HMM_Vec2 text_pos = { origin_point.X - w - padding, start.Y - line_width + state.font.font_size*.5f};
        
        if (text_pos.X <= padding) {
            text_pos.X = padding;
        } else if (text_pos.X + w + padding >= window_size.X) {
            text_pos.X = window_size.X - w - padding;
        }
        
        r_rect(ctx, rect, grid_col[light_mode], 0.0f);
        font_r_text(ui_ctx, &state.font, text_pos, text_col[light_mode], str);
    }
    
    RectF32 axis_x = {
        0.0f, origin_point.Y - line_width, 
        window_size.X, origin_point.Y + line_width
    };
            
    RectF32 axis_y = {
        origin_point.X - line_width, 0.0f,
        origin_point.X + line_width, window_size.Y
    };
    
    r_rect(ctx, axis_y, line_col[light_mode], 0.0f);
    r_rect(ctx, axis_x, line_col[light_mode], 0.0f);

    const HMM_Vec2 scale = {
        state.camera.scale*state.pixels_per_unit.X,
        state.camera.scale*state.pixels_per_unit.Y
    };     
    
    // @Note: Draw points
    for (u32 i = 0; i < state.graph_data.size; ++i) {
        HMM_Vec2 point_pos = {
            origin_point.X + (state.graph_data.xs[i]*scale.X),
            origin_point.Y - (state.graph_data.ys[i]*scale.Y)
        };
        r_circ(ctx, point_pos, 8.0f, 0xFF0000FF);
    }
}

// @Hack: Rendering to texture would require equal amount of code and doesn't feel
// like it would solve the problem any better?
internal void graph_save_to_file(Arena *arena, GFX_Window *window)
{
    String8 out = str8_alloc(arena, MAX_PATH);
    gfx_open_save_dialog(window, &out);
    
    HMM_Vec2 window_size = {0};
    gfx_window_get_rect(window, &window_size.X, &window_size.Y);
    
    GFX_Window *canvas = gfx_window_create(str8("Canvas"), (s32) window_size.X, (s32) window_size.Y);
    gfx_window_set_visible(canvas, 0);
    gfx_window_set_destroy_func(canvas, r_window_unequip);

    graph_fit_limits(canvas);
    
    r_window_equip(canvas);
    
    R_List list = {0};
    R_Ctx ctx = r_make_context(arena, &list);
        
    R_List font_list = {0};
    R_Ctx ui_ctx = r_make_context(arena, &font_list);
        
    r_frame_begin(canvas, 0xFFFFFFFF);
    
    r_graph(canvas, &ctx, &ui_ctx, 1);

    r_flush_batches(canvas, &list);
    r_flush_batches(canvas, &font_list);

    u8 *pixels = r_frame_end_get_backbuffer(canvas, arena);
    stbi_write_jpg((const char *) out.data, (s32) window_size.X, (s32) window_size.Y, 4, pixels, 100);
    
    gfx_window_destroy(canvas);
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
    
    state.font = font_init(arena, str8("./Inconsolata-Regular.ttf"), 16, 96);
    state.light_mode = 0;
    state.auto_scale = 1;
    state.show_slider_control = 0;
    
    state.camera.scale = 1.0f;
    state.camera.scale_step = 0.2f;
    state.camera.scale_max = 10.0f;
    state.camera.scale_min = 0.2f;
    state.camera.offset = { 0.0f, 0.0f };

    state.graph_step = { 1.0f, 1.0f };
    state.pixels_per_unit = { 80.0f, 80.0f };
    state.graph_origin = { WIDTH*.5f, HEIGHT*.5f };

    f32 xs[5] = { 0.0f, 1.0f, 2.5f, 5.0f, 10.0f };
    state.graph_data.xs = xs;
    
    f32 ys[5] = { 0.0f, 1.0f, 4.0f, 5.0f, 10.0f };
    state.graph_data.ys = ys;

    state.graph_data.size = 5;
    
    b32 should_quit = 0;
    f64 frame_prev = os_ticks_now();
    
    while (!should_quit) {
        OPTICK_FRAME("Main");
        
        arena_clear(frame_arena);
        
#ifndef NDEBUG
        er_accum_start();
#endif
        
        f64 frame_start = os_ticks_now();
        // f64 dt = (frame_start - frame_prev) / 1000.0;
        frame_prev = frame_start;
        
        HMM_Vec2 window_size = {0};
        gfx_window_get_rect(window, &window_size.X, &window_size.Y);
               
        GFX_Event_List event_list = gfx_process_input(frame_arena);
        for (GFX_Event *event = event_list.first; event != 0; event = event->next) {
            gfx_events_eat(&event_list);
            
            if (event->kind == GFX_EVENT_QUIT) {
                should_quit = 1;
                goto frame_end;
            }

            if (event->kind == GFX_EVENT_SIZE) {
                state.graph_origin = { event->window_size.X*.5f, event->window_size.Y*.5f };
            }
            
            switch (event->kind) {
                case GFX_EVENT_KEYDOWN: {
                    if (event->ctrl_held) {
                        if (event->character == 'S') {
                            graph_save_to_file(frame_arena, window);
                        }
                    } else {
                        if (event->character == VK_TAB) {
                            state.show_slider_control = !state.show_slider_control;
                        } else if (event->character == '0') {
                            state.camera.scale = 1.0f;
                            state.camera.offset = { 0.0f, 0.0f };
                        }
                    }
                } break;
                
                case GFX_EVENT_MBUTTONDOWN:
                case GFX_EVENT_LBUTTONDOWN: {
                    gfx_mouse_set_capture(window, 1);
                    state.mouse = event->mouse;
                    state.track_mouse = 1;
                } break;

                case GFX_EVENT_MBUTTONUP:
                case GFX_EVENT_LBUTTONUP: {
                    gfx_mouse_set_capture(window, 0);
                    state.track_mouse = 0;
                } break;

                case GFX_EVENT_MOUSEMOVE: {
                    if (state.track_mouse) {
                        state.camera.offset.X += (event->mouse.X - state.mouse.X)/state.camera.scale;
                        state.camera.offset.Y += (event->mouse.Y - state.mouse.Y)/state.camera.scale;
                    }
                    state.mouse = event->mouse;
                } break;

                case GFX_EVENT_MOUSEWHEEL: {
                    HMM_Vec2 before = {0};
                    HMM_Vec2 after = {0};
                    camera_to_screen(&state.camera, state.mouse.X, state.mouse.Y, &before.X, &before.Y);
                    {
                        if (event->mouse_wheel > 0.0f) {
                            state.camera.scale = MIN(state.camera.scale + state.camera.scale_step, state.camera.scale_max);
                        } else {
                            state.camera.scale = MAX(state.camera.scale - state.camera.scale_step, state.camera.scale_min);
                        }
                    }
                    camera_to_screen(&state.camera, state.mouse.X, state.mouse.Y, &after.X, &after.Y);

                    state.camera.offset.X += after.X - before.X;
                    state.camera.offset.Y += after.Y - before.Y;
                } break;
            }
        }

        graph_fit_limits(window);
        
        R_List list = {0};
        R_Ctx ctx = r_make_context(frame_arena, &list);
        
        R_List font_list = {0};
        R_Ctx ui_ctx = r_make_context(frame_arena, &font_list);
        
        r_frame_begin(window, 0x121212FF);

        // @Note: Rendering graph
        {            
            r_graph(window, &ctx, &ui_ctx, state.light_mode);
        }

        // @Note: Rendering and handling ui
        {
            const f32 controls_size = state.font.font_size*2.0f;
            const f32 padding = 10.0f;
            
            RectF32 controls = {
                0.0f, 0.0f,
                window_size.X, controls_size
            };
            r_rect(&ui_ctx, controls, 0x242424FF, 0.0f);

            String8 save = str8("Save");
            HMM_Vec2 text_pos = { padding, state.font.font_size*1.5f };
            font_r_text(&ui_ctx, &state.font, text_pos, 0xFFFFFFFF, save);

            if (state.show_slider_control) {
                RectF32 sliders = {
                    padding, controls_size + padding,
                    padding + 300.0f, controls_size + 200.0f
                };
                r_rect(&ui_ctx, sliders, 0x242424C8, 0.0f);
            }
        }
        
        r_flush_batches(window, &list);
        r_flush_batches(window, &font_list);

        r_frame_end(window);
        
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
    
    font_end(&state.font);
    gfx_window_destroy(window);
    
    r_backend_end();
    
    return 0;
}
