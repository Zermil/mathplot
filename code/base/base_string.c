internal usize str8_cstr_size(const char *cstr)
{
    u8 *ptr = (u8 *) cstr;
    for (;*ptr != 0; ++ptr);
    usize result = (ptr - ((u8 *) cstr));
    return(result);
}

internal String8 str8_make(u8  *data, usize size)
{
    String8 result = {0};
    result.data = data;
    result.size = size;
    return(result);
}

internal String8 str8_push_cstr(Arena *arena, const char *cstr)
{
    String8 result = {0};
    result.size = str8_cstr_size(cstr);
    result.data = arena_push_array(arena, u8, result.size + 1);
    MemoryCopy(result.data, cstr, result.size);
    return(result);
}

internal String8 str8_from_cstr(const char *cstr)
{
    u8 *data = (u8 *) cstr; 
    usize size = str8_cstr_size(cstr);
    String8 result = str8_make(data, size);
    return(result);
}

internal String8 str8_push_copy(Arena *arena, String8 str)
{
    String8 result = {0};
    result.data = arena_push_array(arena, u8, str.size + 1);
    result.size = str.size;
    MemoryCopy(result.data, str.data, str.size);
    result.data[result.size] = 0;
    return(result);
}