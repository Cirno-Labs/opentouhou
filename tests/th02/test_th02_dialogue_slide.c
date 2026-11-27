#include "tests/test.h"
#include "tests/th02/th02_clip_fixture.h"
#include "tests/th02/th02_palette_fixture.h"
#include "tests/th02/th02_player_fixture.h"
#include "tests/th02/th02_score_fixture.h"
#include "tests/th02/th02_tram_fixture.h"
#include "th02/background/private.h"
#include "th02/dialogue/private.h"
#include "th02/player.h"
#include "th02/stage.h"
#include "th02/stage/private.h"

#include "core/base/arena.h"
#include "core/base/memory.h"

#include <string.h>

typedef struct {
    ARENA arena;

    PATTERN_BANK patterns;
    PALETTE_STATE before_palette;
} FIXTURE;

static void M_Init(FIXTURE *const fixture)
{
    memset(p_Palette, 0, sizeof((*p_Palette)));
    Grcg_Init();
    Stage_Init();
    Tram_Init();
    memset(fixture, 0, sizeof(*fixture));
    memset(&m_Player, 0, sizeof(m_Player));
    Arena_Init(&fixture->arena, "dialogue slide test", 2 * 1024 * 1024);
    Video_Init(Arena_Allocator(&fixture->arena));
    memset(g_Video.pages[0], 15, VIDEO_PAGE_SIZE);
    memset(g_Video.pages[1], 15, VIDEO_PAGE_SIZE);
    Clip_Reset();
    memset(p_Tram, 0xa5, sizeof(*p_Tram));
    PatternBank_Init(&fixture->patterns, &g_HeapAllocator);
    uint8_t pixels[32 * 48];
    memset(pixels, 1, sizeof(pixels));
    CHECK(Result_Ignore(
        PatternBank_Put(&fixture->patterns, 0, 32, 48, pixels, 0)));
    memset(pixels, 4, sizeof(pixels));
    for (uint16_t image = 34; image <= 46; image++) {
        if (image <= 38 || image >= 42) {
            CHECK(Result_Ignore(
                PatternBank_Put(&fixture->patterns, image, 32, 32, pixels, 0)));
        }
    }
    memset(pixels, 2, sizeof(pixels));
    CHECK(Result_Ignore(
        PatternBank_Put(&fixture->patterns, 72, 16, 16, pixels, 0)));
    CHECK(Result_Ignore(PatternBank_ConvertTiny(&fixture->patterns, 72)));
    m_Player.current = (ENTITY_POINT) { 200, 330 };
    m_Player.option[0] = (ENTITY_POINT) { 180, 350 };
    m_Player.option[1] = (ENTITY_POINT) { 100, 300 };
    m_Player.option_sprite = 74;
    p_Stage->scroll.front = 1;
    Palette_Init();
    p_Palette->tone = 70;
    Score_Init();
    p_Score->value = 99900;
    p_Score->delta = 200;
    p_Score->transferred = 17;
    p_Score->lives = 4;
}

static void M_Free(FIXTURE *const fixture)
{
    PatternBank_Reset(&fixture->patterns);
    Arena_Free(&fixture->arena);
}

static uint16_t M_Finish(DIALOGUE_SLIDE *const slide)
{
    uint16_t frames = 0;
    while (DialogueSlide_Update(slide) == DIALOGUE_SLIDE_FRAME) {
        frames++;
        CHECK(frames <= 24);
    }
    const DIALOGUE_SLIDE before = *slide;
    CHECK_EQ_INT(DialogueSlide_Update(slide), DIALOGUE_SLIDE_DONE);
    CHECK(memcmp(&before, slide, sizeof(before)) == 0);
    return frames;
}

TEST(entry_grants_retained_delta_before_palette_pages_and_the_first_frame)
{
    FIXTURE fixture;
    M_Init(&fixture);
    const PLAYER player = m_Player;
    const STAGE_SCROLL scroll = g_Stage->scroll;
    CHECK_EQ_INT(g_Video.drawing_page, 0);
    CHECK_EQ_INT(p_Palette->tone, 70);
    DIALOGUE_SLIDE slide;
    DialogueSlide_BeginEnter(&slide, &fixture.patterns);
    CHECK_EQ_INT(p_Score->value, 100100);
    CHECK_EQ_INT(p_Score->delta, 200);
    CHECK_EQ_INT(p_Score->transferred, 17);
    CHECK_EQ_INT(p_Score->extends, 1);
    CHECK_EQ_INT(p_Score->lives, 5);
    CHECK_EQ_INT(p_Tram->codes[80 + 4], ' ');
    CHECK_EQ_INT(p_Tram->codes[6 * TRAM_COLUMNS + 58], 0x2157);
    CHECK_EQ_INT(p_Tram->codes[4 * TRAM_COLUMNS + 58], 0x2157);
    CHECK_EQ_INT(p_Tram->codes[17 * TRAM_COLUMNS + 70], 0x0256);
    CHECK_EQ_INT(p_Palette->tone, 100);
    CHECK_EQ_INT(g_Video.drawing_page, 1);
    CHECK_EQ_INT(g_Video.visible_page, 0);
    CHECK_EQ_INT(g_Video.first_lines, 400);
    CHECK_EQ_INT(slide.top, 320);
    CHECK_EQ_INT(slide.bottom, 352);
    CHECK_EQ_INT(DialogueSlide_Update(&slide), DIALOGUE_SLIDE_FRAME);
    CHECK_EQ_INT(slide.left, 384);
    CHECK_EQ_INT(g_Video.pages[1][330 * 640 + 200], 15);
    CHECK_EQ_INT(DialogueSlide_Update(&slide), DIALOGUE_SLIDE_FRAME);
    CHECK_EQ_INT(g_Video.pages[1][330 * 640 + 200], 1);
    CHECK_EQ_INT(g_Video.pages[1][350 * 640 + 180], 2);
    CHECK_EQ_INT(g_Video.pages[1][300 * 640 + 100], 15);
    CHECK_EQ_INT(g_Video.pages[1][320 * 640 + 384], 4);
    CHECK(memcmp(&m_Player, &player, sizeof(player)) == 0);
    CHECK(memcmp(&p_Stage->scroll, &scroll, sizeof(scroll)) == 0);
    M_Free(&fixture);
}

TEST(entry_and_exit_keep_exact_frame_counts_and_restore_only_disabled_tiles)
{
    const uint8_t modes[] = { TILE_BLACK, TILE_IMAGES, TILE_DISABLED, 3, 255 };
    for (size_t i = 0; i < ARRAY_SIZE(modes); i++) {
        FIXTURE fixture;
        M_Init(&fixture);
        p_Background->cache.mode = modes[i];
        p_Stage->scroll.line = 79;
        const PLAYER player = m_Player;
        const STAGE_SCROLL scroll = g_Stage->scroll;
        DIALOGUE_SLIDE slide;
        DialogueSlide_BeginEnter(&slide, &fixture.patterns);
        CHECK_EQ_INT(slide.top, 399);
        CHECK_EQ_INT(slide.bottom, 31);
        CHECK_EQ_INT(M_Finish(&slide), 23);
        CHECK_EQ_INT(slide.left, 32);
        CHECK_EQ_INT(slide.restore_disabled, modes[i] == TILE_DISABLED);
        DialogueSlide_BeginLeave(&slide, &fixture.patterns);
        CHECK_EQ_INT(M_Finish(&slide), 24);
        CHECK_EQ_INT(slide.left, -352);
        CHECK(!slide.restore_disabled);
        CHECK_EQ_INT(p_Background->cache.mode, modes[i]);
        CHECK_EQ_INT(g_Video.drawing_page, g_Stage->scroll.back);
        CHECK_EQ_INT(g_Grcg->mode, 0);
        CHECK_EQ_INT(g_Grcg->tiles[0], 0);
        CHECK_EQ_INT(g_Grcg->tiles[1], 255);
        CHECK_EQ_INT(p_Score->value, 100100);
        CHECK_EQ_INT(p_Score->delta, 200);
        CHECK(memcmp(&m_Player, &player, sizeof(player)) == 0);
        CHECK(memcmp(&p_Stage->scroll, &scroll, sizeof(scroll)) == 0);
        M_Free(&fixture);
    }
}

TEST(leave_keeps_palette_and_score_and_selects_the_live_back_page_at_completion)
{
    FIXTURE fixture;
    M_Init(&fixture);
    const SCORE score = *g_Score;
    const PALETTE_STATE before_palette = (*p_Palette);
    p_Stage->scroll.back = p_Stage->scroll.front;
    p_Background->cache.mode = TILE_DISABLED;
    DIALOGUE_SLIDE slide = {};
    DialogueSlide_BeginLeave(&slide, &fixture.patterns);
    CHECK_EQ_INT(g_Video.drawing_page, 0);
    CHECK_EQ_INT(M_Finish(&slide), 24);
    CHECK_EQ_INT(p_Background->cache.mode, TILE_DISABLED);
    CHECK_EQ_INT(g_Video.drawing_page, 1);
    CHECK(memcmp(g_Score, &score, sizeof(score)) == 0);
    CHECK(memcmp(p_Palette, &before_palette, sizeof(before_palette)) == 0);
    M_Free(&fixture);
}
