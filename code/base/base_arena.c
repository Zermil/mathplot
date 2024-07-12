static_assert(sizeof(Arena) <= ARENA_HEADER_SIZE, "Size of arena is bigger than arena\'s header");
static_assert(ARENA_HEADER_SIZE <= ARENA_DEFAULT_COMMIT, "ARENA_HEADER_SIZE has to be smaller than ARENA_DEFAULT_COMMIT");
static_assert(IS_POW2(ARENA_DEFAULT_COMMIT), "ARENA_DEFAULT_COMMIT is not a power of 2");

internal Arena *arena_make_sized(usize size, b32 growing)
{
    OPTICK_EVENT();
    
    Arena *result = 0;
    if (size >= ARENA_DEFAULT_COMMIT) {
        usize page_size = os_get_page_size();
        size = ALIGN_POW2(size, page_size);
        void *mem = os_memory_reserve(size);
        
        if (os_memory_commit(mem, ARENA_DEFAULT_COMMIT)) {
            ASAN_MEM_POISON(mem, size);
            ASAN_MEM_UNPOISON(mem, ARENA_HEADER_SIZE);
            
            result = (Arena *) mem;
            result->current = result;
            result->prev = 0;
            result->chunk_cap = size;
            result->chunk_pos = ARENA_HEADER_SIZE;
            result->commit_pos = ARENA_DEFAULT_COMMIT;
            result->growing = growing;
            result->align = sizeof(void *);
            result->base_pos = 0;
        } else {
            os_memory_release(mem);
        }
    }
    
    Assert(result != 0);
    return(result);
}

internal Arena *arena_make(void)
{
    Arena *result = arena_make_sized(ARENA_DEFAULT_SIZE, 1);
    return(result);
}

internal void arena_release(Arena *arena)
{
    OPTICK_EVENT();
    
    Arena *current = arena->current;
    while (current != 0) {
        Arena *prev = current->prev;
        ASAN_MEM_POISON(current, current->chunk_cap);
        os_memory_release(current);
        current = prev;
    }
}

internal void *arena_push_no_zero(Arena *arena, usize size)
{
    OPTICK_EVENT();
    
    void *result = 0;
    
    Arena *current = arena->current;
    
    if (current->growing) {
        usize next_chunk = ALIGN_POW2(current->chunk_pos, current->align);
        usize next_chunk_end = next_chunk + size;
        
        if (next_chunk_end > current->chunk_cap) {
            usize new_chunk_size = size + ARENA_HEADER_SIZE;
            if (new_chunk_size < ARENA_DEFAULT_COMMIT) {
                new_chunk_size = ALIGN_POW2(new_chunk_size, ARENA_DEFAULT_COMMIT);
            }
            
            Arena *new_chunk = arena_make_sized(new_chunk_size, 1);
            Assert(new_chunk);
            
            new_chunk->base_pos = current->base_pos + current->chunk_cap;
            new_chunk->prev = current;
            
            current = arena->current = new_chunk;
        }
    }
    
    {
        // @Note: These variables are repeated here, because
        // we _might_ allocate a new chunk in the code above.
        usize next_chunk_start = ALIGN_POW2(current->chunk_pos, current->align);
        usize next_chunk_end = next_chunk_start + size;
        
        if (next_chunk_end <= current->chunk_cap) {
            if (next_chunk_end > current->commit_pos) {
                usize next_commit_pos = ALIGN_POW2(next_chunk_end, ARENA_DEFAULT_COMMIT);
                
                // @Note: Ensure that we won't _overcommit_, also means that we can't align.
                usize next_commit_min = MIN(next_commit_pos, current->chunk_cap);
                
                usize next_commit_size = next_commit_min - current->commit_pos;
                if (os_memory_commit((u8 *) current + current->commit_pos, next_commit_size)) {
                    current->commit_pos = next_commit_min;
                }
            }
            
            if (next_chunk_end <= current->commit_pos) {
                result = (u8 *) current + next_chunk_start;
                current->chunk_pos = next_chunk_end;
                ASAN_MEM_UNPOISON(result, size);
            }
        }
    }
    
    return(result);
}

internal void *arena_push(Arena *arena, usize size)
{
    void *result = arena_push_no_zero(arena, size);
    MemoryZero(result, size);
    return(result);
}

internal void arena_pop_to(Arena *arena, usize pos)
{
    OPTICK_EVENT();
    
    usize pos_clamp = MAX(pos, ARENA_HEADER_SIZE);
    
    Arena *current = arena->current;
    while (current->base_pos >= pos_clamp) {
        Arena *prev = current->prev;
        os_memory_release(current);
        current = prev;
    }
    
    Assert(current);
    
    arena->current = current;
    usize rel_chunk_pos = pos_clamp - current->base_pos;
    
    {
        // @Note: This is very serious, if this assert hits, this means that our 'rel_chunk_pos' resides in uncommited/poisoned memory.
        // we _could_ also go a different route and do some if(s) and checks or similar approaches to
        // ensure we pop _more_ stuff, but, as a consequence, everything's correct when it comes to memory access.
        Assert(rel_chunk_pos <= current->chunk_pos);
    }
    
    ASAN_MEM_POISON((u8 *) current + rel_chunk_pos, current->chunk_pos - rel_chunk_pos);
    current->chunk_pos = rel_chunk_pos;
}

internal usize arena_pos(Arena *arena)
{
    Arena *current = arena->current;
    usize result = current->base_pos + current->chunk_pos;
    return(result);
}

internal void arena_put_back(Arena *arena, usize size)
{
    usize curr_pos = arena_pos(arena);
    usize new_pos = curr_pos;
    
    if (size <= curr_pos) {
        new_pos = curr_pos - size;
    }
    
    arena_pop_to(arena, new_pos);
}

internal void arena_clear(Arena *arena)
{
    arena_pop_to(arena, 0);
}

internal Arena_Temp arena_temp_begin(Arena *arena)
{
    Arena_Temp result = {0};
    result.pos = arena_pos(arena);
    result.arena = arena;
    return(result);
}

internal void arena_temp_end(Arena_Temp *temp)
{
    arena_pop_to(temp->arena, temp->pos);
}
