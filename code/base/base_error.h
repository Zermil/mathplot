#ifndef ERROR_H
#define ERROR_H

typedef struct ER_Node {
    struct ER_Node *next;
    
    usize arena_pos;
    String8 error;
} ER_Node;

typedef struct {
    Arena *arena;
    ER_Node *stack;
} ER_Thread_Local;

internal void er_accum_start(void);
internal void er_push(String8 error);
internal String8 er_accum_end(Arena *arena);

#endif // ERROR_H
