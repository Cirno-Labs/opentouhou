#include "tests/test.h"
#include "tests/th02/th02_clip_fixture.h"
#include "tests/th02/th02_palette_fixture.h"
#include "tests/th02/th02_player_fixture.h"
#include "tests/th02/th02_score_fixture.h"
#include "tests/th02/th02_tram_fixture.h"
#include "th02/background/private.h"
#include "th02/dialogue/sequence.h"
#include "th02/player.h"
#include "th02/stage.h"
#include "th02/stage/private.h"

#include "core/base/arena.h"
#include "core/base/memory.h"

#include <string.h>

typedef struct {
    ARENA arena;

    PATTERN_BANK patterns;

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

TEST(built_in_script_metadata_loads_counts_faces_and_digits)
{
    DIALOGUE_SCRIPTS scripts;
    CHECK(Result_Ignore(DialogueScripts_Load(&scripts)));
    CHECK_EQ_INT(scripts.counts[0], 16);
    CHECK_EQ_INT(scripts.counts[8], 0);
    CHECK_EQ_INT(scripts.faces[0][0], 0);
    CHECK_EQ_INT(scripts.faces[0][1], 96);
    CHECK_EQ_INT(scripts.stage4_intro[0], 57);
    CHECK_EQ_INT(scripts.stage4_marisa[0], 57);
    CHECK_EQ_INT(scripts.stage4_before[0], 96);
    CHECK_EQ_INT(scripts.stage4_after[0], 144);
    CHECK_EQ_INT(scripts.stage4_no_continue[0], 96);
    for (size_t i = 0; i < 10; i++) {
        CHECK(scripts.digits[i].wide);
        CHECK_EQ_INT(scripts.digits[i].source_end, 2);
    }
}

TEST(sequence_waits_for_vsync_and_release_press_and_restores_pre_dialogue_pages)
{
    for (unsigned post = 0; post < 2; post++) {
        FIXTURE fixture;
        M_Init(&fixture);
        p_Background->cache.mode = TILE_DISABLED;
        const PLAYER player = m_Player;
        const STAGE_SCROLL scroll = g_Stage->scroll;
        const TRAM_CHARACTER character = { .code = 'X', .source_end = 1 };
        const TRAM_TEXT text = { "X", &character, 1, 1 };
        DIALOGUE_BOX boxes[2] = {};
        boxes[1].lines[0] = text;
        boxes[1].lines[1] = text;
        const DIALOGUE_ASSETS assets = { .boxes = boxes, .count = 2 };
        DIALOGUE_SCRIPTS scripts = {};
        scripts.counts[post] = 1;
        scripts.faces[post][0] = 255;
        DIALOGUE_RUNTIME runtime = { .box = 1 };
        memset(&g_VSync, 0, sizeof(g_VSync));
        g_VSync.count1 = 20;
        g_VSync.count2 = 30;
        g_VSync.edges = 40;
        DIALOGUE_SEQUENCE sequence;
        DialogueSequence_Begin(
            &sequence, &fixture.patterns, &assets, &scripts, &runtime, 0,
            post != 0);
        TH02_INPUT input = { .shot = true };
        CHECK(!DialogueSequence_Update(&sequence, &input));
        CHECK_EQ_INT(g_VSync.count1, 0);
        CHECK_EQ_INT(g_VSync.count2, 30);
        CHECK_EQ_INT(g_VSync.edges, 40);
        const DIALOGUE_SEQUENCE waiting = sequence;
        CHECK(!DialogueSequence_Update(&sequence, &input));
        CHECK(memcmp(&waiting, &sequence, sizeof(sequence)) == 0);
        CHECK_EQ_INT(p_Score->value, 100100);
        CHECK_EQ_INT(p_Score->delta, 200);
        unsigned enter_frames = 1;
        while (sequence.phase == DIALOGUE_SEQUENCE_ENTER && enter_frames < 24) {
            VSyncIrq_VSync();
            CHECK(!DialogueSequence_Update(&sequence, &input));
            if (sequence.phase == DIALOGUE_SEQUENCE_ENTER) {
                enter_frames++;
            }
            CHECK(enter_frames <= 23);
        }
        CHECK_EQ_INT(enter_frames, 23);
        CHECK_EQ_INT(sequence.box.phase, DIALOGUE_CONFIRM_FRAMES);
        CHECK_EQ_INT(runtime.box, 1);
        CHECK_EQ_INT(runtime.keys, GAME_KEY_SHOT);
        CHECK_EQ_INT(p_Tram->codes[21 * 80 + 14], 'X');
        CHECK_EQ_INT(p_Tram->codes[22 * 80 + 14], 'X');
        for (unsigned i = 0; i < 5; i++) {
            VSyncIrq_VSync();
            CHECK(!DialogueSequence_Update(&sequence, &input));
            CHECK_EQ_INT(runtime.box, 1);
        }
        input.shot = false;
        for (unsigned i = 0; i < 4; i++) {
            VSyncIrq_VSync();
            CHECK(!DialogueSequence_Update(&sequence, &input));
            CHECK_EQ_INT(runtime.box, 1);
        }
        input.shot = true;
        VSyncIrq_VSync();
        const bool ready = DialogueSequence_Update(&sequence, &input);
        CHECK_EQ_INT(ready, post);
        CHECK_EQ_INT(runtime.box, 2);
        unsigned leave_frames = 0;
        while (sequence.phase != DIALOGUE_SEQUENCE_DONE && leave_frames < 25) {
            VSyncIrq_VSync();
            DialogueSequence_Update(&sequence, &input);
            leave_frames++;
            CHECK(leave_frames <= 24);
        }
        CHECK_EQ_INT(leave_frames, post != 0 ? 0 : 24);
        CHECK_EQ_INT(g_Video.drawing_page, post != 0 ? 1 : 0);
        CHECK_EQ_INT(
            p_Background->cache.mode, post != 0 ? TILE_BLACK : TILE_DISABLED);
        CHECK_EQ_INT(p_Score->value, 100100);
        CHECK_EQ_INT(p_Score->delta, 200);
        CHECK_EQ_INT(p_Score->transferred, 17);
        CHECK_EQ_INT(p_Score->extends, 1);
        CHECK_EQ_INT(p_Score->lives, 5);
        CHECK(memcmp(&m_Player, &player, sizeof(player)) == 0);
        CHECK(memcmp(&p_Stage->scroll, &scroll, sizeof(scroll)) == 0);
        const DIALOGUE_SEQUENCE done = sequence;
        CHECK(DialogueSequence_Update(&sequence, nullptr));
        CHECK(memcmp(&done, &sequence, sizeof(done)) == 0);
        M_Free(&fixture);
    }
}
