global b32 freetype_is_init = 0;

internal b32 font_is_init(void)
{
    return(freetype_is_init);
}

internal Font font_init(Arena *arena, String8 font_name, u32 font_size, u32 dpi)
{
    Arena_Temp scratch = arena_temp_begin(arena);
    b32 error = 0;
    
    Font_Rect_Node *root = arena_push_array(scratch.arena, Font_Rect_Node, 1);
    root->size.X = FLT_MAX;
    root->size.Y = FLT_MAX;

    FT_Library ft = {0};
    if (FT_Init_FreeType(&ft) != 0) {
        error = 1;
    }

    FT_Face face = {0};
    if (!error) {
        b32 init_face = FT_New_Face(ft, (const char *) font_name.data, 0, &face) == 0;
        b32 set_size = FT_Set_Char_Size(face, 0, (font_size << 6), dpi, dpi) == 0;
        
        if (!init_face || !set_size) {
            error = 1;
        }
    }

    Font result = {0};
    result.texture_size = { FONT_INIT_ATLAS_SIZE, FONT_INIT_ATLAS_SIZE };
    result.font_size = font_size;
    if (!error) {
        // @Note: First populate basic metrics and calculate texture size.
        for (u32 i = 0; i < FONT_GLYPH_COUNT; ++i) {
            FT_Load_Char(face, i, FT_LOAD_BITMAP_METRICS_ONLY | FT_LOAD_FORCE_AUTOHINT | FT_LOAD_TARGET_LIGHT);
            FT_Bitmap *bmp = &face->glyph->bitmap;
            HMM_Vec2 glyph_size = { (f32) bmp->width, (f32) bmp->rows };
                
            Font_Rect_Node *node = font_rect_pack(scratch.arena, root, glyph_size, result.texture_size);
            while (node == 0) {
                // @Note: We have to ask for a bigger texture.
                result.texture_size.X *= 2.0f;
                result.texture_size.Y *= 2.0f;            
                node = font_rect_pack(scratch.arena, root, glyph_size, result.texture_size);
            }

            result.glyphs[i].size = glyph_size;
            result.glyphs[i].origin = node->origin;
            result.glyphs[i].offset = { (f32) face->glyph->bitmap_left, (f32) face->glyph->bitmap_top };
            result.glyphs[i].advance = (f32) (face->glyph->advance.x >> 6);
        }

        // @Note: Then render the actual glyphs onto atlas.
        u32 *pixels = arena_push_array(scratch.arena, u32, (usize) (result.texture_size.X*result.texture_size.Y));
        for (u32 i = 0; i < FONT_GLYPH_COUNT; ++i) {
            FT_Load_Char(face, i, FT_LOAD_RENDER | FT_LOAD_FORCE_AUTOHINT | FT_LOAD_TARGET_LIGHT);
            FT_Bitmap *bmp = &face->glyph->bitmap;
            Font_Glyph_Info glyph = result.glyphs[i];
        
            for (u32 row = 0; row < bmp->rows; ++row) {
                for (u32 col = 0; col < bmp->width; ++col) {
                    u8 pixel = bmp->buffer[row*bmp->pitch + col];

                    if (pixel) {
                        u32 x = (u32) (glyph.origin.X + col);
                        u32 y = (u32) (glyph.origin.Y + row);
                        usize index = (usize) (y*result.texture_size.X + x);
                        pixels[index] = (pixel << 3*8) | (pixel << 2*8) | (pixel << 1*8) | 0xFF;
                    }
                }
            }
        
            result.glyphs[i].uv = {
                (glyph.origin.X)/result.texture_size.X,
                (glyph.origin.Y)/result.texture_size.Y,
                (glyph.origin.X + glyph.size.X)/result.texture_size.X,
                (glyph.origin.Y + glyph.size.Y)/result.texture_size.Y
            };
        }

        // @Note: Finally create the texture.
        result.texture = r_texture_create(pixels, (u32) result.texture_size.X, (u32) result.texture_size.Y);
    }

    if (!error) {
        FT_Done_FreeType(ft);
    }
    
    arena_temp_end(&scratch);
    freetype_is_init = !error;
    
    return(result);
}

internal f32 font_text_width_ex(Font *font, String8 text, f32 scale)
{
    f32 result = 0.0f;
    if (font_is_init()) {
        for (u32 i = 0; i < text.size; ++i) {
            Font_Glyph_Info glyph = font->glyphs[text.data[i]];
            result += glyph.advance*scale;
        }
    } else {
        er_push(str8("font not initialized"));
    }
    
    return(result);
}

internal f32 font_text_width(Font *font, String8 text)
{    
    return(font_text_width_ex(font, text, 1.0f));
}

internal void font_r_text_ex(R_Ctx *ctx, Font *font, HMM_Vec2 pos, String8 text, u32 col, f32 scale)
{
    OPTICK_EVENT();

    if (font_is_init()) {
        // @Hack(?): This is here because UV coordinates get messed up for pos = something.5f
        pos.X = (f32) ((s32) (pos.X));
        pos.Y = (f32) ((s32) (pos.Y));

        for (u32 i = 0; i < text.size; ++i) {
            Font_Glyph_Info glyph = font->glyphs[text.data[i]];
        
            HMM_Vec2 glyph_pos = {
                pos.X + glyph.offset.X*scale,
                pos.Y - glyph.offset.Y*scale
            };
        
            RectF32 glyph_rect = {
                glyph_pos.X, glyph_pos.Y,
                glyph_pos.X + glyph.size.X*scale, glyph_pos.Y + glyph.size.Y*scale
            };

            r_rect_tex_ex(ctx, glyph_rect, col, 0.0f, 0.0f, glyph.uv, font->texture);
            pos.X += glyph.advance*scale;
        }
    } else {
        er_push(str8("font not initialized"));
    }
}

internal void font_r_text(R_Ctx *ctx, Font *font, HMM_Vec2 pos, u32 col, String8 text)
{
    font_r_text_ex(ctx, font, pos, text, col, 1.0f);
}

internal void font_end(Font *font)
{
    if (font->texture) {
        r_texture_destroy(font->texture);
    }

    MemoryZero(font, sizeof(Font));
}
