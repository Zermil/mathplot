#ifndef WIN32_GFX_IMPL_H
#define WIN32_GFX_IMPL_H

#ifndef GFX_WIN32_MIN_WIDTH
# define GFX_WIN32_MIN_WIDTH 800
#endif

#ifndef GFX_WIN32_MIN_HEIGHT
# define GFX_WIN32_MIN_HEIGHT 600
#endif

typedef struct Win32_Window {
    struct Win32_Window *next;
    
    HWND handle;
    b32 resizable;
    b32 drop_files;
    HCURSOR cursor;
    
    b32 resizing;
    
    gfx_render_func *render;
    void *render_data;
    
    gfx_destroy_func *destroy;
} Win32_Window;

#define GFX_WIN32_WINDOW_CLASS_NAME "gfx_win32_window_class"

internal LRESULT CALLBACK gfx_win32_window_proc(HWND handle, UINT msg, WPARAM wParam, LPARAM lParam);
internal inline GFX_Window *gfx_win32_opaque_from_window(Win32_Window *window);
internal inline Win32_Window *gfx_win32_window_from_opaque(GFX_Window *handle);
internal GFX_Window *gfx_win32_opaque_from_handle(HWND handle);
internal Win32_Window *gfx_win32_window_from_handle(HWND handle);

#endif // WIN32_GFX_IMPL_H