#ifndef OS_H
#define OS_H

internal b32 os_main_is_init(void);
internal b32 os_main_init(void);
internal void os_wait(f64 ms);
internal f64 os_ticks_now(void);

internal void *os_memory_reserve(usize size);
internal void os_memory_release(void *ptr);
internal b32 os_memory_commit(void *ptr, usize size);
internal void os_memory_decommit(void *ptr, usize size);
internal usize os_get_page_size(void);

internal String8 os_file_read(Arena *arena, String8 file);

internal void os_exit_process(u32 code);

#endif // OS_H
