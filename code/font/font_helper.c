// @Note: Thanks to -> https://straypixels.net/texture-packing-for-fonts/
internal Font_Rect_Node *font_rect_pack(Arena *arena, Font_Rect_Node *node, HMM_Vec2 size, HMM_Vec2 texture_size)
{
    if (node->left && node->right) {
        Font_Rect_Node *result = font_rect_pack(arena, node->left, size, texture_size);
        if (result != 0) {
            return(result);
        }

        result = font_rect_pack(arena, node->right, size, texture_size);
        return(result);
    }

    if (node->occupied) {
        Assert(node->left == 0 && node->right == 0);
        return(0);
    }

    HMM_Vec2 real_size = { node->size.X, node->size.Y };

    // @Note: Recalculate size if on edge
    if (node->size.X + node->origin.X == FLT_MAX) real_size.X = texture_size.X - node->origin.X;
    if (node->size.Y + node->origin.Y == FLT_MAX) real_size.Y = texture_size.Y - node->origin.Y;

    if (real_size.X < size.X || real_size.Y < size.Y) {
        // @Note: Doesn't fit, in our case this is an error
        return(0);
    }

    if (node->size.X == size.X && node->size.Y == size.Y) {
        // @Note: Perfect fit
        node->occupied = 1;
        return(node);
    }

    // @Note: Split and create new nodes
    Font_Rect_Node *left = arena_push_array(arena, Font_Rect_Node, 1);
    Font_Rect_Node *right = arena_push_array(arena, Font_Rect_Node, 1);

    f32 dx = real_size.X - size.X;
    f32 dy = real_size.Y - size.Y;
    b32 split_h = dx > dy;

    // @Note: Edge case
    if (dx == 0 && dy == 0) {
        if (node->size.X > node->size.Y) split_h = 1;
        else split_h = 0;
    }

    if (split_h) {
        // @Note: Split horizontally, left is up
        left->origin = { node->origin.X, node->origin.Y };
        left->size = { size.X, node->size.Y };

        right->origin = { node->origin.X + size.X, node->origin.Y };
        right->size = { node->size.X - size.X, node->size.Y };
    } else {
        // @Note: Split vertically
        left->origin = { node->origin.X, node->origin.Y };
        left->size = { node->size.X, size.Y };

        right->origin = { node->origin.X, node->origin.Y + size.Y };
        right->size = { node->size.X, node->size.Y - size.Y };
    }

    node->left = left;
    node->right = right;
    
    return(font_rect_pack(arena, node->left, size, texture_size));
}
