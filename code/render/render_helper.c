internal R_Ctx r_make_context(Arena *arena, R_List *list)
{
    R_Ctx result = {0};
    result.arena = arena;
    result.list = list;
    return(result);
}

internal void r_new_batch(Arena *arena, R_List *list)
{
    R_Quad_Batch *batch = arena_push_array(arena, R_Quad_Batch, 1);
    SLLQueuePush(list->first, list->last, batch);
    list->count += 1;
}

internal void r_prep_batch(Arena *arena, R_List *list, R_Texture2D *texture)
{
    if (list->first == 0) {
        r_new_batch(arena, list);
    } else {
        if (list->last->texture != texture) {
            r_new_batch(arena, list);
        }
    }
    
    list->last->texture = texture;
}

internal void r_push_quad(Arena *arena, R_Quad_Batch *batch, R_Quad *quad)
{
    if (quad->pos.x0 > quad->pos.x1) {
        SWAP(quad->pos.x0, quad->pos.x1, f32);
    }
    
    if (quad->pos.y0 > quad->pos.y1) {
        SWAP(quad->pos.y0, quad->pos.y1, f32);
    }
    
    if (batch->first == 0) {
        R_Quad_Node *node = arena_push_array(arena, R_Quad_Node, 1);
        SLLQueuePush(batch->first, batch->last, node);
        batch->count += 1;
    }
    
    R_Quad_Node *last_node = batch->last;
    if (last_node->count >= R_MAX_QUAD_CHUNK) {
        R_Quad_Node *node = arena_push_array(arena, R_Quad_Node, 1);
        SLLQueuePush(batch->first, batch->last, node);
        batch->count += 1;
        last_node = node;
    }
    
    if (last_node->quads == 0) {
        last_node->quads = arena_push_array(arena, R_Quad, R_MAX_QUAD_CHUNK);
        last_node->count = 0;
    }
    
    R_Quad *slot = last_node->quads + last_node->count;
    MemoryCopyStruct(slot, quad);
    
    last_node->count += 1;
    batch->total_quad_count += 1;
}

internal void r_rect_ex(R_Ctx *ctx, RectF32 pos, u32 col, f32 radius, f32 theta)
{
    r_prep_batch(ctx->arena, ctx->list, 0);
    
    R_Quad quad = {0};
    quad.pos = pos;
    quad.col = col;
    quad.radius = radius;
    quad.theta = theta;
    quad.uv = { 0.0f, 0.0f, 1.0f, 1.0f };
    
    r_push_quad(ctx->arena, ctx->list->last, &quad);
}

internal void r_rect(R_Ctx *ctx, RectF32 pos, u32 col, f32 radius)
{
    r_rect_ex(ctx, pos, col, radius, 0.0f);
}

internal void r_circ(R_Ctx *ctx, HMM_Vec2 pos, f32 radius, u32 col)
{
    f32 nx = pos.X - radius;
    f32 ny = pos.Y - radius;
    RectF32 offset_pos = {
        nx, ny,
        nx + 2.0f*radius, ny + 2.0f*radius
    };   
    r_rect_ex(ctx, offset_pos, col, radius, 0.0f);
}

internal void r_rect_tex_ex(R_Ctx *ctx, RectF32 pos, u32 tint, f32 radius, f32 theta, RectF32 uv, R_Texture2D *texture)
{
    r_prep_batch(ctx->arena, ctx->list, texture);
    
    R_Quad quad = {0};
    quad.pos = pos;
    quad.col = tint;
    quad.radius = radius;
    quad.theta = theta;
    quad.uv = uv;
    
    r_push_quad(ctx->arena, ctx->list->last, &quad);
}

internal void r_rect_tex(R_Ctx *ctx, RectF32 pos, f32 radius, R_Texture2D *texture)
{
    RectF32 uv = { 0.0f, 0.0f, 1.0f, 1.0f };
    r_rect_tex_ex(ctx, pos, 0xFFFFFFFF, radius, 0.0f, uv, texture);
}

internal void r_flush_batches(GFX_Window *window, R_List *list)
{
    if (!r_is_init()) {
        er_push(str8("render backend not initialized"));
    } else {
        R_Quad_Batch *batch = list->first;
        while (batch != 0) {
            r_submit_quads(window, batch->first, batch->total_quad_count, batch->texture);
            batch = batch->next;
        }
    }
}
