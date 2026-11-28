#include "tests/test.h"
#include "tests/th02/th02_clip_fixture.h"
#include "tests/th02/th02_palette_fixture.h"
#include "th02/ending/private.h"

#include "core/base/arena.h"

#include <string.h>

typedef struct {
    ARENA arena;

    ENDING_ASSETS assets;
    DRAW_TEXT lines[94];
    DRAW_TEXT_GLYPH glyphs[22];
    TRAM_CHARACTER characters[22];
    uint16_t rows[16];
    STORY_MUSIC songs[2];
    size_t song_count;
    TH02_SESSION session;
    bool extra_unlocked;
} FIXTURE;

static void M_Music(void *const context, const STORY_MUSIC song)
{
    FIXTURE *const f = context;
    CHECK(f->song_count < ARRAY_SIZE(f->songs));
    if (f->song_count < ARRAY_SIZE(f->songs)) {
        f->songs[f->song_count++] = song;
    }
}

static void M_Init(FIXTURE *const f)
{
    memset(p_Palette, 0, sizeof((*p_Palette)));
    Grcg_Init();
    Tram_Init();
    memset(&g_VSync, 0, sizeof(g_VSync));
    memset(f, 0, sizeof(*f));
    Arena_Init(&f->arena, "Ending owner test", 8 * 1024 * 1024);
    const ALLOCATOR *const allocator = Arena_Allocator(&f->arena);
    Video_Init(allocator);
    Clip_Reset();
    Palette_Init();
    g_VSync.count2 = 65500;
    PatternBank_Init(&f->assets.title, allocator);
    uint8_t title[16 * 32];
    memset(title, 7, sizeof(title));
    for (uint16_t slot = 0; slot < 60; slot++) {
        CHECK(Result_Ignore(
            PatternBank_Put(&f->assets.title, slot, 16, 32, title, 0)));
    }
    f->rows[0] = 0x8000;
    for (size_t glyph = 0; glyph < 22; glyph++) {
        f->glyphs[glyph] = (DRAW_TEXT_GLYPH) { f->rows, true };
        f->characters[glyph].source_end = (uint16_t)((glyph + 1) * 2);
    }
    for (size_t line = 0; line < ARRAY_SIZE(f->lines); line++) {
        f->lines[line].glyphs = f->glyphs;
        f->lines[line].count = 22;
        f->lines[line].cells.characters = f->characters;
    }
    f->assets.story[ENDING_STORY_BAD] = (ENDING_TEXT_SCRIPT) { f->lines, 51 };
    f->assets.story[ENDING_STORY_GOOD] = (ENDING_TEXT_SCRIPT) { f->lines, 94 };
    for (size_t route = 0; route < ENDING_STORY_COUNT; route++) {
        f->assets.verdict[route] = (ENDING_TEXT_SCRIPT) { f->lines, 12 };
    }
    for (size_t label = 0; label < ENDING_LABEL_COUNT; label++) {
        f->assets.labels.text[label] = f->lines[0];
    }
    for (size_t image = 0; image < ENDING_IMAGE_COUNT; image++) {
        PI_IMAGE *const pixels = &f->assets.images.images[image];
        pixels->width = 640;
        pixels->height = 400;
        pixels->pixels = Allocator_Alloc(allocator, VIDEO_VISIBLE_SIZE);
        memset(pixels->pixels, (int)(image + 1), VIDEO_VISIBLE_SIZE);
    }
    f->session.stage = 127;
    f->session.rank = 1;
    f->session.start_lives = 2;
    f->session.start_bombs = 3;
    f->session.score = 1234567;
    f->session.skill = 77;
}

static void M_Begin(FIXTURE *const f)
{
    Ending_Begin(&f->assets, &f->session, M_Music, f, f->extra_unlocked);
}

TEST(entry_retains_the_displayed_palette_through_the_initial_wait)
{
    for (unsigned extra = 0; extra < 2; extra++) {
        FIXTURE f;
        M_Init(&f);
        if (extra != 0) {
            f.session.rank = SESSION_EXTRA_RANK;
            f.extra_unlocked = true;
        }
        memset(p_Palette->colors, 0xaa, sizeof(p_Palette->colors));
        Palette_SetTone(75);
        GFX_COLOR displayed[16];
        memcpy(displayed, g_Video.palette, sizeof(displayed));
        M_Begin(&f);
        CHECK(memcmp(displayed, g_Video.palette, sizeof(displayed)) == 0);
        const TH02_INPUT input = {};
        for (unsigned i = 0; i < 99; i++) {
            VSyncIrq_VSync();
            CHECK(!Ending_Update(&input, false, 0));
            CHECK(memcmp(displayed, g_Video.palette, sizeof(displayed)) == 0);
        }
        Arena_Free(&f.arena);
    }
}

TEST(normal_routes_keep_one_cache_through_story_credits_and_verdict)
{
    for (size_t route = 0; route < 2; route++) {
        FIXTURE f;
        M_Init(&f);
        f.session.continues_used = route == 0 ? 0 : 1;
        f.session.shot_type = route == 0 ? 0 : 2;
        const TH02_SESSION saved = f.session;
        M_Begin(&f);
        TH02_INPUT input = { .shot = true };
        CHECK(!Ending_Update(&input, false, 0));
        CHECK_EQ_INT(p_Ending->phase, ENDING_PREPARE);
        for (size_t frame = 0; frame < 99; frame++) {
            VSyncIrq_VSync();
            CHECK(!Ending_Update(&input, false, 0));
            CHECK_EQ_INT(p_Ending->phase, ENDING_PREPARE);
        }
        bool done = false;
        unsigned phases = 0;
        size_t frame;
        for (frame = 99; frame < 15000 && !done; frame++) {
            VSyncIrq_VSync();
            input.shot = p_Ending->phase == ENDING_VERDICT
                    && p_Verdict->phase == VERDICT_CONFIRM
                ? (frame % 8 >= 4)
                : true;
            done = Ending_Update(&input, route != 0, UINT16_MAX);
            phases |= 1u << p_Ending->phase;
        }
        CHECK(done);
        CHECK((phases & (1u << ENDING_STORY_SCENE)) != 0);
        CHECK((phases & (1u << ENDING_CREDITS)) != 0);
        CHECK((phases & (1u << ENDING_VERDICT)) != 0);
        CHECK(
            p_Verdict->assets == &f.assets
            && p_Verdict->route == p_Ending->route);
        CHECK_EQ_INT(f.song_count, 2);
        CHECK_EQ_INT(f.songs[0], STORY_MUSIC_INTRO);
        CHECK_EQ_INT(f.songs[1], STORY_MUSIC_CREDITS);
        CHECK_EQ_INT(g_VSync.count2, (uint16_t)(65500 + frame));
        CHECK_EQ_INT(p_Palette->tone, 0);
        CHECK_EQ_INT(g_Video.pages[0][0], 0);
        CHECK_EQ_INT(g_Video.drawing_page, 0);
        CHECK_EQ_INT(g_Video.visible_page, 0);
        CHECK(memcmp(&saved, &f.session, sizeof(saved)) == 0);
        const ENDING finished = *p_Ending;
        CHECK(Ending_Update(nullptr, false, 0));
        CHECK(memcmp(&finished, p_Ending, sizeof(finished)) == 0);
        Arena_Free(&f.arena);
    }
}

TEST(extra_clear_fades_holds_all_clear_then_waits_on_the_partial_picture)
{
    FIXTURE f;
    M_Init(&f);
    f.session.rank = SESSION_EXTRA_RANK;
    f.extra_unlocked = true;
    f.assets.images.images[ENDING_IMAGE_EXTRA_CLEAR].width = 320;
    f.assets.images.images[ENDING_IMAGE_EXTRA_CLEAR].height = 160;
    M_Begin(&f);
    TH02_INPUT input = { .shot = true };
    for (unsigned i = 0; i < 100; i++) {
        VSyncIrq_VSync();
        CHECK(!Ending_Update(&input, false, 0));
    }
    CHECK_EQ_INT(p_Ending->phase, ENDING_FINISH_IN);
    CHECK_EQ_INT(p_Palette->tone, 0);
    CHECK_EQ_INT(g_Video.pages[0][0], ENDING_IMAGE_ALL_CLEAR + 1);
    const ENDING waiting = *p_Ending;
    CHECK(!Ending_Update(&input, false, 0));
    CHECK(memcmp(&waiting, p_Ending, sizeof(waiting)) == 0);
    for (unsigned i = 0; i < 35; i++) {
        VSyncIrq_VSync();
        CHECK(!Ending_Update(&input, false, 0));
    }
    CHECK_EQ_INT(p_Ending->phase, ENDING_FINISH_HOLD);
    CHECK_EQ_INT(p_Palette->tone, 100);
    for (unsigned i = 0; i < 149; i++) {
        VSyncIrq_VSync();
        CHECK(!Ending_Update(&input, false, 0));
        CHECK_EQ_INT(p_Ending->phase, ENDING_FINISH_HOLD);
    }
    VSyncIrq_VSync();
    CHECK(!Ending_Update(&input, false, 0));
    CHECK_EQ_INT(p_Ending->phase, ENDING_FINISH_KEY);
    CHECK_EQ_INT(g_Video.pages[0][0], ENDING_IMAGE_EXTRA_CLEAR + 1);
    CHECK_EQ_INT(g_Video.pages[0][320], ENDING_IMAGE_ALL_CLEAR + 1);
    CHECK_EQ_INT(g_Video.pages[0][160 * 640], ENDING_IMAGE_ALL_CLEAR + 1);
    CHECK_EQ_INT(f.song_count, 0);
    input.shot = false;
    for (unsigned i = 0; i < 4; i++) {
        VSyncIrq_VSync();
        CHECK(!Ending_Update(&input, false, 0));
    }
    input.shot = true;
    for (unsigned i = 0; i < 4 && p_Ending->phase == ENDING_FINISH_KEY; i++) {
        VSyncIrq_VSync();
        CHECK(!Ending_Update(&input, false, 0));
    }
    CHECK_EQ_INT(p_Ending->phase, ENDING_FINISH_OUT);
    for (unsigned i = 0; i < 86; i++) {
        VSyncIrq_VSync();
        CHECK_EQ_INT(Ending_Update(&input, false, 0), i == 85);
    }
    CHECK_EQ_INT(p_Palette->tone, 0);
    CHECK_EQ_INT(g_Video.pages[0][0], ENDING_IMAGE_EXTRA_CLEAR + 1);
    Arena_Free(&f.arena);
}

TEST(extra_without_normal_clear_markers_skips_the_finish_pictures)
{
    FIXTURE f;
    M_Init(&f);
    f.session.rank = SESSION_EXTRA_RANK;
    for (size_t i = 0; i < 16; i++) {
        f.assets.title_palette[i] = (GFX_COLOR) { 0x22, 0x44, 0x66, 255 };
    }
    memset(p_Palette->colors, 255, sizeof(p_Palette->colors));
    M_Begin(&f);
    const TH02_INPUT input = {};
    for (unsigned i = 0; i < 100; i++) {
        VSyncIrq_VSync();
        CHECK_EQ_INT(Ending_Update(&input, false, 0), i == 99);
    }
    CHECK_EQ_INT(p_Ending->phase, ENDING_DONE);
    CHECK_EQ_INT(g_Video.pages[0][0], 0);
    Palette_SetTone(50);
    for (size_t i = 0; i < 16; i++) {
        CHECK_EQ_INT(g_Video.palette[i].red, 17);
        CHECK_EQ_INT(g_Video.palette[i].green, 34);
        CHECK_EQ_INT(g_Video.palette[i].blue, 51);
    }
    Arena_Free(&f.arena);
}

TEST(normal_finish_only_enters_unlock_after_all_clears_without_continues)
{
    for (unsigned branch = 0; branch < 3; branch++) {
        FIXTURE f;
        M_Init(&f);
        f.extra_unlocked = branch != 0;
        f.session.continues_used = branch == 2 ? 1 : 0;
        M_Begin(&f);
        p_Ending->phase = ENDING_VERDICT;
        p_Verdict->phase = VERDICT_DONE;
        const TH02_INPUT input = {};
        CHECK_EQ_INT(Ending_Update(&input, false, 0), branch != 1);
        CHECK_EQ_INT(
            p_Ending->phase, branch == 1 ? ENDING_FINISH_IN : ENDING_DONE);
        CHECK_EQ_INT(
            g_Video.pages[0][0], branch == 1 ? ENDING_IMAGE_ALL_CLEAR + 1 : 0);
        Arena_Free(&f.arena);
    }
}
