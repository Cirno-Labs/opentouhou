#include "th07/gui.h"
#include "th07/gui/private.h"

#include "gfx_recorder.h"
#include "patch_builder.h"
#include "test.h"
#include "th07_anm_builder.h"
#include "th07_gui_setup.h"
#include "th07_msg_builder.h"

#include "th07/anm.h"
#include "th07/ascii.h"
#include "th07/enemy.h"
#include "th07/files.h"
#include "th07/game.h"
#include "th07/game_manager.h"
#include "th07/scene.h"
#include "th07/sound.h"
#include "th07/strings.h"

#include "core/gfx.h"

#include "stg/timer.h"
#include "stg/translation.h"

#include <string.h>

#define M_SPRITE_OP 3
#define M_DELETE_OP 1
#define M_STOP_OP 20
#define M_LABEL_OP 21
#define M_TEXT_SLOT 0
#define M_BAR_APPEAR_FRAMES 100
#define M_BAR_LEAVE_FRAMES 100

// Adds the boss bar's script: it waits at a stop, runs for
// M_BAR_APPEAR_FRAMES at interrupt 1 and for M_BAR_LEAVE_FRAMES at
// interrupt 2.
static void M_AddBarScript(ANM_BUILDER_ANM *const anm, const int32_t id)
{
    const int32_t s = AnmBuilder_BeginScript(anm, id);
    const uint32_t first = 0;
    const uint32_t second = 1;
    const uint32_t appear = 1;
    const uint32_t leave = 2;
    const int16_t shown = M_BAR_APPEAR_FRAMES;
    const int16_t gone = M_BAR_APPEAR_FRAMES + M_BAR_LEAVE_FRAMES;
    AnmBuilder_Instr(anm, s, M_SPRITE_OP, 0, 0, &first, 1);
    AnmBuilder_Instr(anm, s, M_STOP_OP, 0, 0, nullptr, 0);
    AnmBuilder_Instr(anm, s, M_LABEL_OP, 0, 0, &appear, 1);
    AnmBuilder_Instr(anm, s, M_SPRITE_OP, 0, 0, &second, 1);
    AnmBuilder_Instr(anm, s, M_STOP_OP, shown, 0, nullptr, 0);
    AnmBuilder_Instr(anm, s, M_LABEL_OP, shown, 0, &leave, 1);
    AnmBuilder_Instr(anm, s, M_SPRITE_OP, shown, 0, &first, 1);
    AnmBuilder_Instr(anm, s, M_STOP_OP, gone, 0, nullptr, 0);
    AnmBuilder_End(anm, s);
}

// Adds a script that shows sprite 0 and ends after frames.
static void M_AddEndingScript(
    ANM_BUILDER_ANM *const anm, const int32_t id, const int16_t frames)
{
    const int32_t s = AnmBuilder_BeginScript(anm, id);
    const uint32_t first = 0;
    AnmBuilder_Instr(anm, s, M_SPRITE_OP, 0, 0, &first, 1);
    AnmBuilder_Instr(anm, s, M_DELETE_OP, frames, 0, nullptr, 0);
    AnmBuilder_End(anm, s);
}

// Writes the interface over the data of GuiSetup_WriteData: front.anm
// with every interface sprite, the boss bar and the banner names;
// loading.anm with the tiles, which end after 2 and 4 frames; the
// portraits and glows of the banners; and text.anm with the banner bars and
// the digits from 0x84, each 8 wide.
static void M_WriteInterface(void)
{
    static ANM_BUILDER builder;
    AnmBuilder_Init(&builder);
    ANM_BUILDER_ANM *anm = GuiSetup_AddAnm(&builder);
    for (int32_t i = 0; i < GUI_SPRITE_COUNT; i++) {
        if (i == M_BOSS_BAR) {
            M_AddBarScript(anm, i);
        } else {
            GuiSetup_AddScript(anm, i);
        }
    }
    for (int32_t i = 0x100; i <= 0x105; i++) {
        GuiSetup_AddScript(anm, i);
    }
    GuiSetup_WriteBuilder(&builder, "data/front.anm");

    AnmBuilder_Init(&builder);
    anm = GuiSetup_AddAnm(&builder);
    GuiSetup_AddScript(anm, 0);
    GuiSetup_AddScript(anm, M_SNAPSHOT_SCRIPT - M_LOADING_SCRIPT);
    M_AddEndingScript(anm, M_TILE_SCRIPT - M_LOADING_SCRIPT, 2);
    M_AddEndingScript(anm, M_TILE_SCRIPT + 1 - M_LOADING_SCRIPT, 4);
    GuiSetup_WriteBuilder(&builder, "data/loading.anm");

    AnmBuilder_Init(&builder);
    anm = GuiSetup_AddAnm(&builder);
    AnmBuilder_AddSprite(
        anm, M_GLOW_SPRITE - M_PLAYER_PORTRAIT, 64.0f, 0.0f, 32.0f, 32.0f);
    for (int32_t i = 0; i <= M_SPELL_FRAME_SCRIPT - M_PLAYER_PORTRAIT; i++) {
        GuiSetup_AddScript(anm, i);
    }
    GuiSetup_WriteBuilder(&builder, "data/face_rm00.anm");

    AnmBuilder_Init(&builder);
    anm = AnmBuilder_Add(&builder, 256, 64, "@");
    for (int32_t i = 0; i < 4; i++) {
        AnmBuilder_AddSprite(anm, i, 0.0f, 32.0f, 64.0f, 16.0f);
        GuiSetup_AddScript(anm, i);
    }
    for (int32_t i = 0; i < 10; i++) {
        AnmBuilder_AddSprite(
            anm, M_SCORE_DIGIT_SPRITE + i, (float)i * 8.0f, 0.0f, 8.0f, 16.0f);
    }
    GuiSetup_WriteBuilder(&builder, "data/text.anm");
}

// Writes the data of stage and the next one, with one dialogue script that
// halts, and starts the game state without the interface.
static void M_Open(const char *const case_name, const int32_t stage)
{
    MSG_BUILDER msg;
    MsgBuilder_Init(&msg);
    MsgBuilder_Op(&msg, MsgBuilder_BeginScript(&msg), 0, GUI_MSG_OP_HALT);
    GuiSetup_WriteData(case_name, stage, &msg);
    GuiSetup_WriteStage(stage + 1, &msg);
    M_WriteInterface();
    GuiSetup_Start(stage);
    CHECK(
        Result_Log(Anm_Load(M_TEXT_SLOT, "data/text.anm", 0), LOG_LEVEL_ERROR));
    g_GameManager.camera.viewport =
        (VIEWPORT) { 32.0f, 16.0f, 384.0f, 448.0f, 0.0f, 1.0f };
    AsciiText_Initialize();
}

static void M_SetUp(const char *const case_name, const int32_t stage)
{
    M_Open(case_name, stage);
    CHECK(Result_Log(Gui_Register(), LOG_LEVEL_ERROR));
}

static void M_TearDown(void)
{
    AsciiText_Initialize();
    GuiSetup_TearDown();
}

// Draws the interface into a new tick and a new text queue.
static void M_Draw(void)
{
    Gfx_BeginTick((GFX_COLOR) {});
    AsciiText_Initialize();
    CHECK(GuiDraw_Frame() == CHAIN_CONTINUE);
    Gfx_EndTick();
}

// Returns the first untextured quad drawn from x0, y0, or -1.
static int32_t M_FindFill(const float x0, const float y0)
{
    for (int32_t i = 0; i < GfxRecorder_QuadCount(); i++) {
        const GFX_RECORDED_QUAD *const quad = GfxRecorder_Quad(i);
        if (quad->texture == nullptr && quad->vertices[0].position.x == x0
            && quad->vertices[0].position.y == y0) {
            return i;
        }
    }
    return -1;
}

// Returns the queued text that is text, or nullptr.
static const ASCII_STRING *M_FindString(const char *const text)
{
    for (int32_t i = 0; i < g_AsciiText.string_count; i++) {
        if (strcmp(g_AsciiText.strings[i].text, text) == 0) {
            return &g_AsciiText.strings[i];
        }
    }
    return nullptr;
}

static int32_t M_StringIndex(const char *const text)
{
    const ASCII_STRING *const string = M_FindString(text);
    return string == nullptr ? -1 : (int32_t)(string - g_AsciiText.strings);
}

// Runs frames until the boss bar shows; returns how many it took.
static int32_t M_ShowBossBar(void)
{
    g_Gui.boss.present = true;
    int32_t frames = 0;
    while (g_Gui.hud.boss_bar != GUI_BOSS_BAR_SHOWN && frames < 1000) {
        GuiSetup_Run(1);
        frames++;
    }
    return frames;
}

TEST(the_boss_bar_fades_in_while_it_appears)
{
    M_SetUp("bar_appears", 1);
    g_Gui.boss.present = true;
    GuiSetup_Run(1);
    CHECK(g_Gui.hud.boss_bar == GUI_BOSS_BAR_APPEARING);
    CHECK_EQ_INT(g_Gui.hud.boss_bar_alpha, 0);
    for (uint32_t alpha = 4; alpha <= 0xfc; alpha += 4) {
        GuiSetup_Run(1);
        CHECK_EQ_INT(g_Gui.hud.boss_bar_alpha, alpha);
    }
    GuiSetup_Run(1);
    CHECK_EQ_INT(g_Gui.hud.boss_bar_alpha, 0xff);
    GuiSetup_Run(1);
    CHECK_EQ_INT(g_Gui.hud.boss_bar_alpha, 0xff);
    // It shows once its script stops.
    CHECK(g_Gui.hud.boss_bar == GUI_BOSS_BAR_APPEARING);
    CHECK_EQ_INT(M_ShowBossBar(), M_BAR_APPEAR_FRAMES + 2 - 66);
    M_TearDown();
}

TEST(a_shown_boss_bar_hides_on_the_frame_it_starts_to_leave)
{
    M_SetUp("bar_hides", 1);
    g_Gui.boss.health = 0.5f;
    M_ShowBossBar();
    GuiSetup_Run(5);
    CHECK(g_Gui.hud.boss_health_shown > 0.0f);
    g_Gui.boss.present = false;
    GuiSetup_Run(1);
    CHECK(g_Gui.hud.boss_bar == GUI_BOSS_BAR_HIDDEN);
    CHECK_EQ_INT(g_Gui.hud.boss_bar_alpha, 0);
    CHECK(g_Gui.hud.boss_health_shown == 0.0f);
    // The leave interrupt still runs the bar's script.
    CHECK(!g_Gui.hud.sprites[M_BOSS_BAR].stopped);
    M_TearDown();
}

TEST(a_boss_bar_leaving_from_full_alpha_wraps_below_zero)
{
    M_SetUp("bar_wraps", 1);
    g_Gui.boss.present = true;
    GuiSetup_Run(70);
    CHECK_EQ_INT(g_Gui.hud.boss_bar_alpha, 0xff);
    CHECK(g_Gui.hud.boss_bar == GUI_BOSS_BAR_APPEARING);
    g_Gui.boss.present = false;
    GuiSetup_Run(1);
    CHECK(g_Gui.hud.boss_bar == GUI_BOSS_BAR_LEAVING);
    CHECK_EQ_INT(g_Gui.hud.boss_bar_alpha, 0xfb);
    GuiSetup_Run(62);
    CHECK_EQ_INT(g_Gui.hud.boss_bar_alpha, 3);
    GuiSetup_Run(1);
    CHECK(g_Gui.hud.boss_bar_alpha == 0xffffffffu);
    GuiSetup_Run(1);
    CHECK(g_Gui.hud.boss_bar_alpha == 0xfffffffbu);
    // The leaving bar draws at the low byte of the wrapped alpha.
    M_Draw();
    const int32_t bar = M_FindFill(64.0f, 19.0f);
    CHECK(bar >= 0);
    CHECK_EQ_INT(GfxRecorder_Quad(bar)->vertices[0].color.alpha, 0xfb);
    // It hides once the script of its leaving stops.
    int32_t frames = 0;
    while (g_Gui.hud.boss_bar == GUI_BOSS_BAR_LEAVING && frames < 1000) {
        GuiSetup_Run(1);
        frames++;
    }
    CHECK(g_Gui.hud.boss_bar == GUI_BOSS_BAR_HIDDEN);
    CHECK_EQ_INT(g_Gui.hud.boss_bar_alpha, 0);
    M_TearDown();
}

TEST(the_shown_health_follows_the_boss)
{
    M_SetUp("bar_health", 1);
    g_Gui.boss.health = 0.035f;
    M_ShowBossBar();
    float expected = 0.01f;
    CHECK(g_Gui.hud.boss_health_shown == expected);
    GuiSetup_Run(1);
    expected += 0.01f;
    CHECK(g_Gui.hud.boss_health_shown == expected);
    GuiSetup_Run(2);
    CHECK(g_Gui.hud.boss_health_shown == 0.035f);
    g_Gui.boss.health = 0.0f;
    GuiSetup_Run(1);
    expected = 0.035f - 0.02f;
    CHECK(g_Gui.hud.boss_health_shown == expected);
    GuiSetup_Run(1);
    CHECK(g_Gui.hud.boss_health_shown == 0.0f);
    // It does not move while the bar appears.
    M_TearDown();
    M_SetUp("bar_health_appearing", 1);
    g_Gui.boss.health = 0.5f;
    g_Gui.boss.present = true;
    GuiSetup_Run(10);
    CHECK(g_Gui.hud.boss_health_shown == 0.0f);
    M_TearDown();
}

static void M_SetSection(
    const int32_t index, const float start, const float end,
    const int32_t color)
{
    g_Gui.boss.sections[index].start = start;
    g_Gui.boss.sections[index].end = end;
    g_Gui.boss.sections[index].color = color;
}

TEST(the_boss_bar_draws_its_sections_up_to_the_shown_health)
{
    M_SetUp("bar_sections", 1);
    M_ShowBossBar();
    GuiSetup_Run(70);
    g_Gui.hud.boss_health_shown = 0.75f;
    g_Gui.boss.health = 0.75f;
    M_SetSection(0, 0.0f, 0.125f, 0x00ffffff);
    M_SetSection(1, 0.5f, 0.25f, 0x00ff8040);
    M_SetSection(2, 0.875f, 0.625f, 0x0040ff80);
    M_SetSection(3, 0.9375f, 0.8125f, 0x00ffffff);
    M_Draw();
    const int32_t bar = M_FindFill(64.0f, 19.0f);
    CHECK(bar >= 0);
    CHECK(GfxRecorder_Quad(bar)->vertices[1].position.x == 304.0f);
    // A section at start 0 is unused.
    CHECK_EQ_INT(M_FindFill(0.125f * 320.0f + 64.0f, 19.0f), -1);
    const int32_t first = M_FindFill(144.0f, 19.0f);
    CHECK(first > bar);
    const GFX_VERTEX *const v = GfxRecorder_Quad(first)->vertices;
    CHECK(v[1].position.x == 224.0f);
    CHECK(v[2].position.y == 23.0f);
    CHECK_EQ_INT(v[0].color.red, 0xff);
    CHECK_EQ_INT(v[0].color.green, 0x80);
    CHECK_EQ_INT(v[0].color.blue, 0x40);
    CHECK_EQ_INT(v[0].color.alpha, 0xff);
    CHECK_EQ_INT(v[3].color.red, 0x3f);
    CHECK_EQ_INT(v[3].color.green, 0x20);
    CHECK_EQ_INT(v[3].color.blue, 0x10);
    // One past the shown health stops there.
    const int32_t second = M_FindFill(0.625f * 320.0f + 64.0f, 19.0f);
    CHECK(second > first);
    CHECK(GfxRecorder_Quad(second)->vertices[1].position.x == 304.0f);
    // One that ends past it is not drawn.
    CHECK_EQ_INT(M_FindFill(0.8125f * 320.0f + 64.0f, 19.0f), -1);
    // A dialogue hides the bar.
    Gui_StartMsg(0);
    M_Draw();
    CHECK_EQ_INT(M_FindFill(64.0f, 19.0f), -1);
    M_TearDown();
}

TEST(the_boss_timer_counts_down_with_a_sound_below_10)
{
    M_SetUp("bar_timer", 1);
    M_ShowBossBar();
    g_Gui.boss.seconds = 123;
    M_Draw();
    CHECK(M_FindString("99") != nullptr);
    CHECK_EQ_INT(g_SoundEvents.queue[0], -1);
    g_Gui.boss.seconds = 9;
    M_Draw();
    CHECK(M_FindString("09") != nullptr);
    CHECK_EQ_INT(g_SoundEvents.queue[0], SOUND_TIMEOUT);
    Sound_Reset();
    M_Draw();
    CHECK_EQ_INT(g_SoundEvents.queue[0], -1);
    M_TearDown();
}

TEST(the_bomb_name_shows_its_portrait_and_bar)
{
    M_SetUp("bomb_name", 1);
    Gui_ShowBombName(M_PLAYER_PORTRAIT + 2, "Fantasy Seal");
    CHECK(
        g_Gui.banners.bomb_portrait.script
        == Anm_GetScript(M_BOMB_PORTRAIT_SCRIPT));
    CHECK_EQ_INT(g_Gui.banners.bomb_portrait.sprite, M_PLAYER_PORTRAIT + 2);
    CHECK(g_Gui.banners.bomb_glow.script == Anm_GetScript(M_BOMB_GLOW_SCRIPT));
    CHECK_EQ_INT(g_Gui.banners.bomb_glow.sprite, M_GLOW_SPRITE);
    CHECK(
        g_Gui.banners.bomb_frame.script == Anm_GetScript(M_BOMB_FRAME_SCRIPT));
    CHECK_EQ_INT(g_Gui.banners.bomb_frame.sprite, M_GLOW_SPRITE);
    CHECK(g_Gui.banners.bomb_name.script == Anm_GetScript(M_BOMB_NAME_SCRIPT));
    CHECK_EQ_INT(g_Gui.banners.bomb_bar.pending_interrupt, 1);
    CHECK_EQ_INT(g_SoundEvents.queue[0], SOUND_CARD);
    Gui_EndBombName();
    CHECK_EQ_INT(g_Gui.banners.bomb_name.pending_interrupt, 1);
    CHECK_EQ_INT(g_Gui.banners.bomb_bar.pending_interrupt, 2);
    M_TearDown();
}

TEST(the_spell_card_name_places_its_portrait_by_width)
{
    static const struct {
        int32_t sprite;
        float offset;
    } cases[] = { { 0, 0.0f }, { 2, -112.0f }, { 3, -288.0f } };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        M_SetUp("spell_name", 1);
        Gui_ShowSpellCardName(cases[i].sprite, "Moon Sign");
        const ANM_VM *const portrait = &g_Gui.banners.spell_portrait;
        CHECK(portrait->script == Anm_GetScript(M_SPELL_PORTRAIT_SCRIPT));
        CHECK_EQ_INT(
            portrait->sprite, M_BOSS_PORTRAIT_SPRITE + cases[i].sprite);
        CHECK(portrait->pos_offset.x == cases[i].offset);
        CHECK_EQ_INT(g_Gui.banners.spell_glow.sprite, M_GLOW_SPRITE);
        CHECK_EQ_INT(g_Gui.banners.spell_frame.sprite, M_GLOW_SPRITE);
        CHECK(
            g_Gui.banners.spell_name.script
            == Anm_GetScript(M_SPELL_NAME_SCRIPT));
        CHECK_EQ_INT(g_Gui.banners.spell_bar.pending_interrupt, 1);
        CHECK_EQ_INT(g_Gui.banners.spell_score_bar.pending_interrupt, 1);
        CHECK_EQ_INT(g_SoundEvents.queue[0], SOUND_CARD);
        M_TearDown();
    }
    // Without a sprite, the portrait stays as it was.
    M_SetUp("spell_name_no_portrait", 1);
    Gui_ShowSpellCardName(-1, "Moon Sign");
    CHECK(g_Gui.banners.spell_portrait.script == nullptr);
    CHECK(
        g_Gui.banners.spell_glow.script == Anm_GetScript(M_SPELL_GLOW_SCRIPT));
    Gui_EndSpellCardName();
    CHECK_EQ_INT(g_Gui.banners.spell_name.pending_interrupt, 1);
    CHECK_EQ_INT(g_Gui.banners.spell_bar.pending_interrupt, 2);
    CHECK_EQ_INT(g_Gui.banners.spell_score_bar.pending_interrupt, 2);
    M_TearDown();
}

TEST(the_spell_score_draws_its_digits_by_sprite_data)
{
    M_SetUp("spell_score", 1);
    Gui_ShowSpellCardName(-1, "Moon Sign");
    GuiSetup_Run(1);
    CHECK(g_Gui.banners.spell_name.shown);
    g_GuiFake.spell_captures = 12;
    g_GuiFake.spell_attempts = 5;
    g_Gui.banners.spell_score_bar.pos = (VEC3) { 200.0f, 100.0f, 0.0f };
    ANM_VM *const digit = &g_Gui.banners.spell_score_digit;
    const int32_t sprite = digit->sprite;
    // Counts from the second draw: the first leaves the viewport full.
    M_Draw();
    M_Draw();
    const int32_t without = GfxRecorder_QuadCount();
    // The digits swap only the sprite data, from 0x84.
    CHECK(digit->sprite_data == Anm_GetSpriteEntry(M_SCORE_DIGIT_SPRITE + 5));
    CHECK_EQ_INT(digit->sprite, sprite);
    // 1234567, then 12 and 5: six digits more than a 0.
    g_Enemies.spell_card.capturable = true;
    g_Enemies.spell_card.bonus = 1234000;
    g_Enemies.spell_card.graze_bonus = 567;
    M_Draw();
    CHECK_EQ_INT(GfxRecorder_QuadCount(), without + 6);
    CHECK_EQ_INT(digit->sprite, sprite);
    // A count of 99 or more shows 99.
    g_GuiFake.spell_attempts = 150;
    M_Draw();
    CHECK(digit->sprite_data == Anm_GetSpriteEntry(M_SCORE_DIGIT_SPRITE + 9));
    CHECK_EQ_INT(GfxRecorder_QuadCount(), without + 7);
    M_TearDown();
}

TEST(a_notice_shows_for_180_frames)
{
    M_SetUp("notice_frames", 1);
    Gui_ShowFullPowerPopup();
    CHECK(g_Gui.popups.notice.shown);
    CHECK(g_Gui.popups.notice_kind == GUI_NOTICE_FULL_POWER);
    CHECK(g_Gui.popups.notice.pos.y == 168.0f);
    GuiSetup_Run(M_NOTICE_POPUP_FRAMES);
    CHECK(g_Gui.popups.notice.shown);
    CHECK(g_Gui.popups.notice.pos.x == 104.0f);
    GuiSetup_Run(1);
    CHECK(!g_Gui.popups.notice.shown);
    M_TearDown();
}

TEST(a_later_notice_replaces_an_earlier_one)
{
    M_SetUp("notice_replaced", 1);
    Gui_ShowFullPowerPopup();
    GuiSetup_Run(50);
    Gui_ShowBorderBonusPopup(1234);
    CHECK(g_Gui.popups.notice_kind == GUI_NOTICE_BORDER_BONUS);
    CHECK_EQ_INT(g_Gui.popups.notice.value, 1234);
    CHECK_EQ_INT(g_Gui.popups.notice.timer.current, 0);
    CHECK(g_Gui.popups.notice.pos.x == 416.0f);
    GuiSetup_Run(M_NOTICE_POPUP_FRAMES);
    CHECK(g_Gui.popups.notice.shown);
    GuiSetup_Run(1);
    CHECK(!g_Gui.popups.notice.shown);
    M_TearDown();
}

TEST(a_notice_slides_in_at_the_game_speed)
{
    M_SetUp("notice_slide", 1);
    g_GameSpeed = 0.5f;
    Gui_ShowBorderStartPopup();
    GuiSetup_Run(1);
    CHECK(g_Gui.popups.notice.pos.x == 416.0f);
    GuiSetup_Run(1);
    CHECK(g_Gui.popups.notice.pos.x == 0.5f * -312.0f / 30.0f + 416.0f);
    GuiSetup_Run(1);
    CHECK(g_Gui.popups.notice.pos.x == 1.0f * -312.0f / 30.0f + 416.0f);
    GuiSetup_Run(58);
    CHECK(g_Gui.popups.notice.pos.x == 104.0f);
    // At half speed it shows for twice the frames.
    GuiSetup_Run(2 * M_NOTICE_POPUP_FRAMES - 61);
    CHECK(g_Gui.popups.notice.shown);
    GuiSetup_Run(1);
    CHECK(!g_Gui.popups.notice.shown);
    M_TearDown();
}

TEST(each_notice_draws_its_text)
{
    M_SetUp("notice_text", 1);
    Gui_ShowFullPowerPopup();
    M_Draw();
    const ASCII_STRING *text = M_FindString("Full Power Mode!");
    CHECK(text != nullptr);
    CHECK(text->pos.x == 416.0f);
    CHECK(text->in_arena);
    CHECK(text->scale_y == 1.0f);
    Gui_ShowCherryFullPopup(50000);
    M_Draw();
    CHECK(M_FindString("CherryPoint Max!") != nullptr);
    CHECK(M_FindString("Full Power Mode!") == nullptr);
    Gui_ShowBorderStartPopup();
    M_Draw();
    text = M_FindString("Supernatural Border!!");
    CHECK(text != nullptr);
    CHECK(text->scale_y == 0.9f);
    CHECK(g_AsciiText.glyph_width == 14);
    Gui_ShowBorderBonusPopup(1234);
    M_Draw();
    CHECK(M_FindString("Border Bonus    1234") != nullptr);
    M_TearDown();
}

TEST(the_bonus_popups_show_for_their_frames)
{
    M_SetUp("bonus_popups", 1);
    Gui_ShowBonusPopup(500000);
    Gui_ShowSpellCardBonus(12345);
    CHECK(g_Gui.popups.bonus.pos.y == 48.0f);
    M_Draw();
    CHECK(M_FindString("BONUS   500000") != nullptr);
    CHECK(M_FindString("Spell Card Bonus!") != nullptr);
    const ASCII_STRING *const bonus = M_FindString("+12345");
    CHECK(bonus != nullptr);
    CHECK(bonus->scale_x == 2.0f);
    CHECK(bonus->pos.x == (384.0f - 6.0f * 32.0f) / 2.0f + 32.0f);
    CHECK(bonus->pos.y == 96.0f);
    const float spell_bonus_x = bonus->pos.x;
    GuiSetup_Run(M_BONUS_POPUP_FRAMES);
    CHECK(g_Gui.popups.bonus.shown);
    CHECK(g_Gui.popups.bonus.pos.x == 104.0f);
    GuiSetup_Run(1);
    CHECK(!g_Gui.popups.bonus.shown);
    // The spell card bonus does not slide.
    CHECK(g_Gui.popups.spell_bonus.pos.x == spell_bonus_x);
    CHECK(g_Gui.popups.spell_bonus.pos.y == 96.0f);
    GuiSetup_Run(M_SPELL_BONUS_POPUP_FRAMES - M_BONUS_POPUP_FRAMES - 1);
    CHECK(g_Gui.popups.spell_bonus.shown);
    GuiSetup_Run(1);
    CHECK(!g_Gui.popups.spell_bonus.shown);
    M_TearDown();
}

// Clears stage on difficulty with start_lives and returns the total, in
// tens. Stage 3 gives 985003 on Normal before the penalty.
static int32_t M_ClearStage(
    const char *const case_name, const int32_t stage, const int32_t difficulty,
    const uint8_t start_lives)
{
    M_SetUp(case_name, stage);
    g_GameManager.run.difficulty = difficulty;
    g_GameManager.run.start_lives = start_lives;
    g_GameManager.player.lives = 2.0f;
    g_GameManager.player.bombs = 3.0f;
    g_Gui.stage_clear.state = GUI_STAGE_CLEAR_PENDING;
    g_Gui.stage_clear.graze = 300;
    g_Gui.stage_clear.point_items = 120;
    g_Gui.stage_clear.cherry = 70003;
    GuiSetup_Run(1);
    CHECK(g_Gui.stage_clear.state == GUI_STAGE_CLEAR_SHOWN);
    return g_Gui.stage_clear.total;
}

TEST(the_stage_clear_bonus_scales_by_difficulty)
{
    static const int32_t totals[] = {
        492501, 985003, 1182003, 1477504, 1970006, 1970006,
    };
    for (int32_t i = 0; i < 6; i++) {
        CHECK_EQ_INT(M_ClearStage("clear_difficulty", 3, i, 2), totals[i]);
        M_TearDown();
    }
}

TEST(the_stage_clear_bonus_adds_ten_times_a_tenth)
{
    CHECK_EQ_INT(M_ClearStage("clear_score", 3, 1, 2), 985003);
    CHECK_EQ_INT(g_GameManager.score.value, 985000);
    // Only one update adds it.
    GuiSetup_Run(1);
    CHECK_EQ_INT(g_GameManager.score.value, 985000);
    M_TearDown();
}

TEST(the_stage_clear_bonus_wraps_as_32_bits)
{
    M_SetUp("clear_wrap", 1);
    g_GameManager.run.difficulty = 3;
    g_Gui.stage_clear.state = GUI_STAGE_CLEAR_PENDING;
    g_Gui.stage_clear.graze = 3000000;
    GuiSetup_Run(1);
    // 150100000 * 15 wraps to -2043467296.
    CHECK_EQ_INT(g_Gui.stage_clear.total, -204346729);
    M_TearDown();
}

TEST(the_start_lives_penalty_applies_on_every_difficulty)
{
    CHECK_EQ_INT(M_ClearStage("penalty", 3, 1, 3), 492501);
    M_TearDown();
    CHECK_EQ_INT(M_ClearStage("penalty", 3, 1, 4), 197000);
    M_TearDown();
    CHECK_EQ_INT(M_ClearStage("penalty", 3, 4, 3), 985003);
    M_TearDown();
    M_Open("penalty_practice", 3);
    g_GameManager.mode.in_practice = true;
    CHECK(Result_Log(Gui_Register(), LOG_LEVEL_ERROR));
    g_GameManager.run.start_lives = 4;
    g_Gui.stage_clear.state = GUI_STAGE_CLEAR_PENDING;
    g_Gui.stage_clear.cherry = 1000;
    GuiSetup_Run(1);
    CHECK_EQ_INT(g_Gui.stage_clear.total, (300000 + 1000) * 2 / 10);
    M_TearDown();
}

TEST(the_penalty_text_shows_below_extra_outside_practice)
{
    static const struct {
        int32_t difficulty;
        uint8_t start_lives;
        bool practice;
        const char *text;
    } cases[] = {
        { 0, 3, false, "Player Penalty*0.5" },
        { 3, 4, false, "Player Penalty*0.2" },
        { 1, 2, false, nullptr },
        { 4, 3, false, nullptr },
        { 1, 3, true, nullptr },
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        M_ClearStage(
            "penalty_text", 3, cases[i].difficulty, cases[i].start_lives);
        g_GameManager.mode.in_practice = cases[i].practice;
        M_Draw();
        CHECK(M_FindString("Stage Clear") != nullptr);
        const bool penalty = M_FindString("Player Penalty*0.5") != nullptr
            || M_FindString("Player Penalty*0.2") != nullptr;
        CHECK(penalty == (cases[i].text != nullptr));
        if (cases[i].text != nullptr) {
            CHECK(M_FindString(cases[i].text) != nullptr);
        }
        M_TearDown();
    }
}

TEST(the_last_stages_count_the_lives_and_the_bombs)
{
    static const struct {
        int32_t stage;
        bool practice, replay;
        int32_t replay_stage;
        bool counts;
    } cases[] = {
        { 5, false, false, 0, false }, { 6, false, false, 0, true },
        { 6, true, false, 0, false },  { 6, false, true, 0, false },
        { 6, false, true, 5, true },   { 6, false, true, 6, false },
        { 7, true, false, 0, true },   { 8, false, true, 0, true },
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        M_Open("lives_and_bombs", cases[i].stage);
        CHECK(Result_Log(Gui_Register(), LOG_LEVEL_ERROR));
        g_GameManager.mode.in_practice = cases[i].practice;
        g_GameManager.mode.in_replay = cases[i].replay;
        g_GuiFake.replay_stages[cases[i].replay_stage] = true;
        CHECK(GuiStageClear_CountsLivesAndBombs() == cases[i].counts);
        g_GameManager.player.lives = 2.0f;
        g_GameManager.player.bombs = 3.0f;
        g_Gui.stage_clear.state = GUI_STAGE_CLEAR_PENDING;
        GuiSetup_Run(1);
        const int32_t clear = cases[i].stage * 100000;
        CHECK_EQ_INT(
            g_Gui.stage_clear.total,
            cases[i].counts ? clear + 4000000 + 1200000 : clear);
        M_Draw();
        CHECK(
            (M_FindString("Player = 40000000") != nullptr) == cases[i].counts);
        CHECK(
            (M_FindString("Bomb   = 12000000") != nullptr) == cases[i].counts);
        M_TearDown();
    }
}

// Registers stage 1, then stage 2 in the scene that keeps the interface.
static void M_GoToNextStage(const char *const case_name, const SCENE scene)
{
    M_SetUp(case_name, 1);
    g_GuiFake.scene = scene;
    Gui_Remove();
    g_GameManager.run.stage = 2;
    CHECK(Result_Log(Gui_Register(), LOG_LEVEL_ERROR));
}

TEST(the_next_stage_lays_the_tiles_out)
{
    M_GoToNextStage("tiles_laid_out", SCENE_NEXT_STAGE);
    CHECK_EQ_INT(g_Gui.loading.tiles_running, GUI_TILE_COUNT);
    for (int32_t y = 0; y < GUI_TILE_ROWS; y++) {
        for (int32_t x = 0; x < GUI_TILE_COLUMNS; x++) {
            const ANM_VM *const tile =
                &g_Gui.loading.tiles[y * GUI_TILE_COLUMNS + x];
            CHECK(tile->script == Anm_GetScript(M_TILE_SCRIPT + ((x + y) & 1)));
            CHECK_EQ_INT(tile->int_vars[4], y + x * 2);
            CHECK(tile->pos.x == (float)x * 32.0f - 0.5f + 16.0f);
            CHECK(tile->pos.y == (float)y * 32.0f - 0.5f + 16.0f);
            CHECK(tile->uv_scroll_x == (float)x * 32.0f / 512.0f);
            CHECK(tile->uv_scroll_y == (float)y * 32.0f / 512.0f);
        }
    }
    CHECK(g_Gui.loading.snapshot.script == Anm_GetScript(M_SNAPSHOT_SCRIPT));
    CHECK_EQ_INT(g_Gui.loading.snapshot.pending_interrupt, 1);
    CHECK_EQ_INT(g_Gui.loading.icon.sprite, -1);
    // The snapshot's copy is pending.
    CHECK(!Anm_SetPendingCopy(
        M_SNAPSHOT_TEXTURE_SLOT, (GFX_RECT) {}, (GFX_RECT) {}));
    M_TearDown();
}

TEST(the_tiles_run_until_their_scripts_end)
{
    M_GoToNextStage("tiles_running", SCENE_NEXT_STAGE);
    GuiSetup_Run(1);
    CHECK_EQ_INT(g_Gui.loading.tiles_running, GUI_TILE_COUNT);
    GuiSetup_Run(1);
    CHECK_EQ_INT(g_Gui.loading.tiles_running, GUI_TILE_COUNT / 2);
    GuiSetup_Run(2);
    CHECK_EQ_INT(g_Gui.loading.tiles_running, 0);
    GuiSetup_Run(1);
    CHECK_EQ_INT(g_Gui.loading.tiles_running, 0);
    M_TearDown();
}

TEST(a_gameplay_register_resets_the_interface)
{
    M_Open("register_resets", 1);
    g_Gui.stage_clear.total = 9;
    g_Gui.popups.notice.shown = true;
    CHECK(Result_Log(Gui_Register(), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(g_Gui.stage_clear.total, 0);
    CHECK(!g_Gui.popups.notice.shown);
    for (int32_t i = 0; i < GUI_SPRITE_COUNT; i++) {
        CHECK(g_Gui.hud.sprites[i].script == Anm_GetScript(M_FRONT_SCRIPT + i));
    }
    CHECK_EQ_INT(g_Gui.loading.tiles_running, 0);
    M_TearDown();
}

TEST(the_three_keep_scenes_keep_the_interface)
{
    static const SCENE scenes[] = {
        SCENE_NEXT_STAGE,
        SCENE_RESTART_STAGE,
        SCENE_NEXT_STAGE_AGAIN,
    };
    for (size_t i = 0; i < sizeof(scenes) / sizeof(scenes[0]); i++) {
        M_SetUp("keep_scenes", 1);
        Gui_StartMsg(0);
        g_Gui.stage_clear.total = 9;
        g_Gui.hud.sprites[0].pos.x = 7.0f;
        g_GuiFake.scene = scenes[i];
        Gui_Remove();
        CHECK(Anm_GetScript(M_FRONT_SCRIPT) != nullptr);
        CHECK(Anm_GetScript(M_LOADING_SCRIPT) != nullptr);
        CHECK(Anm_GetScript(M_PLAYER_PORTRAIT) != nullptr);
        CHECK(Anm_GetScript(M_TITLE_SCRIPT) == nullptr);
        CHECK_EQ_INT(g_Gui.msg.script, 0);
        g_GameManager.run.stage = 2;
        CHECK(Result_Log(Gui_Register(), LOG_LEVEL_ERROR));
        CHECK_EQ_INT(g_Gui.stage_clear.total, 9);
        CHECK(g_Gui.hud.sprites[0].pos.x == 7.0f);
        CHECK(Anm_GetScript(M_TITLE_SCRIPT) != nullptr);
        CHECK_EQ_INT(g_Gui.msg.script, GUI_MSG_NONE);
        M_TearDown();
    }
}

TEST(other_scenes_release_the_interface)
{
    static const SCENE scenes[] = {
        SCENE_GAME_RESULT,
        SCENE_TITLE,
        SCENE_GAMEPLAY,
    };
    for (size_t i = 0; i < sizeof(scenes) / sizeof(scenes[0]); i++) {
        M_SetUp("release_scenes", 1);
        Gui_StartMsg(0);
        g_GuiFake.scene = scenes[i];
        Gui_Remove();
        CHECK(Anm_GetScript(M_FRONT_SCRIPT) == nullptr);
        CHECK(Anm_GetScript(M_LOADING_SCRIPT) == nullptr);
        CHECK(Anm_GetScript(M_PLAYER_PORTRAIT) == nullptr);
        CHECK(Anm_GetScript(M_TITLE_SCRIPT) == nullptr);
        CHECK_EQ_INT(g_Gui.msg.script, GUI_MSG_NONE);
        M_TearDown();
    }
}

TEST(the_draw_ignores_the_shake)
{
    M_SetUp("draw_shake", 1);
    M_Draw();
    const GFX_VERTEX first = GfxRecorder_Quad(0)->vertices[0];
    Anm_SetShake(5.0f, 3.0f);
    M_Draw();
    CHECK(GfxRecorder_Quad(0)->vertices[0].position.x == first.position.x);
    CHECK(GfxRecorder_Quad(0)->vertices[0].position.y == first.position.y);
    M_TearDown();
}

TEST(the_draw_runs_from_the_stage_clear_to_the_popups)
{
    M_SetUp("draw_order", 1);
    g_GameManager.score.shown = 123456;
    g_Gui.stage_clear.state = GUI_STAGE_CLEAR_SHOWN;
    Gui_ShowBonusPopup(500000);
    Gui_StartMsg(0);
    Timer_Set(&g_Gui.msg.timer, M_BOX_OPEN_FRAMES);
    M_Draw();
    const int32_t clear = M_StringIndex("Stage Clear");
    const int32_t score = M_StringIndex("00123456");
    const int32_t bonus = M_StringIndex("BONUS   500000");
    CHECK(clear >= 0);
    CHECK(score > clear);
    CHECK(bonus > score);
    // The dialogue box comes before the frame of the interface, whose
    // first sprite is centered on the top left.
    const int32_t box = M_FindFill(32.0f + 16.0f, 384.0f);
    CHECK(box >= 0);
    const GFX_TEXTURE *const front = Anm_GetTexture(M_FRONT_SLOT);
    int32_t frame = -1;
    for (int32_t i = 0; i < GfxRecorder_QuadCount() && frame < 0; i++) {
        const GFX_RECORDED_QUAD *const quad = GfxRecorder_Quad(i);
        if (quad->texture == front && quad->vertices[0].position.x == -8.0f
            && quad->vertices[0].position.y == -8.0f) {
            frame = i;
        }
    }
    CHECK(frame > box);
    CHECK(GfxRecorder_Quad(box)->vertices[2].position.y == 384.0f + 48.0f);
    const GFX_RECORDED_QUAD *const gradient = GfxRecorder_Quad(box);
    const VEC3 projected = Vec3_TransformCoord(
        gradient->vertices[0].position, &gradient->view.projection);
    CHECK(projected.x > 48.49f && projected.x < 48.51f);
    CHECK(projected.y > 384.49f && projected.y < 384.51f);
    M_TearDown();
}

TEST(only_the_boss_portrait_moves_by_its_offset)
{
    M_SetUp("draw_portraits", 1);
    Gui_StartMsg(0);
    ANM_VM *const portraits = g_Gui.msg.portraits;
    Anm_StartScript(&portraits[0], M_PLAYER_PORTRAIT);
    Anm_StartScript(&portraits[1], M_BOSS_PORTRAIT_SCRIPT);
    for (int32_t i = 0; i < 2; i++) {
        portraits[i].pos = (VEC3) { 100.0f + 200.0f * (float)i, 200.0f, 0.0f };
        portraits[i].pos_offset = (VEC3) { -50.0f, 0.0f, 0.0f };
    }
    M_Draw();
    // The first two textured quads of the play area are the portraits.
    float left[2];
    int32_t found = 0;
    for (int32_t i = 0; i < GfxRecorder_QuadCount() && found < 2; i++) {
        const GFX_RECORDED_QUAD *const quad = GfxRecorder_Quad(i);
        if (quad->texture != nullptr) {
            left[found++] = quad->vertices[0].position.x;
        }
    }
    CHECK_EQ_INT(found, 2);
    CHECK(left[1] - left[0] == 150.0f);
    M_TearDown();
}

TEST(base_tsa_centers_translated_notices_at_their_adjusted_position)
{
    M_SetUp("translated_notice", 1);
    char *const patches = TestDir_Prepare("translated_notice_patches");
    PatchBuilder_AddPatch(patches, "nmlgc/base_tsa", "{}");
    PatchBuilder_Write(
        patches, "nmlgc/base_tsa/th07/stringlocs.js",
        "{\"Rx983a0\":\"th07 Border Bonus Format\"}");
    PatchBuilder_Write(
        patches, "nmlgc/base_tsa/stringdefs.js",
        "{\"th07 Border Bonus Format\":\"Bonus %7d\"}");
    PATCH_STACK *stack = nullptr;
    CHECK(
        IS_OK(Patch_OpenStack(patches, "th07", &g_PatchGame, nullptr, &stack)));
    Translation_Init(stack, &g_TranslationGame);
    Gui_ShowBorderBonusPopup(2947100);
    g_Gui.popups.notice.pos.x = 104.0f;
    M_Draw();
    const ASCII_STRING *const notice = M_FindString("Bonus 2947100");
    CHECK(notice != nullptr);
    CHECK(notice->pos.x > 140.59f && notice->pos.x < 140.61f);
    CHECK(g_Gui.popups.notice.pos.x == 104.0f);
    Gui_ShowFullPowerPopup();
    M_Draw();
    const ASCII_STRING *const full = M_FindString("Full Power Mode!");
    CHECK(full != nullptr);
    CHECK(full->pos.x == 424.5f);
    Translation_Shutdown();
    Patch_CloseStack(stack);
    Mem_Free(patches);
    M_TearDown();
}

TEST(base_tsa_aligns_the_translated_spell_bonus_label)
{
    M_SetUp("translated_spell_bonus", 1);
    char *const patches = TestDir_Prepare("translated_spell_bonus_patches");
    PatchBuilder_AddPatch(patches, "nmlgc/base_tsa", "{}");
    PatchBuilder_Write(
        patches, "nmlgc/base_tsa/th07/stringlocs.js",
        "{\"Rx9838c\":\"th06_ascii_centered_spell_bonus\"}");
    PatchBuilder_Write(
        patches, "nmlgc/base_tsa/stringdefs.js",
        "{\"th06_ascii_centered_spell_bonus\":\"Captured!\"}");
    PATCH_STACK *stack = nullptr;
    CHECK(
        IS_OK(Patch_OpenStack(patches, "th07", &g_PatchGame, nullptr, &stack)));
    Translation_Init(stack, &g_TranslationGame);
    Gui_ShowSpellCardBonus(12345);
    M_Draw();
    const ASCII_STRING *const label = M_FindString("Captured!");
    CHECK(label != nullptr);
    CHECK(label->pos.x == 161.5f);
    CHECK(label->pos.y == 80.0f);
    CHECK(g_Gui.popups.spell_bonus.pos.x == 128.0f);
    Translation_Shutdown();
    Patch_CloseStack(stack);
    Mem_Free(patches);
    M_TearDown();
}

TEST(base_tsa_aligns_the_translated_stage_clear_table)
{
    M_SetUp("translated_stage_clear", 2);
    g_Gui.stage_clear.state = GUI_STAGE_CLEAR_SHOWN;
    M_Draw();
    CHECK(M_FindString("Stage Clear")->pos.x == 144.0f);
    char *const patches = TestDir_Prepare("translated_stage_clear_patches");
    PatchBuilder_AddPatch(patches, "nmlgc/base_tsa", "{}");
    PatchBuilder_Write(
        patches, "nmlgc/base_tsa/th07/stringlocs.js",
        "{\"Rx98518\":\"title\",\"Rx984fc\":\"clear\",\"Rx98474\":\"rank\","
        "\"Rx98400\":\"total\"}");
    PatchBuilder_Write(
        patches, "nmlgc/base_tsa/stringdefs.js",
        "{\"title\":\"Complete\",\"clear\":\"Stage = %8d\",\"rank\":\"Hard "
        "x1.2\",\"total\":\"Sum = %8d0\"}");
    PATCH_STACK *stack = nullptr;
    CHECK(
        IS_OK(Patch_OpenStack(patches, "th07", &g_PatchGame, nullptr, &stack)));
    Translation_Init(stack, &g_TranslationGame);
    g_GameManager.run.difficulty = 2;
    g_Gui.stage_clear.total = 312452;
    M_Draw();
    const ASCII_STRING *const title = M_FindString("Complete");
    const ASCII_STRING *const clear = M_FindString("Stage =  2000000");
    const ASCII_STRING *const rank = M_FindString("Hard x1.2");
    const ASCII_STRING *const total = M_FindString("Sum =   3124520");
    CHECK(
        title != nullptr && clear != nullptr && rank != nullptr
        && total != nullptr);
    CHECK(title->pos.x == 112.0f);
    CHECK(clear->pos.x == title->pos.x && clear->pos.y == 160.0f);
    CHECK(rank->pos.x == title->pos.x && rank->pos.y == 240.0f);
    CHECK(total->pos.x == title->pos.x && total->pos.y == 256.0f);
    Translation_Shutdown();
    Patch_CloseStack(stack);
    Mem_Free(patches);
    M_TearDown();
}
