#include "th07/ascii.h"

#include "th07/files.h"
#include "th07/game.h"
#include "th07/game_manager.h"
#include "th07/player.h"
#include "th07/stubs.h"

#include "gfx_recorder.h"
#include "test.h"
#include "test_dir.h"
#include "th07_anm_builder.h"
#include "th07_data.h"
#include "th07_scene_stubs.h"

#include "core/base/memory.h"

#include "stg/rng.h"

#define I(x) { .i = (x) }
#define INSTR(op_, ...)                                                        \
    {                                                                          \
        .opcode = (op_),                                                       \
        .args = { __VA_ARGS__ },                                               \
    }
#define M_SPRITE_COUNT 0x200

static const TH07_DATA_FILE m_Version = { "th07_0100b.ver", "0100b" };
static RNG m_Rng;
static char *m_Directory;
static ANM_INSTR m_Instrs[] = {
    INSTR(3, I(0)),
    INSTR(20),
    INSTR(-1),
};
static ANM_SCRIPT m_Scripts[8];

static void M_WriteAnm(const char *const name)
{
    TH07_ANM_BUILDER builder;
    Th07AnmBuilder_Init(&builder);
    TH07_ANM_BUILDER_ANM *const anm =
        Th07AnmBuilder_Add(&builder, 256, 256, "@");
    Th07AnmBuilder_AddSprite(anm, 0, 0.0f, 0.0f, 8.0f, 8.0f);
    Th07AnmBuilder_WriteFile(&builder, m_Directory, name);
}

static RESULT M_Start(const char *const case_name, const bool capture)
{
    SceneStubCalls_Reset();
    GfxRecorder_Start();
    g_GameSpeed = 1.0f;
    m_Directory = TestDir_Prepare(case_name);
    Th07Data_WriteArchive(m_Directory, &m_Version, 1);
    Th07Data_WriteMusic(m_Directory, nullptr, 0);
    char *const data = Str_Format("%s/data", m_Directory);
    CHECK(SDL_CreateDirectory(data));
    Mem_Free(data);
    M_WriteAnm("data/ascii.anm");
    if (capture) {
        M_WriteAnm("data/capture.anm");
    }
    CHECK(Result_Log(Th07Data_Mount(m_Directory), LOG_LEVEL_ERROR));
    Anm_Init(&g_HeapAllocator, &m_Rng);
    Chain_Init(&g_Chain);
    g_GameManager.play_area = (PLAY_RECT) { 32.0f, 16.0f, 384.0f, 448.0f };
    g_GameManager.camera.viewport = (VIEWPORT) { 0, 0, 640, 480, 0, 1 };
    g_Player.pos = (VEC3) { 1000.0f, 1000.0f, 0.0f };
    return AsciiText_Register();
}

// Registers, then gives sprite i u0 = i / 1024 and scripts 3 to 7 sprite
// 0x100 + script.
static void M_Setup(const char *const case_name)
{
    CHECK(Result_Log(M_Start(case_name, true), LOG_LEVEL_ERROR));
    for (int32_t i = 0; i < M_SPRITE_COUNT; i++) {
        const float u = (float)i / 1024.0f;
        const ANM_SPRITE sprite = {
            .slot = 1,
            .x1 = 8.0f,
            .y1 = 8.0f,
            .texture_width = 256.0f,
            .texture_height = 256.0f,
            .u0 = u,
            .u1 = u,
            .width = 8.0f,
            .height = 8.0f,
            .scale_x = 1.0f,
            .scale_y = 1.0f,
            .id = i,
        };
        Anm_SetSpriteEntry(i, &sprite);
    }
    for (int32_t i = 3; i < 8; i++) {
        m_Scripts[i] = (ANM_SCRIPT) { m_Instrs, 3, 0x100 + i };
        Anm_SetScriptEntry(i, &m_Scripts[i]);
    }
    AsciiText_Initialize();
    Gfx_BeginTick((GFX_COLOR) {});
}

static void M_Teardown(void)
{
    Chain_Release(&g_Chain);
    Anm_Shutdown();
    Th07Data_Unmount();
    GfxRecorder_Stop();
    Mem_Free(m_Directory);
}

static void M_Draw(void)
{
    Chain_RunDraw(&g_Chain);
    Gfx_EndTick();
}

static int32_t M_Sprite(const int32_t quad)
{
    return (int32_t)(GfxRecorder_Quad(quad)->vertices[0].uv.x * 1024.0f);
}

static float M_X(const int32_t quad)
{
    return GfxRecorder_Quad(quad)->vertices[0].position.x;
}

static float M_Y(const int32_t quad)
{
    return GfxRecorder_Quad(quad)->vertices[0].position.y;
}

static uint32_t M_Color(const int32_t quad)
{
    const GFX_COLOR c = GfxRecorder_Quad(quad)->vertices[0].color;
    return (uint32_t)c.alpha << 24 | (uint32_t)c.red << 16
        | (uint32_t)c.green << 8 | c.blue;
}

TEST(register_fails_without_the_capture_sprites)
{
    CHECK(!Result_Log(M_Start("register_fails", false), LOG_LEVEL_INFO));
    M_Teardown();
}

TEST(initialize_keeps_the_summary_and_resends_its_interrupt)
{
    M_Setup("initialize");
    g_AsciiText.glyph_width = 11;
    g_AsciiText.summary_interrupt = 3;
    g_AsciiText.boss_marker_blue[2] = true;
    AsciiText_AddString((VEC3) {}, "A");
    AsciiText_Initialize();
    CHECK_EQ_INT(g_AsciiText.glyph_width, 14);
    CHECK_EQ_INT(g_AsciiText.string_count, 0);
    CHECK_EQ_INT(g_AsciiText.summary.pending_interrupt, 3);
    CHECK(g_AsciiText.boss_marker_blue[2]);
    CHECK_EQ_INT(g_AsciiText.digit.anchor, 0);
    M_Teardown();
}

TEST(strings_advance_by_glyph_width_and_scale)
{
    M_Setup("strings");
    g_AsciiText.scale_x = 2.0f;
    g_AsciiText.scale_y = 0.5f;
    g_AsciiText.glyph_width = 11;
    AsciiText_SetColor(0xff102030u);
    AsciiText_AddString((VEC3) { 100.0f, 50.0f, 0.0f }, "AB C\nD");
    g_AsciiText.highlight_font = true;
    AsciiText_SetHighlight(true);
    AsciiText_AddFormat((VEC3) { 10.0f, 20.0f, 0.0f }, "%c", 'A');
    M_Draw();
    CHECK_EQ_INT(GfxRecorder_QuadCount(), 5);
    CHECK_EQ_INT(M_Sprite(0), 'A' - 1);
    CHECK(M_X(0) == 100.0f && M_Y(0) == 50.0f);
    CHECK(M_X(1) == 122.0f);
    CHECK(M_X(2) == 166.0f);
    CHECK(M_X(3) == 100.0f && M_Y(3) == 58.0f);
    CHECK_EQ_INT(M_Color(0), 0xff102030u);
    CHECK_EQ_INT(M_Sprite(4), 'A' + 0x7c);
    CHECK_EQ_INT(M_Color(4), 0xffffffffu);
    CHECK_EQ_INT(g_AsciiText.string_count, 0);
    M_Teardown();
}

TEST(popups_start_in_the_play_area_and_end_after_sixty_frames)
{
    M_Setup("popup_lifetime");
    AsciiText_AddPopup((VEC3) { 10.0f, 20.0f, 0.0f }, 105, 0xff00ff00u);
    const ASCII_POPUP *const popup = &g_AsciiText.popups[0];
    CHECK(popup->pos.x == 42.0f && popup->pos.y == 36.0f);
    CHECK_EQ_INT(popup->digit_count, 3);
    for (int32_t i = 0; i < 60; i++) {
        Chain_RunCalc(&g_Chain);
    }
    CHECK(popup->active);
    CHECK(popup->pos.y == 6.0f);
    Chain_RunCalc(&g_Chain);
    CHECK(!popup->active);
    M_Teardown();
}

TEST(popups_move_and_count_at_the_game_speed)
{
    M_Setup("popup_speed");
    AsciiText_AddPopup2((VEC3) {}, -3, 0xffffffffu);
    const ASCII_POPUP *const popup = &g_AsciiText.popups[0x2d0];
    CHECK_EQ_INT(popup->digits[0], 10);
    g_GameSpeed = 0.5f;
    Chain_RunCalc(&g_Chain);
    Chain_RunCalc(&g_Chain);
    Chain_RunCalc(&g_Chain);
    CHECK_EQ_INT(popup->timer.current, 1);
    CHECK(popup->pos.y == 15.25f);
    g_GameManager.mode.paused = 2;
    Chain_RunCalc(&g_Chain);
    CHECK(popup->pos.y == 15.25f);
    M_Teardown();
}

TEST(popup_slots_wrap)
{
    M_Setup("popup_slots");
    g_AsciiText.next_popup = 0x2d0;
    AsciiText_AddPopup((VEC3) {}, 1, 0);
    CHECK_EQ_INT(g_AsciiText.next_popup, 1);
    g_AsciiText.next_popup2 = 3;
    AsciiText_AddPopup2((VEC3) {}, 1, 0);
    CHECK_EQ_INT(g_AsciiText.next_popup2, 1);
    CHECK(g_AsciiText.popups[0x2d0].active);
    M_Teardown();
}

TEST(popups_fade_near_the_player_and_swap_digits_late)
{
    M_Setup("popup_draw");
    AsciiText_AddPopup((VEC3) { 10.0f, 20.0f, 0.0f }, 105, 0xff00ff00u);
    AsciiText_AddPopup((VEC3) { 10.0f, 20.0f, 0.0f }, -1, 0xff00ff00u);
    AsciiText_AddPopup((VEC3) { 10.0f, 20.0f, 0.0f }, 7, 0xff00ff00u);
    g_AsciiText.popups[1].timer.current = 56;
    g_AsciiText.popups[2].timer.current = 52;
    g_Player.pos = (VEC3) { 74.0f, 68.0f, 0.0f };
    M_Draw();
    CHECK_EQ_INT(GfxRecorder_QuadCount(), 5);
    CHECK_EQ_INT(M_Sprite(0), 1);
    CHECK_EQ_INT(M_Sprite(1), 0);
    CHECK_EQ_INT(M_Sprite(2), 5);
    CHECK(M_X(0) == 26.0f && M_X(1) == 34.0f);
    CHECK_EQ_INT(M_Color(0), 0x7a00ff00u);
    CHECK_EQ_INT(GfxRecorder_Quad(0)->state.depth_test, GFX_DEPTH_ALWAYS);
    CHECK_EQ_INT(M_Sprite(3), 10);
    CHECK_EQ_INT(M_Sprite(4), 7 + 0xb);
    M_Teardown();
}

TEST(popup_alpha_bounds)
{
    M_Setup("popup_alpha");
    AsciiText_AddPopup((VEC3) { 10.0f, 20.0f, 0.0f }, 1, 0xffffffffu);
    AsciiText_AddPopup((VEC3) { 10.0f, 20.0f, 0.0f }, 3, 0xffffffffu);
    g_AsciiText.popups[1].timer.current = 56;
    g_Player.pos = (VEC3) { 42.75f, 77.75f, 0.0f };
    M_Draw();
    CHECK_EQ_INT(M_Color(0) >> 24, 0x6d);
    CHECK_EQ_INT(M_Sprite(1), 3 + 0x15);
    M_Teardown();
    M_Setup("popup_alpha_near");
    AsciiText_AddPopup((VEC3) { 10.0f, 20.0f, 0.0f }, 1, 0xffffffffu);
    g_Player.pos = (VEC3) { 42.0f, 36.0f, 0.0f };
    M_Draw();
    CHECK_EQ_INT(M_Color(0) >> 24, 0x50);
    M_Teardown();
    M_Setup("popup_alpha_far");
    AsciiText_AddPopup((VEC3) { 10.0f, 20.0f, 0.0f }, 1, 0xffffffffu);
    M_Draw();
    CHECK_EQ_INT(M_Color(0) >> 24, 0xd0);
    M_Teardown();
}

TEST(boss_markers_fade_near_the_player_and_turn_blue)
{
    M_Setup("boss_markers");
    const float xs[4] = { 200.0f, 232.0f, 50.0f, 300.0f };
    for (int32_t i = 0; i < 4; i++) {
        Anm_StartScript(&g_AsciiText.boss_markers[i], 6);
        g_AsciiText.boss_markers[i].pos = (VEC3) { xs[i], 470.0f, 0.0f };
    }
    g_AsciiText.boss_marker_blue[3] = true;
    g_Player.pos.x = 160.0f;
    M_Draw();
    CHECK_EQ_INT(GfxRecorder_QuadCount(), 3);
    CHECK_EQ_INT(M_Sprite(0), 0x106);
    CHECK_EQ_INT(M_Color(0), 0x40ffffffu);
    CHECK_EQ_INT(M_Color(1), 0x80ffffffu);
    CHECK_EQ_INT(M_Color(2), 0x804040ffu);
    M_Teardown();
}

TEST(boss_markers_beyond_64_pixels_stay_dim)
{
    M_Setup("boss_markers_far");
    Anm_StartScript(&g_AsciiText.boss_markers[0], 6);
    g_AsciiText.boss_markers[0].pos = (VEC3) { 392.0f, 470.0f, 0.0f };
    g_Player.pos.x = 296.0f;
    M_Draw();
    CHECK_EQ_INT(M_Color(0), 0xb0ffffffu);
    M_Teardown();
}

static void M_ShowSummary(void)
{
    Anm_StartScript(&g_AsciiText.summary, 4);
    Anm_StartScript(&g_AsciiText.summary_digit, 3);
    Anm_StartScript(&g_AsciiText.summary_bomb_icon, 5);
    g_AsciiText.summary.pos = (VEC3) { 100.0f, 200.0f, 0.0f };
    g_AsciiText.summary.color = 0xc0ffffffu;
    g_GameManager.cherry.offset = 777;
    g_GameManager.cherry.value = 777;
    g_GameManager.cherry.max = 777;
    g_GameManager.cherry.plus = 777;
}

TEST(summary_shows_cherry_and_its_maximum)
{
    M_Setup("summary");
    M_ShowSummary();
    g_GameManager.cherry.value = 777 + 12345;
    g_GameManager.cherry.max = 777 + 1000000;
    M_Draw();
    CHECK_EQ_INT(GfxRecorder_QuadCount(), 1 + 5 + 7 + 1);
    CHECK_EQ_INT(M_Sprite(0), 0x104);
    CHECK_EQ_INT(M_Sprite(1), 0x85);
    CHECK(M_X(1) == 149.0f && M_Y(1) == 207.0f);
    CHECK(M_X(5) == 177.0f);
    CHECK_EQ_INT(M_Color(1), 0xc0ffffffu);
    CHECK_EQ_INT(M_Sprite(6), 0x85);
    CHECK(M_X(6) == 193.0f);
    CHECK_EQ_INT(M_Color(6), 0xc0e0d0f0u);
    CHECK_EQ_INT(M_Sprite(13), 0x84);
    CHECK(M_X(13) == 177.0f && M_Y(13) == 198.0f);
    CHECK_EQ_INT(M_Color(13), 0xc0c080b0u);
    M_Teardown();
}

TEST(summary_colours_full_and_high_cherry)
{
    M_Setup("summary_full");
    M_ShowSummary();
    g_GameManager.cherry.value = 777 + 50000;
    g_GameManager.cherry.max = 777 + 60000;
    M_Draw();
    CHECK_EQ_INT(M_Color(1), 0xc080ffffu);
    M_Teardown();
    M_Setup("summary_max");
    M_ShowSummary();
    g_GameManager.cherry.value = 777 + 60000;
    g_GameManager.cherry.max = 777 + 60000;
    M_Draw();
    CHECK_EQ_INT(M_Color(1), 0xc080d0ffu);
    M_Teardown();
}

TEST(summary_pulses_during_the_border_with_the_bomb_icon)
{
    M_Setup("summary_border");
    M_ShowSummary();
    g_GameManager.cherry.plus = 777 + 27000;
    g_Player.border_state = PLAYER_BORDER_RUNNING;
    M_Draw();
    const int32_t count = GfxRecorder_QuadCount();
    CHECK_EQ_INT(count, 1 + 1 + 1 + 5 + 1);
    CHECK_EQ_INT(M_Sprite(3), 0x86);
    CHECK(M_X(3) == 149.0f && M_Y(3) == 194.0f);
    CHECK(M_X(4) == 159.0f);
    CHECK(GfxRecorder_Quad(3)->vertices[1].position.x == 161.0f);
    CHECK_EQ_INT(M_Color(3), 0xc0ff8787u);
    CHECK_EQ_INT(M_Sprite(count - 1), 0x105);
    CHECK(M_X(count - 1) == 120.0f && M_Y(count - 1) == 204.0f);
    M_Teardown();
}

TEST(summary_hides_the_bomb_icon_unless_the_border_runs)
{
    M_Setup("summary_border_pending");
    M_ShowSummary();
    g_Player.border_state = PLAYER_BORDER_PENDING;
    M_Draw();
    CHECK_EQ_INT(GfxRecorder_QuadCount(), 1 + 1 + 1 + 1);
    M_Teardown();
}

TEST(demo_icon_runs_only_in_a_demo)
{
    M_Setup("demo_icon");
    g_GameManager.mode.in_demo = true;
    Chain_RunCalc(&g_Chain);
    CHECK_EQ_INT(g_AsciiText.demo_icon.script_index, 7);
    M_Draw();
    CHECK_EQ_INT(GfxRecorder_QuadCount(), 1);
    CHECK_EQ_INT(M_Sprite(0), 0x107);
    g_GameManager.mode.in_demo = false;
    Chain_RunCalc(&g_Chain);
    CHECK_EQ_INT(g_AsciiText.demo_icon.script_index, 0);
    M_Teardown();
}
