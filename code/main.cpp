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
    
    b32 should_quit = 0;
    f64 frame_prev = os_ticks_now();
    
    while (!should_quit) {
        OPTICK_FRAME("Main");
        
        arena_clear(frame_arena);
        
#ifndef NDEBUG
        er_accum_start();
#endif
        
        f64 frame_start = os_ticks_now();
        // f64 delta = (frame_start - frame_prev)/1000.0f;
        frame_prev = frame_start;
        
        GFX_Event_List event_list = gfx_process_input(frame_arena);
        for (GFX_Event *event = event_list.first; event != 0; event = event->next) {
            gfx_events_eat(&event_list);
            
            if (event->kind == GFX_EVENT_QUIT) {
                should_quit = 1;
                goto frame_end;
            }
        }
        
        R_List list = {0};
        R_Ctx ctx = r_make_context(frame_arena, &list);
        r_frame_begin(window);

        r_flush_batches(window, &list);
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
    
    gfx_window_destroy(window);
    r_backend_end();
    
    return 0;
}
