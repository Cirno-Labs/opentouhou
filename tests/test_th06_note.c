// SPDX-License-Identifier: GPL-3.0-only

#include "th06/note.h"

#include "gfx_recorder.h"
#include "patch_builder.h"
#include "test.h"

#include "th06/files.h"
#include "th06/translation.h"

#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/gfx.h"
#include "core/patch.h"
#include "core/text.h"

#include <string.h>

// Holds base_tsa's notes settings: the region of its th06.js at the
// reference resolution of its global.js.
#define M_BASE_TSA_NOTES                                                       \
    "reference_resolution: [1280, 960], region_topleft: [864, 516], "          \
    "region_size: [384, 412], font: \"'MS Gothic' 32 0 400 "                   \
    "DEFAULT_QUALITY\""

static PATCH_STACK *m_Stack;
static char *m_Directory;

// Starts the notes with a patch whose global.js holds the tlnotes object
// notes, below nmlgc/base_tsa when base_tsa is set.
static void M_Begin(
    const char *const name, const bool base_tsa, const char *const notes)
{
    m_Directory = TestDir_Prepare(name);
    if (base_tsa) {
        PatchBuilder_AddPatch(m_Directory, "nmlgc/base_tsa", "{}");
    }
    PatchBuilder_AddPatch(m_Directory, "a/lang", "{}");
    char *const settings = Str_Format("{tlnotes: {%s}}", notes);
    PatchBuilder_Write(m_Directory, "a/lang/global.js", settings);
    Mem_Free(settings);
    m_Stack = nullptr;
    CHECK(Result_Log(
        Patch_OpenStack(
            m_Directory, "th06", &g_PatchGame, nullptr, 0, &m_Stack),
        LOG_LEVEL_ERROR));
    Translation_Init(m_Stack);
    CHECK(Result_Log(Text_Init(), LOG_LEVEL_ERROR));
    GfxRecorder_Start();
    Note_Init(m_Stack);
}

static void M_End(void)
{
    Note_Shutdown();
    GfxRecorder_Stop();
    Text_Shutdown();
    Translation_Shutdown();
    Patch_CloseStack(m_Stack);
    Mem_Free(m_Directory);
}

// Draws one tick of notes and returns the quad of the note, or nullptr.
static const GFX_RECORDED_QUAD *M_Draw(void)
{
    Gfx_BeginTick((GFX_COLOR) {});
    Note_Draw();
    Gfx_EndTick();
    return GfxRecorder_QuadCount() == 1 ? GfxRecorder_Quad(0) : nullptr;
}

static void M_DrawTicks(const int32_t ticks)
{
    for (int32_t i = 0; i < ticks; i++) {
        M_Draw();
    }
}

static int32_t M_Alpha(const GFX_RECORDED_QUAD *const quad)
{
    return quad != nullptr ? quad->vertices[0].color.alpha : -1;
}

TEST(notes_need_base_tsa)
{
    M_Begin("no_base_tsa", false, M_BASE_TSA_NOTES);
    Note_Show("Line\x14Note");
    M_Draw();
    CHECK_EQ_INT(GfxRecorder_QuadCount(), 0);
    M_End();
}

TEST(notes_need_a_region)
{
    M_Begin("no_region", true, "reference_resolution: [1280, 960]");
    Note_Show("Line\x14Note");
    M_Draw();
    CHECK_EQ_INT(GfxRecorder_QuadCount(), 0);
    M_End();
}

TEST(a_region_larger_than_the_reference_shows_nothing)
{
    M_Begin(
        "large_region", true,
        "reference_resolution: [1280, 960], region_size: [1280, 412]");
    Note_Show("Line\x14Note");
    M_Draw();
    CHECK_EQ_INT(GfxRecorder_QuadCount(), 0);
    M_End();
}

TEST(a_text_without_a_note_shows_nothing)
{
    M_Begin("plain", true, M_BASE_TSA_NOTES);
    Note_Show("Line");
    Note_Show("Line\x14");
    M_Draw();
    CHECK_EQ_INT(GfxRecorder_QuadCount(), 0);
    M_End();
}

TEST(a_short_note_sits_centered_at_the_bottom_of_the_region)
{
    M_Begin("bottom", true, M_BASE_TSA_NOTES);
    Note_Show("Line\x14Note");
    const GFX_RECORDED_QUAD *const quad = M_Draw();
    CHECK(quad != nullptr);
    const float width = (float)GfxTexture_GetWidth(quad->texture);
    CHECK_EQ_INT(GfxTexture_GetHeight(quad->texture), 32 + 2 * 2);
    CHECK(width > 4.0f && width < 384.0f);
    CHECK(
        quad->vertices[0].position.x
        == (864.0f + (384.0f - width) / 2.0f) / 2.0f);
    CHECK(
        quad->vertices[1].position.x
        == quad->vertices[0].position.x + width / 2.0f);
    CHECK(quad->vertices[2].position.y == 464.0f);
    CHECK(quad->vertices[0].position.y == 464.0f - 18.0f);
    CHECK(quad->vertices[0].uv.y == 0.0f && quad->vertices[2].uv.y == 1.0f);
    CHECK(quad->vertices[0].uv.x == 0.0f && quad->vertices[2].uv.x == 1.0f);
    CHECK_EQ_INT(quad->state.blend, GFX_BLEND_ALPHA);
    CHECK_EQ_INT(quad->state.depth_test, GFX_DEPTH_OFF);
    M_End();
}

TEST(valign_puts_the_note_at_the_top_or_the_center)
{
    M_Begin("top", true, M_BASE_TSA_NOTES ", valign: 'top'");
    Note_Show("\x14Note");
    CHECK(M_Draw()->vertices[0].position.y == 258.0f);
    M_End();
    M_Begin("center", true, M_BASE_TSA_NOTES ", valign: 'center'");
    Note_Show("\x14Note");
    CHECK(M_Draw()->vertices[0].position.y == (516.0f + 206.0f - 18.0f) / 2.0f);
    M_End();
}

TEST(the_font_rule_sets_the_line_height)
{
    M_Begin(
        "font_height", true,
        "reference_resolution: [1280, 960], region_size: [384, 412], "
        "font: \"'Face' 20 0 400\", outline_radius: 3");
    Note_Show("\x14Note");
    CHECK_EQ_INT(GfxTexture_GetHeight(M_Draw()->texture), 20 + 2 * 3);
    M_End();
}

TEST(a_note_fades_in_over_500_milliseconds)
{
    M_Begin("fade", true, M_BASE_TSA_NOTES);
    Note_Show("\x14Note");
    CHECK_EQ_INT(M_Alpha(M_Draw()), 0);
    M_DrawTicks(14);
    CHECK_EQ_INT(M_Alpha(M_Draw()), 127);
    M_DrawTicks(14);
    CHECK_EQ_INT(M_Alpha(M_Draw()), 255);
    CHECK_EQ_INT(M_Alpha(M_Draw()), 255);
    M_End();
}

TEST(fade_ms_sets_the_fade)
{
    M_Begin("fade_ms", true, M_BASE_TSA_NOTES ", fade_ms: 1000");
    Note_Show("\x14Note");
    M_DrawTicks(30);
    CHECK_EQ_INT(M_Alpha(M_Draw()), 127);
    M_End();
}

TEST(the_same_note_keeps_its_fade_and_another_restarts_it)
{
    M_Begin("same", true, M_BASE_TSA_NOTES);
    Note_Show("One\x14Note");
    M_DrawTicks(30);
    Note_Show("Two\x14Note");
    CHECK_EQ_INT(M_Alpha(M_Draw()), 255);
    Note_Show("\x14Other");
    CHECK_EQ_INT(M_Alpha(M_Draw()), 0);
    M_End();
}

TEST(a_removed_note_hides_and_fades_in_again)
{
    M_Begin("remove", true, M_BASE_TSA_NOTES);
    Note_Show("\x14Note");
    M_DrawTicks(30);
    Note_Remove();
    CHECK(M_Draw() == nullptr);
    Note_Show("\x14Note");
    CHECK_EQ_INT(M_Alpha(M_Draw()), 0);
    M_End();
}

// thcrap compares the note of one frame with the note of the frame before,
// so a note hidden and shown again between two draws keeps its fade.
TEST(a_note_hidden_and_shown_between_draws_keeps_its_fade)
{
    M_Begin("reshow", true, M_BASE_TSA_NOTES);
    Note_Show("\x14Note");
    M_DrawTicks(30);
    Note_Remove();
    Note_Show("\x14Note");
    CHECK_EQ_INT(M_Alpha(M_Draw()), 255);
    M_End();
}

TEST(a_note_ends_at_the_next_separator)
{
    M_Begin("separator", true, M_BASE_TSA_NOTES);
    Note_Show("\x14Note\x14More");
    const int32_t width = GfxTexture_GetWidth(M_Draw()->texture);
    Note_Show("\x14Note");
    CHECK_EQ_INT(GfxTexture_GetWidth(M_Draw()->texture), width);
    M_End();
}

TEST(a_long_note_breaks_between_words_into_lines)
{
    M_Begin("lines", true, M_BASE_TSA_NOTES);
    Note_Show(
        "\x14The kanji numeral for ten is shaped like a cross, so the "
        "Japanese word for cross is literally character for ten.");
    const GFX_RECORDED_QUAD *const quad = M_Draw();
    const int32_t height = GfxTexture_GetHeight(quad->texture);
    CHECK(height > 32 * 2 + 4);
    CHECK_EQ_INT((height - 4) % 32, 0);
    CHECK(GfxTexture_GetWidth(quad->texture) <= 384 + 4);
    M_End();
}

TEST(a_newline_starts_a_line)
{
    M_Begin("newline", true, M_BASE_TSA_NOTES);
    Note_Show("\x14One\nTwo");
    CHECK_EQ_INT(GfxTexture_GetHeight(M_Draw()->texture), 2 * 32 + 4);
    M_End();
}

TEST(a_word_wider_than_the_region_keeps_its_line)
{
    M_Begin(
        "wide_word", true,
        "reference_resolution: [1280, 960], region_size: [40, 412], "
        "font: \"'Face' 32\"");
    Note_Show("\x14Wonderful");
    const GFX_RECORDED_QUAD *const quad = M_Draw();
    CHECK_EQ_INT(GfxTexture_GetHeight(quad->texture), 32 + 4);
    CHECK(GfxTexture_GetWidth(quad->texture) > 40 + 4);
    M_End();
}

// Returns the v of the top of a note of size bytes and texture_height
// rows in a region 40 rows high, age milliseconds after it showed, as
// thcrap computes it.
static float M_ExpectedScroll(
    const uint32_t age, const size_t size, const float texture_height)
{
    const uint32_t read_ms = (uint32_t)(1000.0f / 35.0f * (float)size);
    const float ms_per_row = (float)(read_ms / (uint32_t)texture_height);
    const float row = (float)(age % read_ms) / ms_per_row;
    float scroll = (row - 20.0f) / (texture_height - 40.0f);
    scroll = scroll < 1.0f ? scroll : 1.0f;
    scroll = scroll > 0.0f ? scroll : 0.0f;
    return scroll * (1.0f - 40.0f / texture_height);
}

TEST(a_note_taller_than_the_region_scrolls_through_it_and_starts_again)
{
    M_Begin(
        "scroll", true,
        "reference_resolution: [1280, 960], region_topleft: [0, 100], "
        "region_size: [100, 40], font: \"'Face' 32\"");
    const char *const note =
        "one two six ten one two six ten one two six ten one two six ten";
    char *const text = Str_Format("\x14%s", note);
    Note_Show(text);
    Mem_Free(text);
    const GFX_RECORDED_QUAD *quad = M_Draw();
    const float texture_height = (float)GfxTexture_GetHeight(quad->texture);
    CHECK(texture_height > 40.0f);
    CHECK(quad->vertices[0].position.y == 50.0f);
    CHECK(quad->vertices[2].position.y == 70.0f);
    CHECK(
        quad->vertices[2].uv.y - quad->vertices[0].uv.y
        == 40.0f / texture_height);
    bool bottom = false;
    bool again = false;
    for (uint32_t tick = 0; tick < 200; tick++) {
        const float v = quad->vertices[0].uv.y;
        CHECK(
            v
            == M_ExpectedScroll(
                tick * 1000 / 60, strlen(note), texture_height));
        bottom = bottom || v == 1.0f - 40.0f / texture_height;
        again = again || (bottom && v == 0.0f);
        quad = M_Draw();
    }
    CHECK(bottom);
    CHECK(again);
    M_End();
}

TEST(the_note_is_white_text_with_a_black_outline)
{
    M_Begin("outline", true, M_BASE_TSA_NOTES);
    Note_Show("\x14Note");
    const GFX_TEXTURE *const texture = M_Draw()->texture;
    const uint8_t *const pixels = GfxRecorder_TexturePixels(texture);
    const int32_t count =
        GfxTexture_GetWidth(texture) * GfxTexture_GetHeight(texture);
    bool white = false;
    bool outline = false;
    bool above = false;
    for (int32_t i = 0; i < count; i++) {
        const uint8_t *const pixel = &pixels[i * 4];
        white = white || (pixel[0] == 255 && pixel[3] == 255);
        outline = outline || (pixel[0] == 0 && pixel[3] == 255);
        above = above || pixel[0] > pixel[3] || pixel[0] != pixel[1]
            || pixel[0] != pixel[2];
    }
    CHECK(white);
    CHECK(outline);
    CHECK(!above);
    CHECK_EQ_INT(pixels[3], 0);
    M_End();
}

TEST(outline_radius_0_draws_no_outline)
{
    M_Begin("no_outline", true, M_BASE_TSA_NOTES ", outline_radius: 0");
    Note_Show("\x14Note");
    const GFX_TEXTURE *const texture = M_Draw()->texture;
    const uint8_t *const pixels = GfxRecorder_TexturePixels(texture);
    CHECK_EQ_INT(GfxTexture_GetHeight(texture), 32);
    const int32_t count =
        GfxTexture_GetWidth(texture) * GfxTexture_GetHeight(texture);
    bool same = true;
    for (int32_t i = 0; i < count; i++) {
        same = same && pixels[i * 4] == pixels[i * 4 + 3];
    }
    CHECK(same);
    M_End();
}

// Checks that the alpha of each pixel is the largest colour within the
// outline radius 2 around it: the outline is a disc.
TEST(the_outline_is_a_disc_of_its_radius)
{
    M_Begin("disc", true, M_BASE_TSA_NOTES);
    Note_Show("\x14Note");
    const GFX_TEXTURE *const texture = M_Draw()->texture;
    const uint8_t *const pixels = GfxRecorder_TexturePixels(texture);
    const int32_t width = GfxTexture_GetWidth(texture);
    const int32_t height = GfxTexture_GetHeight(texture);
    int32_t wrong = 0;
    for (int32_t y = 0; y < height; y++) {
        for (int32_t x = 0; x < width; x++) {
            uint8_t expected = 0;
            for (int32_t oy = -2; oy <= 2; oy++) {
                for (int32_t ox = -2; ox <= 2; ox++) {
                    const int32_t px = x + ox;
                    const int32_t py = y + oy;
                    if (ox * ox + oy * oy > 4 || px < 0 || py < 0 || px >= width
                        || py >= height) {
                        continue;
                    }
                    const uint8_t value = pixels[(py * width + px) * 4];
                    expected = value > expected ? value : expected;
                }
            }
            wrong += pixels[(y * width + x) * 4 + 3] != expected;
        }
    }
    CHECK_EQ_INT(wrong, 0);
    M_End();
}
