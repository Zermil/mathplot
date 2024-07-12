thread_var ER_Thread_Local *er_thread_local = 0;

internal void er_accum_start(void)
{
    ER_Thread_Local *er = er_thread_local;
    if (er == 0) {
        Arena *arena = arena_make_sized(KB(64), 0);
        er = er_thread_local = arena_push_array(arena, ER_Thread_Local, 1);
        er->arena = arena;
    }
    
    usize pos = arena_pos(er->arena);
    ER_Node *node = arena_push_array(er->arena, ER_Node, 1);
    if (node != 0) {
        node->arena_pos = pos;
        SLLStackPush(er->stack, node);
    }
}

internal void er_push(String8 error)
{
    ER_Thread_Local *er = er_thread_local;
    if (er != 0) {
        ER_Node *top = er->stack;
        // @Note: We're only saving the first error
        if (top != 0 && top->error.size == 0) {
            top->error = str8_push_copy(er->arena, error);
        }
    }
}

internal String8 er_accum_end(Arena *arena)
{
    String8 result = {0};
    ER_Thread_Local *er = er_thread_local;
    if (er != 0) {
        ER_Node *top = er->stack;
        if (top != 0) {
            result = str8_push_copy(arena, top->error);
            SLLStackPop(er->stack);
            arena_pop_to(er->arena, top->arena_pos);
        }
    }
    
    return(result);
}
