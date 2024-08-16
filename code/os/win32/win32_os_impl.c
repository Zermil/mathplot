global u64 win32_ticks_per_sec = 1;
global HINSTANCE win32_instance =  0;

global b32 win32_os_is_init = 0;

internal b32 os_main_is_init(void)
{
    return(win32_os_is_init);
}

internal b32 os_main_init(void)
{
    b32 error = 0;
    if (win32_os_is_init) {
        er_push(str8("OS layer already initialized"));
        error = 1;
    }
    
    if (!error) {
        LARGE_INTEGER freq = {0};
        if (QueryPerformanceFrequency(&freq)) {
            win32_ticks_per_sec = (u64) freq.QuadPart;
        }
        
        win32_instance = GetModuleHandle(0);
    }
    
    if (!error) {
        win32_os_is_init = 1;
    }
    
    b32 result = !error;
    return(result);
}

internal void os_wait(f64 ms)
{
    if (!os_main_is_init()) {
        er_push(str8("OS layer was not initialized"));
    } else {
        f64 prev = os_ticks_now();
        f64 now = prev;
        while ((now - prev) < ms) {
            now = os_ticks_now();
        }
    }
}

internal f64 os_ticks_now(void)
{
    f64 result = 0.0;
    if (!os_main_is_init()) {
        er_push(str8("OS layer was not initialized"));
    } else {
        LARGE_INTEGER time = {0};
        if (QueryPerformanceCounter(&time)) {
            result = (f64) time.QuadPart;
            result *= 1000.0;
            result /= win32_ticks_per_sec;
        }
    }
    
    return(result);
}

internal HINSTANCE os_win32_get_instance(void)
{
    if (win32_instance == 0) {
        win32_instance = GetModuleHandle(0);
    }
    
    return(win32_instance);
}

internal void *os_memory_reserve(usize size)
{
    void *result = VirtualAlloc(0, size, MEM_RESERVE, PAGE_READWRITE);
    return(result);
}

internal void os_memory_release(void *ptr)
{
    VirtualFree(ptr, 0, MEM_RELEASE);
}

internal b32 os_memory_commit(void *ptr, usize size)
{
    b32 result = (VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE) != 0);
    return(result);
}

internal void os_memory_decommit(void *ptr, usize size)
{
    VirtualFree(ptr, size, MEM_DECOMMIT);
}

internal usize os_get_page_size(void)
{
    SYSTEM_INFO sysinfo = {0};
    GetSystemInfo(&sysinfo);
    return(sysinfo.dwPageSize);
}

internal String8 os_file_read(Arena *arena, String8 file)
{
    String8 result = {0};
    HANDLE file_handle = CreateFile((LPCSTR) file.data, 
                                    GENERIC_READ, 0, 0,
                                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    
    if (file_handle != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER win32_size = {0};
        
        GetFileSizeEx(file_handle, &win32_size);
        u64 file_size = win32_size.QuadPart;
        
        Arena_Temp save_point = arena_temp_begin(arena);
        u8 *buffer = arena_push_array(arena, u8, file_size);
        
        // @Note: we can get a size bigger than 32 bits, so we need to acount for
        // that and read appropriately
        u8 *start = buffer;
        u8 *end = buffer + file_size;
        b32 success = 1;
        while (start < end) {
            u64 total_to_read = (u64) (end - start);
            DWORD left_to_read = (DWORD) total_to_read;
            if (total_to_read > 0xFFFFFFFF) {
                left_to_read = 0xFFFFFFFF;
            }
            
            DWORD actual_read = 0;
            if (!ReadFile(file_handle, buffer, left_to_read, &actual_read, 0)) {
                success = 0;
                break;
            }
            
            start += actual_read;
        }
        
        if (success) {
            result.data = buffer;
            result.size = file_size;
        } else {
            arena_temp_end(&save_point);
        }
        
        CloseHandle(file_handle);
    }
    
    return(result);
}

internal void os_exit_process(u32 code)
{
    ExitProcess(code);
}
