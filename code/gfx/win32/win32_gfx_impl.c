// @Note: All of the if (!error) {} in here are actually not that bad, if you're using an okay compile
// with some optimizations it generates reasonable code, so there's nothing to worry about (also globals are useful).
// I'm very sorry if your CS professors lied to you, they lied to me too.

global b32 win32_gfx_is_init = 0;
global Win32_Window win32_windows[GFX_MAX_WINDOW_COUNT] = {0};
global Win32_Window *win32_window_free = 0;

global Arena *win32_arena = 0;
global GFX_Event_List win32_event_list = {0};

// @Note: +1 and -1 here are for the case when window is a null-pointer
#define win32_window_from_opaque(w) (win32_windows + ((u64)(w) - 1))
#define win32_opaque_from_window(w) ((GFX_Window *)(((w) - win32_windows) + 1))
#define win32_window_is_valid(w) ((u64)(w) >= 1 && (u64)(w) < ARRAY_SIZE(win32_windows))

internal b32 gfx_is_init(void)
{
    return(win32_gfx_is_init);
}

internal LRESULT CALLBACK gfx_win32_window_proc(HWND handle, UINT msg, WPARAM wParam, LPARAM lParam)
{
    GFX_Window *window = gfx_win32_opaque_from_handle(handle);
    
    switch (msg) {
        case WM_CLOSE: {
            gfx_events_push(GFX_EVENT_QUIT, window);
        } break;
        
        case WM_GETMINMAXINFO: {
            MINMAXINFO *info = (MINMAXINFO *) lParam;
            info->ptMinTrackSize.x = GFX_WIN32_MIN_WIDTH;
            info->ptMinTrackSize.y = GFX_WIN32_MIN_HEIGHT;
        } break;
        
        case WM_SIZE: {
            GFX_Window *gfx_window = gfx_win32_opaque_from_handle(handle);
            Win32_Window *win32_window = win32_window_from_opaque(gfx_window);
            
            if (win32_window->render) {
                PAINTSTRUCT ps = {0};
                BeginPaint(handle, &ps);
                win32_window->render(gfx_window, win32_window->render_data);
                EndPaint(handle, &ps);
            }
        } break;
        
        case WM_ENTERSIZEMOVE: {
            Win32_Window *w = gfx_win32_window_from_handle(handle);
            w->resizing = 1;
        } break;
        
        case WM_EXITSIZEMOVE: {
            Win32_Window *w = gfx_win32_window_from_handle(handle);
            w->resizing = 0;
        } break;
        
        case WM_SETCURSOR: {
            Win32_Window *w = gfx_win32_window_from_handle(handle);
            if (!w->resizing) {
                SetClassLongPtr(handle, GCLP_HCURSOR, (LONG_PTR) w->cursor);
            }
        } break;
        
        case WM_KEYDOWN: {
            // @ToDo: Actaully translate the keycodes comming in to something useful
            GFX_Event *event = gfx_events_push(GFX_EVENT_KEYDOWN, window);
            event->ctrl_held = GetAsyncKeyState(VK_CONTROL) & ( 1 << 16);
            event->character = wParam;
        } break;
        
        // @ToDo: Think about memory usage here, is it okay to use win32_arena?
        // do these things need to live this long? (kinda since we have names as String8?)
        case WM_DROPFILES: {
            GFX_Event *event = gfx_events_push(GFX_EVENT_DROPFILES, window);
            
            HDROP hdrop = (HDROP) wParam;
            u32 count = DragQueryFile(hdrop, 0xFFFFFFFF, 0, 0);
            
            GFX_Drop_Files drop_files = {0};
            drop_files.count = (usize) count;
            drop_files.files = arena_push_array(win32_arena, GFX_Drop_Files_Node, drop_files.count);
            
            for (u32 i = 0; i < drop_files.count; ++i) {
                u32 size = DragQueryFile(hdrop, i, 0, 0) + 1;
                drop_files.files[i].name.size = (usize) size;
                drop_files.files[i].name.data = arena_push_array(win32_arena, u8, (usize) size);
                DragQueryFile(hdrop, i, (LPSTR) drop_files.files[i].name.data, (UINT) size);
            }
            
            event->drop_files = drop_files;
            DragFinish(hdrop);
        } break;

        case WM_MBUTTONUP:
        case WM_LBUTTONUP: {
            // @Note: wParam here, describes what key was down when we release mouse
            // could be useful with ctrl+click.
            GFX_Event_Kind kind = GFX_EVENT_NONE;
            switch (msg) {
                case WM_MBUTTONUP: kind = GFX_EVENT_MBUTTONUP; break;
                case WM_LBUTTONUP: kind = GFX_EVENT_LBUTTONUP; break;
            }
            Assert(kind != GFX_EVENT_NONE);
            
            GFX_Event *event = gfx_events_push(kind, window);
            event->mouse.X = (f32) GET_X_LPARAM(lParam);
            event->mouse.Y = (f32) GET_Y_LPARAM(lParam);
        } break;

        case WM_MBUTTONDOWN:
        case WM_LBUTTONDOWN: {
            GFX_Event_Kind kind = GFX_EVENT_NONE;
            switch (msg) {
                case WM_MBUTTONDOWN: kind = GFX_EVENT_MBUTTONDOWN; break;
                case WM_LBUTTONDOWN: kind = GFX_EVENT_LBUTTONDOWN; break;
            }
            Assert(kind != GFX_EVENT_NONE);
            
            GFX_Event *event = gfx_events_push(kind, window);
            event->mouse.X = (f32) GET_X_LPARAM(lParam);
            event->mouse.Y = (f32) GET_Y_LPARAM(lParam);
        } break;

        case WM_MOUSEMOVE: {
            GFX_Event *event = gfx_events_push(GFX_EVENT_MOUSEMOVE, window);
            event->mouse.X = (f32) GET_X_LPARAM(lParam);
            event->mouse.Y = (f32) GET_Y_LPARAM(lParam);
        } break;

        case WM_MOUSEWHEEL: {
            GFX_Event *event = gfx_events_push(GFX_EVENT_MOUSEWHEEL, window);
            event->mouse_wheel = GET_WHEEL_DELTA_WPARAM(wParam);
            gfx_mouse_get_screen_pos((s32 *) &event->mouse.X, (s32 *) &event->mouse.Y);
        } break;
    }
    
    return(DefWindowProc(handle, msg, wParam, lParam));
}

internal b32 gfx_init(void)
{
    b32 error = 0;
    if (win32_gfx_is_init) {
        er_push(str8("GFX layer is already initialized"));
        error = 1;
    }
    
    if (!error) {        
        WNDCLASSEX window_class = {0};
        window_class.cbSize = sizeof(WNDCLASSEX);
        window_class.style = CS_OWNDC|CS_HREDRAW|CS_VREDRAW;
        window_class.lpfnWndProc = gfx_win32_window_proc;
        window_class.hInstance = os_win32_get_instance();
        window_class.lpszClassName = GFX_WIN32_WINDOW_CLASS_NAME;
        window_class.hCursor = LoadCursor(0, IDC_ARROW);
        
        ATOM atom = RegisterClassEx(&window_class);
        if (atom == 0) {
            er_push(str8("Failed to register class"));
            error = 1;
        }
    }
    
    if (!error) {
        win32_window_free = win32_windows;
        Win32_Window *last = win32_windows + ARRAY_SIZE(win32_windows) - 1;
        for (Win32_Window *slot = win32_window_free; slot < last; ++slot) {
            slot->next = (slot + 1);
        }
    }
    
    if (!error) {
        win32_gfx_is_init = 1;
    }
    
    b32 result = !error;
    return(result);
}

internal GFX_Window *gfx_window_create(String8 title, s32 width, s32 height)
{
    b32 error = 0;
    if (!gfx_is_init()) {
        er_push(str8("GFX layer not initialized"));
        error = 1;
    }
    
    Win32_Window *slot = 0;
    if (!error) {
        slot = win32_window_free;
        if (slot == 0) {
            er_push(str8("Run out of free windows to allocate from"));
            error = 1;
        }
    }
    
    HWND handle = 0;
    if (!error) {
        DWORD win32_window_style = WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX;
        HINSTANCE instance = os_win32_get_instance();
        
        RECT client_area = {0};
        client_area.top = 0;
        client_area.left = 0;
        client_area.bottom = height;
        client_area.right = width;
        AdjustWindowRectEx(&client_area, win32_window_style, 0, 0);
        
        width = client_area.right - client_area.left;
        height = client_area.bottom - client_area.top;
        
        handle = CreateWindowEx(0, GFX_WIN32_WINDOW_CLASS_NAME, (LPCSTR) title.data,
                                win32_window_style, CW_USEDEFAULT, CW_USEDEFAULT,
                                width, height, 0, 0, instance, 0);
        
        if (handle == 0) {
            er_push(str8("Failed to create a valid window"));
            error = 1;
        }
    }
    
    // @Note: Cleanup
    if (error) {
        if (handle) {
            DestroyWindow(handle);
        }
    }
    
    GFX_Window *result = 0;
    if (!error) {
        FREE_LIST_ALLOC(win32_window_free);
        slot->handle = handle;
        slot->cursor = LoadCursor(0, IDC_ARROW);
        result = win32_opaque_from_window(slot);
    }
    
    return(result);
}

internal void gfx_window_destroy(GFX_Window *window)
{
    if (!gfx_is_init()) {
        er_push(str8("GFX layer not initialized"));
    } else {
        if (gfx_window_is_valid(window)) {
            Win32_Window *w = win32_window_from_opaque(window);
            
            if (w->destroy) {
                w->destroy(window);
            }
            
            DestroyWindow(w->handle);
            FREE_LIST_RELEASE(win32_window_free, w);
            MemoryZero(w, sizeof(Win32_Window));
        }
    }
}

internal b32 gfx_window_set_title(GFX_Window *window, String8 title)
{
    b32 result = 0;
    if (!gfx_is_init()) {
        er_push(str8("GFX layer not initialized"));
    } else {
        if (gfx_window_is_valid(window)) {
            Win32_Window *w = win32_window_from_opaque(window);
            if (SetWindowText(w->handle, (LPCSTR) title.data)) {
                result = 1;
            }        
        }
    }
    
    return(result);
}

internal b32 gfx_window_set_visible(GFX_Window *window, b32 visible)
{
    b32 result = 0;
    if (!gfx_is_init()) {
        er_push(str8("GFX layer not initialized"));
    } else {
        if (gfx_window_is_valid(window)) {
            int cmd = SW_HIDE;
            if (visible) {
                cmd = SW_SHOW;
            }
            
            Win32_Window *w = win32_window_from_opaque(window);
            ShowWindow(w->handle, cmd);
            result = 1;
        }
    }
    
    return(result);
}

internal b32 gfx_window_set_resizable(GFX_Window *window, b32 resizable)
{
    b32 result = 0;
    if (!gfx_is_init()) {
        er_push(str8("GFX layer not initialized"));
    } else {
        if (gfx_window_is_valid(window)) {
            Win32_Window *w = win32_window_from_opaque(window);
            
            DWORD resizable_flags = WS_MAXIMIZEBOX|WS_SIZEBOX;
            DWORD old_style = GetWindowLong(w->handle, GWL_STYLE);
            DWORD new_style = 0;
            
            if (resizable) {
                new_style = old_style | resizable_flags;
            } else {
                new_style = old_style & (~resizable_flags);
            }
            
            if (SetWindowLong(w->handle, GWL_STYLE, new_style)) {
                w->resizable = 1;
                result = 1;
            }
        }
    }
    
    return(result);
}


internal b32 gfx_window_set_drop_files(GFX_Window *window, b32 drop_files)
{
    b32 result = 0;
    if (!gfx_is_init()) {
        er_push(str8("GFX layer not initialized"));
    } else {
        if (gfx_window_is_valid(window)) {
            Win32_Window *w = win32_window_from_opaque(window);
            
            DWORD drop_files_flags = WS_EX_ACCEPTFILES;
            DWORD old_style = GetWindowLong(w->handle, GWL_EXSTYLE);
            DWORD new_style = 0;
            
            if (drop_files) {
                new_style = old_style | drop_files_flags;
            } else {
                new_style = old_style & (~drop_files_flags);
            }
            
            if (SetWindowLong(w->handle, GWL_EXSTYLE, new_style)) {
                w->drop_files = 1;
                result = 1;
            }
        }
    }
    
    return(result);
}

internal b32 gfx_window_get_resizable(GFX_Window *window)
{
    b32 result = 0;
    if (!gfx_is_init()) {
        er_push(str8("GFX layer not initialized"));
    } else {
        if (gfx_window_is_valid(window)) {
            Win32_Window *w = win32_window_from_opaque(window);
            result = w->resizable;
        }
    }
    
    return(result);
}

internal GFX_Event *gfx_events_push(GFX_Event_Kind kind, GFX_Window *window)
{
    GFX_Event *event = arena_push_array(win32_arena, GFX_Event, 1);
    
    if (event != 0) {
        event->kind = kind;
        event->window = window;
        
        SLLQueuePush(win32_event_list.first, win32_event_list.last, event);
        win32_event_list.count += 1;
    }
    
    return(event);
}

internal void gfx_events_eat(GFX_Event_List *list)
{
    SLLQueuePop(list->first, list->last);
    list->count -= 1;
}

internal GFX_Event_List gfx_process_input(Arena *arena)
{
    win32_arena = arena;
    MemoryZero(&win32_event_list, sizeof(GFX_Event_List));
    
    MSG msg = {0};
    while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    return(win32_event_list);
}

internal b32 gfx_window_is_valid(GFX_Window *window)
{
    b32 result = 0;
    if (!gfx_is_init()) {
        er_push(str8("GFX layer not initialized"));
    } else {
        if (win32_window_is_valid(window)) {
            Win32_Window *w = win32_window_from_opaque(window);
            if (w->handle) {
                result = 1;
            }
        }
    }
    
    return(result);
}

internal b32 gfx_window_set_render_func(GFX_Window *window, gfx_render_func *render, void *data)
{
    b32 result = 0;
    if (!gfx_is_init()) {
        er_push(str8("GFX layer not initialized"));
    } else {
        if (win32_window_is_valid(window)) {
            Win32_Window *w = win32_window_from_opaque(window);
            w->render = render;
            w->render_data = data;
            result = 1;
        }
    }
    
    return(result);
}

internal b32 gfx_window_set_destroy_func(GFX_Window *window, gfx_destroy_func *destroy)
{
    b32 result = 0;
    if (!gfx_is_init()) {
        er_push(str8("GFX layer not initialized"));
    } else {
        if (win32_window_is_valid(window)) {
            Win32_Window *w = win32_window_from_opaque(window);
            w->destroy = destroy;
            result = 1;
        }
    }
    
    return(result);
}

internal b32 gfx_window_get_rect(GFX_Window *window, f32 *width, f32 *height)
{
    b32 result = 0;
    if (!gfx_is_init()) {
        er_push(str8("GFX layer not initialized"));
    } else {
        if (win32_window_is_valid(window)) {
            Win32_Window *w = win32_window_from_opaque(window);
            
            RECT win32_rect = {0};
            GetClientRect(w->handle, &win32_rect);
            
            *width = (f32) (win32_rect.right - win32_rect.left);
            *height = (f32) (win32_rect.bottom - win32_rect.top);
            
            result = 1;
        }
    }
    
    return(result);
}

internal b32 gfx_mouse_get_screen_pos(s32 *mx, s32 *my)
{
    b32 result = 0;
    if (!gfx_is_init()) {
        er_push(str8("GFX layer not initialized"));
    } else {
        POINT mouse = {0};
        if (GetCursorPos(&mouse)) {
            *mx = mouse.x;
            *my = mouse.y;
            result = 1;
        }
    }
    
    return(result);
}

internal b32 gfx_mouse_get_relative_pos(GFX_Window *window, s32 *mx, s32 *my)
{
    b32 result = 0;
    if (!gfx_is_init()) {
        er_push(str8("GFX layer not initialized"));
    } else {
        if (win32_window_is_valid(window)) {
            Win32_Window *w = win32_window_from_opaque(window);
            POINT mouse = {0};
            
            b32 get_mouse = gfx_mouse_get_screen_pos((s32 *) &mouse.x, (s32 *) &mouse.y);
            b32 get_client = ScreenToClient(w->handle, &mouse);
            
            if (get_mouse && get_client) {
                *mx = mouse.x;
                *my = mouse.y;
                result = 1;
            }
        }
    }
    
    return(result);
}

internal void gfx_mouse_set_cursor(GFX_Window *window, GFX_Cursor_Kind kind)
{
    if (!gfx_is_init()) {
        er_push(str8("GFX layer not initialized"));
    } else {
        if (win32_window_is_valid(window)) {
            Win32_Window *w = win32_window_from_opaque(window);
            HCURSOR cursor = w->cursor;
        
            switch (kind) {
                case GFX_CURSOR_ARROW: {
                    cursor = LoadCursor(0, IDC_ARROW);
                } break;
            
                case GFX_CURSOR_HAND: {
                    cursor = LoadCursor(0, IDC_HAND);
                } break;
            
                case GFX_CURSOR_HSIZE: {
                    cursor = LoadCursor(0, IDC_SIZEWE);
                } break;
            }
        
            if (w->cursor != cursor) {
                w->cursor = cursor;
                SetCursor(w->cursor);
            }
        }
    }
}

internal void gfx_mouse_set_capture(GFX_Window *window, b32 capture)
{
    if (!gfx_is_init()) {
        er_push(str8("GFX layer not initialized"));
    } else {
        if (win32_window_is_valid(window)) {
            Win32_Window *w = win32_window_from_opaque(window);
            
            if (capture) {
                SetCapture(w->handle);
            } else {
                ReleaseCapture();
            }
        }
    }
}

internal void gfx_error_display(GFX_Window *window, String8 text, String8 caption)
{
    if (win32_window_is_valid(window)) {
        Win32_Window *w = win32_window_from_opaque(window);
        MessageBox(w->handle, (LPCSTR) text.data, (LPCSTR) caption.data, MB_OK | MB_ICONEXCLAMATION);
    }
}

internal b32 gfx_open_save_dialog(GFX_Window *window, String8 *out)
{
    b32 result = 0;
    if (!gfx_is_init()) {
        er_push(str8("GFX layer not initialized"));
    } else {
        if (win32_window_is_valid(window)) {
            Win32_Window *w = win32_window_from_opaque(window);
            
            OPENFILENAME ofn = {0};
            MemoryZero(&ofn, sizeof(OPENFILENAME));
            MemoryZero(out->data, out->size);
            
            ofn.lStructSize = sizeof(OPENFILENAME);
            ofn.hwndOwner = w->handle;
            ofn.lpstrFilter = "Image file (*.jpg;*jpeg;*jpe;*jfif)\0*.jpg;*.jpeg;*.jpe;*.jfif\0\0";
            ofn.lpstrTitle = "Select a file";
            ofn.lpstrFile = (LPSTR) out->data;
            ofn.nMaxFile = (DWORD) out->size;
            ofn.lpstrDefExt = "jpg";
            ofn.Flags = OFN_EXPLORER | OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY;
            
            result = GetSaveFileName(&ofn);
        }
    }

    return(result);
}
    
//
// @Note: Windows specific
//

internal GFX_Window *gfx_win32_opaque_from_handle(HWND handle)
{
    GFX_Window *result = 0;
    if (!gfx_is_init()) {
        er_push(str8("GFX layer not initialized"));
    } else {
        if (handle != 0) {
            // @Note: Hash-map might cause too much overhead for now
            for (usize i = 0; i < ARRAY_SIZE(win32_windows); ++i) {
                if (win32_windows[i].handle == handle) {
                    result = win32_opaque_from_window(&win32_windows[i]);
                    break;
                }
            }
        }
    }
    
    return(result);
}

internal Win32_Window *gfx_win32_window_from_handle(HWND handle)
{
    Win32_Window *result = 0;
    GFX_Window *w = gfx_win32_opaque_from_handle(handle);
    
    if (w) {
        result = win32_window_from_opaque(w);
    }
    
    return(result);
}

internal inline GFX_Window *gfx_win32_opaque_from_window(Win32_Window *window)
{
    return(win32_opaque_from_window(window));
}

internal inline Win32_Window *gfx_win32_window_from_opaque(GFX_Window *handle)
{
    return(win32_window_from_opaque(handle));
}
