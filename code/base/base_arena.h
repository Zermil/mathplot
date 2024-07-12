// @Note: For more information look here, amazing article!
// https://www.rfleury.com/p/untangling-lifetimes-the-arena-allocator

#ifndef BASE_ARENA_H
#define BASE_ARENA_H

#ifndef ARENA_HEADER_SIZE
# define ARENA_HEADER_SIZE KB(4)
#endif

#ifndef ARENA_DEFAULT_SIZE
# define ARENA_DEFAULT_SIZE MB(16)
#endif

#ifndef ARENA_DEFAULT_COMMIT
# define ARENA_DEFAULT_COMMIT KB(64)
#endif

typedef struct Arena {
    struct Arena *current;
    struct Arena *prev;
    
    usize chunk_cap;
    usize chunk_pos;
    
    usize commit_pos;
    
    b32 growing; // @Note: We want to allow arenas that are static i.e. not 'growing'
    usize align;
    usize base_pos; // @Note: Helper for when we 'pop' arenas
} Arena;

typedef struct {
    Arena *arena;
    usize pos;
} Arena_Temp;

internal Arena *arena_make_sized(usize size, b32 growing);
internal Arena *arena_make(void);
internal void arena_release(Arena *arena);

internal void *arena_push_no_zero(Arena *arena, usize size);
internal void *arena_push(Arena *arena, usize size);
internal void arena_pop_to(Arena *arena, usize pos);

internal usize arena_pos(Arena *arena);
internal void arena_put_back(Arena *arena, usize size);
internal void arena_clear(Arena *arena);

internal Arena_Temp arena_temp_begin(Arena *arena);
internal void arena_temp_end(Arena_Temp *temp);

#define arena_push_array(a, t, s) ((t *) (arena_push((a), sizeof(t)*(s))))

#endif // BASE_ARENA_H
