#ifndef FONT_H
#define FONT_H

#define FONT_INIT_ATLAS_SIZE 128.0f
#define FONT_GLYPH_COUNT 128

typedef struct {
    HMM_Vec2 size;
    HMM_Vec2 origin;
    HMM_Vec2 offset;
    RectF32 uv;
    f32 advance;
} Font_Glyph_Info;

typedef struct {
    R_Texture2D *texture;
    HMM_Vec2 texture_size;
    u32 font_size;

    // @ToDo: Make this a variable sized array?
    Font_Glyph_Info glyphs[FONT_GLYPH_COUNT];
} Font;

internal b32 font_is_init(void);
internal void font_end(Font *font);
internal Font font_init(Arena *arena, String8 font_name, u32 font_size, u32 dpi);
internal f32 font_text_width_ex(Font *font, String8 text, f32 scale);
internal f32 font_text_width(Font *font, String8 text);
internal void font_r_text_ex(R_Ctx *ctx, Font *font, HMM_Vec2 pos, String8 text, f32 scale);
internal void font_r_text(R_Ctx *ctx, Font *font, HMM_Vec2 pos, String8 text);

#endif // FONT_H
