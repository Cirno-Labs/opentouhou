#include "th06/ascii.h"

#include "gfx_recorder.h"
#include "test.h"

#include "th06/game.h"
#include "th06/game_manager.h"
#include "th06/scene.h"

#include "core/gfx/private.h"

#include <limits.h>
#include <string.h>

CHAIN g_Chain;
GAME_MANAGER g_GameManager;
BUTTONS g_Buttons;

static uint32_t m_PresentationGeneration;
static int32_t m_LoadCount;
static int32_t m_LoadSlots[3];
static int32_t m_LoadSprites[3];
static const char *m_LoadPaths[3];
static int32_t m_FailLoad;
static int32_t m_ReleaseCount;
static int32_t m_Releases[3];
static SCENE m_Requested;
static int32_t m_LaterCalls;
static CHAIN_ELEM m_LaterElem;

RESULT Anm_Load(
    const int32_t slot, const char *const path, const int32_t first_sprite)
{
    const int32_t index = m_LoadCount++;
    m_LoadSlots[index] = slot;
    m_LoadSprites[index] = first_sprite;
    m_LoadPaths[index] = path;
    return m_FailLoad == slot ? FAIL("test font load failure") : OK;
}

void Anm_Release(const int32_t slot)
{
    m_Releases[m_ReleaseCount++] = slot;
}

void Anm_RenewPresentation(ANM_VM *const vm)
{
    vm->presentation_generation = ++m_PresentationGeneration;
}

void Anm_InitVm(ANM_VM *const vm)
{
    vm->anchor = 0;
    vm->scale_x = 1.0f;
    vm->scale_y = 1.0f;
    vm->color = 0xffffffffu;
    vm->shown = true;
    Timer_Init(&vm->script_time);
}

void Anm_SetSprite(ANM_VM *const vm, const int32_t sprite)
{
    vm->sprite = (int16_t)sprite;
}

void Anm_StartScript(ANM_VM *const vm, const int32_t script)
{
    vm->script_index = (int16_t)script;
}

bool Anm_Step(ANM_VM *const vm)
{
    return false;
}

void Scene_Request(const SCENE scene)
{
    m_Requested = scene;
}

static void M_Setup(void)
{
    Chain_Release(&g_Chain);
    AsciiText_Initialize();
    Chain_Init(&g_Chain);
    g_GameManager = (GAME_MANAGER) {
        .play_area = { 32.0f, 16.0f, 384.0f, 448.0f },
    };
    g_Buttons = (BUTTONS) {};
    g_AsciiText.capture_background = false;
    g_AsciiText.highlight_font = false;
    m_LoadCount = 0;
    m_FailLoad = 0;
    m_ReleaseCount = 0;
    m_Requested = SCENE_NONE;
    m_LaterCalls = 0;
}

static void M_Register(void)
{
    CHECK(Result_Ignore(AsciiText_Register()));
}

static CHAIN_STATUS M_AfterReplayDeath(void *const arg)
{
    CHECK_EQ_INT(m_Requested, SCENE_REPLAY_END);
    CHECK(!g_GameManager.mode.in_continue_menu);
    CHECK_EQ_INT(g_GameManager.score.shown, 12345);
    m_LaterCalls++;
    return CHAIN_CONTINUE;
}

TEST(register_loads_the_fonts_and_capture_in_original_order)
{
    M_Setup();
    M_Register();
    CHECK_EQ_INT(m_LoadCount, 3);
    for (int32_t i = 0; i < 3; i++) {
        CHECK_EQ_INT(m_LoadSlots[i], i + 1);
    }
    CHECK_EQ_STR(m_LoadPaths[0], "data/ascii.anm");
    CHECK_EQ_STR(m_LoadPaths[1], "data/asciis.anm");
    CHECK_EQ_STR(m_LoadPaths[2], "data/capture.anm");
    CHECK_EQ_INT(m_LoadSprites[0], 0);
    CHECK_EQ_INT(m_LoadSprites[1], 0x77);
    CHECK_EQ_INT(m_LoadSprites[2], 0x718);
    CHECK_EQ_INT(g_Chain.calc.next->priority, 1);
    CHECK_EQ_INT(g_Chain.draw.next->priority, 10);
    CHECK_EQ_INT(g_Chain.draw.next->next->priority, 15);
    CHECK_EQ_INT(g_AsciiText.digit.anchor, 0);
    CHECK_EQ_INT(g_AsciiText.digit.sprite, 0);
    CHECK_EQ_INT(g_AsciiText.glyph.sprite, 0x20);
    CHECK(g_AsciiText.digit.pos.z == 0.1f);
}

TEST(removal_releases_fonts_and_leaves_the_second_draw_callback)
{
    M_Setup();
    M_Register();
    AsciiText_Remove();
    CHECK_EQ_INT(m_ReleaseCount, 3);
    CHECK_EQ_INT(m_Releases[0], 1);
    CHECK_EQ_INT(m_Releases[1], 2);
    CHECK_EQ_INT(m_Releases[2], 3);
    CHECK(g_Chain.calc.next == nullptr);
    CHECK(g_Chain.draw.next != nullptr);
    CHECK_EQ_INT(g_Chain.draw.next->priority, 10);
    CHECK(g_Chain.draw.next->next == nullptr);
    AsciiText_Remove();
    CHECK_EQ_INT(m_ReleaseCount, 3);
}

TEST(failed_font_registration_keeps_cleanup_available)
{
    M_Setup();
    m_FailLoad = 2;
    CHECK(!Result_Ignore(AsciiText_Register()));
    CHECK_EQ_INT(m_LoadCount, 2);
    CHECK(g_Chain.calc.next != nullptr);
    CHECK(g_Chain.draw.next == nullptr);
    AsciiText_Remove();
    CHECK_EQ_INT(m_ReleaseCount, 3);
}

TEST(strings_copy_utf8_and_capture_the_style_at_enqueue)
{
    M_Setup();
    M_Register();
    char text[] = "東方 text";
    g_AsciiText.scale_x = 2.0f;
    g_AsciiText.scale_y = 3.0f;
    g_AsciiText.in_arena = true;
    g_AsciiText.highlight_font = true;
    AsciiText_SetColor(0x12345678);
    AsciiText_SetHighlight(true);
    AsciiText_AddString((VEC3) { 5.0f, 6.0f, 7.0f }, text);
    text[0] = 'x';
    AsciiText_SetColor(0);
    AsciiText_SetHighlight(false);
    CHECK_EQ_STR(g_AsciiText.strings[0].text, "東方 text");
    CHECK_EQ_INT(g_AsciiText.strings[0].color, 0x12345678);
    CHECK(g_AsciiText.strings[0].scale_x == 2.0f);
    CHECK(g_AsciiText.strings[0].scale_y == 3.0f);
    CHECK(g_AsciiText.strings[0].highlight);
    CHECK(g_AsciiText.strings[0].in_arena);
    CHECK(g_AsciiText.strings[0].pos.z == 7.0f);
}

TEST(string_queue_is_bounded_and_draw_clears_it)
{
    M_Setup();
    M_Register();
    for (int32_t i = 0; i < ASCII_STRING_COUNT + 4; i++) {
        AsciiText_AddFormat((VEC3) {}, "line %d", i);
    }
    CHECK_EQ_INT(g_AsciiText.string_count, ASCII_STRING_COUNT);
    CHECK_EQ_STR(g_AsciiText.strings[255].text, "line 255");
    Chain_RunDraw(&g_Chain);
    CHECK_EQ_INT(g_AsciiText.string_count, 0);
    CHECK(g_AsciiText.strings[0].text == nullptr);
    AsciiText_AddFormat((VEC3) {}, "%080d", 3);
    CHECK_EQ_INT(strlen(g_AsciiText.strings[0].text), 80);
}

TEST(highlight_depends_on_the_original_presentation_flag)
{
    M_Setup();
    AsciiText_SetHighlight(true);
    AsciiText_AddString((VEC3) {}, "plain");
    CHECK(!g_AsciiText.strings[0].highlight);
    g_AsciiText.highlight_font = true;
    AsciiText_AddString((VEC3) {}, "highlight");
    CHECK(g_AsciiText.strings[1].highlight);
}

TEST(initialize_resets_contents_and_keeps_external_presentation_facts)
{
    M_Setup();
    g_AsciiText.capture_background = true;
    g_AsciiText.highlight_font = true;
    AsciiText_AddString((VEC3) {}, "queued");
    AsciiText_AddPopup((VEC3) {}, 45, 0);
    g_AsciiText.pause_menu.state = 6;
    AsciiText_Initialize();
    CHECK_EQ_INT(g_AsciiText.string_count, 0);
    CHECK_EQ_INT(g_AsciiText.pause_menu.state, 0);
    CHECK_EQ_INT(g_AsciiText.next_popup, 0);
    CHECK(!g_AsciiText.popups[0].active);
    CHECK(g_AsciiText.capture_background);
    CHECK(g_AsciiText.highlight_font);
    CHECK_EQ_INT(g_AsciiText.color, 0xffffffffu);
    CHECK(g_AsciiText.scale_x == 1.0f && g_AsciiText.scale_y == 1.0f);
}

TEST(popups_keep_decimal_digits_and_wrap_the_two_pools_independently)
{
    M_Setup();
    AsciiText_AddPopup((VEC3) {}, 1203, 0xabcdef01);
    CHECK_EQ_INT(g_AsciiText.popups[0].digit_count, 4);
    CHECK_EQ_INT(g_AsciiText.popups[0].digits[0], 3);
    CHECK_EQ_INT(g_AsciiText.popups[0].digits[1], 0);
    CHECK_EQ_INT(g_AsciiText.popups[0].digits[2], 2);
    CHECK_EQ_INT(g_AsciiText.popups[0].digits[3], 1);
    AsciiText_AddPopup((VEC3) {}, INT32_MIN, 0);
    CHECK_EQ_INT(g_AsciiText.popups[1].digit_count, 1);
    CHECK_EQ_INT(g_AsciiText.popups[1].digits[0], 10);
    AsciiText_AddPopup((VEC3) {}, 0, 0);
    CHECK_EQ_INT(g_AsciiText.popups[2].digits[0], 0);
    AsciiText_AddPopup((VEC3) {}, INT32_MAX, 0);
    CHECK_EQ_INT(g_AsciiText.popups[3].digit_count, 10);
    g_AsciiText.next_popup = ASCII_NORMAL_POPUP_COUNT;
    AsciiText_AddPopup((VEC3) {}, 9, 0);
    CHECK_EQ_INT(g_AsciiText.popups[0].digits[0], 9);
    CHECK_EQ_INT(g_AsciiText.next_popup, 1);
    for (int32_t i = 0; i < 4; i++) {
        AsciiText_AddPopup2((VEC3) {}, i, 0);
    }
    CHECK_EQ_INT(g_AsciiText.popups[512].digits[0], 3);
    CHECK_EQ_INT(g_AsciiText.next_popup2, 1);
    CHECK_EQ_INT(g_AsciiText.next_popup, 1);
}

TEST(popups_move_for_61_calcs_and_freeze_in_both_menus)
{
    M_Setup();
    M_Register();
    AsciiText_AddPopup((VEC3) { 4.0f, 50.0f, 0.0f }, 1, 0);
    for (int32_t i = 0; i < 60; i++) {
        Chain_RunCalc(&g_Chain);
    }
    CHECK(g_AsciiText.popups[0].active);
    CHECK(g_AsciiText.popups[0].pos.y == 20.0f);
    g_GameManager.mode.paused = 2;
    Chain_RunCalc(&g_Chain);
    CHECK_EQ_INT(g_AsciiText.popups[0].timer.current, 60);
    g_GameManager.mode.paused = 0;
    g_GameManager.mode.in_continue_menu = true;
    Chain_RunCalc(&g_Chain);
    CHECK_EQ_INT(g_AsciiText.popups[0].timer.current, 60);
    g_GameManager.mode.in_continue_menu = false;
    Chain_RunCalc(&g_Chain);
    CHECK(!g_AsciiText.popups[0].active);
    CHECK(g_AsciiText.popups[0].pos.y == 19.5f);
}

TEST(draw_keeps_string_layout_and_popup_position_state)
{
    M_Setup();
    M_Register();
    AsciiText_AddPopup((VEC3) { 40.0f, 25.0f, 3.0f }, 123, 0x55555555);
    g_AsciiText.scale_x = 2.0f;
    g_AsciiText.scale_y = 2.0f;
    AsciiText_AddString((VEC3) { 10.0f, 20.0f, 0.0f }, "A \nB");
    Chain_RunDraw(&g_Chain);
    CHECK(g_AsciiText.digit.pos.x == 52.0f);
    CHECK(g_AsciiText.digit.pos.y == 25.0f);
    CHECK(g_AsciiText.glyph.pos.x == 38.0f);
    CHECK(g_AsciiText.glyph.pos.y == 52.0f);
    CHECK_EQ_INT(g_AsciiText.glyph.sprite, 'B' - 0x15);
    CHECK_EQ_INT(g_AsciiText.glyph.anchor, 3);
    CHECK(g_GameManager.camera.viewport.width == 640.0f);
}

TEST(replay_death_routes_before_later_gameplay_callbacks)
{
    M_Setup();
    M_Register();
    g_GameManager.mode.in_replay = true;
    g_GameManager.mode.in_continue_menu = true;
    g_GameManager.score.value = 12345;
    g_GameManager.score.shown = 9000;
    Chain_InitElem(&m_LaterElem, M_AfterReplayDeath, nullptr);
    CHECK(Result_Ignore(Chain_AddCalc(&g_Chain, &m_LaterElem, 4)));
    Chain_RunCalc(&g_Chain);
    CHECK_EQ_INT(m_Requested, SCENE_REPLAY_END);
    CHECK(!g_GameManager.mode.in_continue_menu);
    CHECK_EQ_INT(g_GameManager.score.shown, 12345);
    CHECK_EQ_INT(m_LaterCalls, 1);
}

TEST(strings_record_the_atlas_glyphs_at_their_screen_positions)
{
    M_Setup();
    M_Register();
    GfxRecorder_Start();
    const ANM_SPRITE sprite = {
        .slot = 0, .width = 8.0f, .height = 16.0f, .u1 = 1.0f, .v1 = 1.0f
    };
    CHECK(Result_Ignore(Anm_CreateTexture(0, 8, 16, ANM_TEXTURE_RGBA8)));
    g_AsciiText.glyph.sprite_data = &sprite;
    g_AsciiText.glyph.visible = true;
    AsciiText_AddString((VEC3) { 10.0f, 20.0f, 0.0f }, "AB");
    Gfx_BeginTick((GFX_COLOR) {});
    Chain_RunDraw(&g_Chain);
    Gfx_EndTick();
    CHECK_EQ_INT(GfxRecorder_QuadCount(), 2);
    CHECK(GfxRecorder_Quad(0)->vertices[0].position.x == 10.0f);
    CHECK(GfxRecorder_Quad(0)->vertices[0].position.y == 20.0f);
    CHECK(GfxRecorder_Quad(1)->vertices[0].position.x == 24.0f);
    CHECK_EQ_INT(g_AsciiText.string_count, 0);
    Anm_ReleaseTexture(0);
    Chain_Release(&g_Chain);
    GfxRecorder_Stop();
}

TEST(point_popup_digits_keep_their_lifetime_across_shared_vm_updates)
{
    M_Setup();
    M_Register();
    GfxRecorder_Start();
    const ANM_SPRITE sprite = { .slot = 0, .width = 8, .height = 8 };
    CHECK(IS_OK(Anm_CreateTexture(0, 256, 256, ANM_TEXTURE_RGBA8)));
    g_AsciiText.digit.sprite_data = &sprite;
    g_AsciiText.digit.visible = true;
    g_AsciiText.digit.uv_scale_x = g_AsciiText.digit.uv_scale_y = 0.03125f;
    g_GameManager.camera.view = Mat4_Identity();
    g_GameManager.camera.projection = Mat4_Identity();
    AsciiText_AddPopup((VEC3) { 20, 30, 0 }, 12, 0xffffffff);
    AsciiText_AddPopup((VEC3) { 40, 30, 0 }, 12, 0xffffffff);
    Gfx_BeginTick((GFX_COLOR) {});
    Chain_RunDraw(&g_Chain);
    Gfx_EndTick();
    CHECK_EQ_INT(GfxRecorder_QuadCount(), 4);
    const VEC3 first = GfxRecorder_Quad(0)->vertices[0].position;
    const VEC3 second = GfxRecorder_Quad(2)->vertices[0].position;
    g_AsciiText.popups[0].pos.y += 10.0f;
    g_AsciiText.popups[1].pos.y += 10.0f;
    Anm_RenewPresentation(&g_AsciiText.digit);
    Gfx_BeginTick((GFX_COLOR) {});
    Chain_RunDraw(&g_Chain);
    Gfx_EndTick();
    CHECK(GfxRecorder_Quad(0)->vertices[0].position.y != first.y);
    CHECK(GfxRecorder_Blend(0.0f, 0)[0].position.y == first.y);
    CHECK(GfxRecorder_Blend(0.0f, 2)[0].position.x == second.x);
    CHECK(GfxRecorder_Blend(0.0f, 2)[0].position.y == second.y);
    g_AsciiText.next_popup = 0;
    AsciiText_AddPopup((VEC3) { 20, 0, 0 }, 12, 0xffffffff);
    Gfx_BeginTick((GFX_COLOR) {});
    Chain_RunDraw(&g_Chain);
    Gfx_EndTick();
    const float moved = GfxRecorder_Quad(0)->vertices[0].position.y;
    CHECK(GfxRecorder_Blend(0.0f, 0)[0].position.y == moved);
    Anm_ReleaseTexture(0);
    Chain_Release(&g_Chain);
    GfxRecorder_Stop();
}
