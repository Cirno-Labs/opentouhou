#include "th07/gui/private.h"

#include "th07/ascii.h"
#include "th07/enemy.h"
#include "th07/game_manager.h"
#include "th07/sound.h"
#include "th07/stubs.h"

#include "core/base/color.h"
#include "core/math/x87.h"

#include <stdio.h>
#include <string.h>

#define M_FRAME_Z 0.49f
#define M_FRAME_ITEM_Z 0.48f
#define M_ICON_Z 0.46f
#define M_FRAME_SIDE 12
#define M_FRAME_EDGE 13
#define M_LIFE_ICON 9
#define M_BOMB_ICON 10

static GFX_COLOR M_DrawColor(const uint32_t color)
{
    return (GFX_COLOR) { (uint8_t)(color >> 16), (uint8_t)(color >> 8),
                         (uint8_t)color, (uint8_t)(color >> 24) };
}

static ANM_DRAW M_ScreenDraw(const ANM_VIEW view)
{
    ANM_DRAW draw = Anm_ScreenDraw();
    draw.view = Anm_ScreenView(g_GameManager.camera.viewport, view);
    draw.depth_test = GFX_DEPTH_ALWAYS;
    return draw;
}

// Fills a rectangle of screen pixels with colours at its corners.
static void M_Gradient(
    const float x0, const float y0, const float x1, const float y1,
    const float depth, const uint32_t left_top, const uint32_t right_top,
    const uint32_t left_bottom, const uint32_t right_bottom)
{
    const GFX_VERTEX vertices[4] = {
        { { x0, y0, depth }, M_DrawColor(left_top), {} },
        { { x1, y0, depth }, M_DrawColor(right_top), {} },
        { { x1, y1, depth }, M_DrawColor(right_bottom), {} },
        { { x0, y1, depth }, M_DrawColor(left_bottom), {} },
    };
    GFX_VIEW view = GfxView_Screen();
    view.id = ANM_VIEW_SCREEN;
    Gfx_SetView(&view);
    Gfx_DrawQuad(
        &(GFX_STATE) { .blend = GFX_BLEND_ALPHA }, nullptr, vertices, 0);
}

// Draws a bar that is light at its top and dark at its bottom.
static void M_Bar(
    const float x0, const float y0, const float x1, const float y1,
    const uint32_t light, const uint32_t dark)
{
    M_Gradient(x0, y0, x1, y1, 0.0f, light, light, dark, dark);
}

// Draws vm moved by its pos_offset.
static void M_DrawOffset(ANM_VM *const vm, const ANM_DRAW *const draw)
{
    const VEC3 pos = vm->pos;
    vm->pos = Vec3_Add(vm->pos, vm->pos_offset);
    Anm_DrawUnrotated(vm, draw);
    vm->pos = pos;
}

static void M_DrawStageClear(void)
{
    if (g_Gui.stage_clear.state == GUI_STAGE_CLEAR_NONE) {
        return;
    }
    const GAME_MANAGER *const gm = &g_GameManager;
    VEC3 cursor = { 144.0f, 128.0f, 0.0f };
    AsciiText_SetColor(COLOR_ARGB(0xff, 0xff, 0xff, 0x40));
    AsciiText_AddString(
        cursor, gm->run.stage < 6 ? "Stage Clear" : "All Clear!");
    cursor.y += 32.0f;
    AsciiText_SetColor(COLOR_WHITE);
    AsciiText_AddFormat(cursor, "Clear  = %8d", gm->run.stage * 1000000);
    cursor.y += 16.0f;
    AsciiText_SetColor(COLOR_ARGB(0xff, 0xe0, 0xe0, 0xff));
    AsciiText_AddFormat(
        cursor, "Point  = %8d", g_Gui.stage_clear.point_items * 50000);
    cursor.y += 16.0f;
    AsciiText_SetColor(COLOR_ARGB(0xff, 0xd0, 0xd0, 0xff));
    AsciiText_AddFormat(cursor, "Graze  = %8d", g_Gui.stage_clear.graze * 500);
    cursor.y += 16.0f;
    AsciiText_AddFormat(cursor, "Cherry = %8d", g_Gui.stage_clear.cherry * 10);
    if (GuiStageClear_CountsLivesAndBombs()) {
        cursor.y += 16.0f;
        AsciiText_SetColor(COLOR_ARGB(0xff, 0xff, 0xff, 0x80));
        AsciiText_AddFormat(
            cursor, "Player =%9d", X87_Ftol(gm->player.lives) * 20000000);
        cursor.y += 16.0f;
        AsciiText_AddFormat(
            cursor, "Bomb   = %8d", X87_Ftol(gm->player.bombs) * 4000000);
    }
    cursor.y += 32.0f;
    static const char *const ranks[] = {
        "Easy Rank    *0.5", "Normal Rank  *1.0", "Hard Rank    *1.2",
        "Lunatic Rank *1.5", "Extra Rank   *2.0", "Phantasm Rank*2.0",
    };
    if (gm->run.difficulty >= 0 && gm->run.difficulty < 6) {
        AsciiText_SetColor(COLOR_ARGB(0xff, 0xff, 0x80, 0x80));
        AsciiText_AddString(cursor, ranks[gm->run.difficulty]);
    }
    cursor.y += 16.0f;
    if (gm->run.difficulty < 4 && !gm->mode.in_practice
        && (gm->run.start_lives == 3 || gm->run.start_lives == 4)) {
        AsciiText_SetColor(COLOR_ARGB(0xff, 0xff, 0x80, 0x80));
        AsciiText_AddString(
            cursor,
            gm->run.start_lives == 3 ? "Player Penalty*0.5"
                                     : "Player Penalty*0.2");
        cursor.y += 16.0f;
    }
    AsciiText_SetColor(COLOR_WHITE);
    AsciiText_AddFormat(cursor, "Total = %8d0", g_Gui.stage_clear.total);
}

static void M_DrawMsg(void)
{
    if (g_Gui.msg.script < 0
        || (g_GameManager.run.stage == 6
            && (g_Gui.msg.script == 1 || g_Gui.msg.script == 11))) {
        return;
    }
    const float height = g_Gui.msg.timer.current < M_BOX_OPEN_FRAMES
        ? Timer_AsFloat(&g_Gui.msg.timer) * 48.0f / 60.0f
        : 48.0f;
    ANM_DRAW draw = M_ScreenDraw(ANM_VIEW_PLAY_AREA);
    draw.interpolate = true;
    Anm_DrawUnrotated(&g_Gui.msg.portraits[0], &draw);
    // Only the boss's portrait moves by its offset.
    M_DrawOffset(&g_Gui.msg.portraits[1], &draw);
    const float x = g_GameManager.play_area.x;
    M_Gradient(
        x + 16.0f, 384.0f, x + 384.0f - 16.0f, 384.0f + height, 0.0f,
        COLOR_ARGB(0xd0, 0x00, 0x00, 0x00), COLOR_ARGB(0xd0, 0x00, 0x00, 0x00),
        COLOR_ARGB(0x90, 0x00, 0x00, 0x00), COLOR_ARGB(0x90, 0x00, 0x00, 0x00));
    for (int32_t i = 0; i < GUI_MSG_LINE_COUNT; i++) {
        Anm_DrawUnrotated(&g_Gui.msg.lines[i], &draw);
    }
    for (int32_t i = 0; i < GUI_MSG_LINE_COUNT; i++) {
        Anm_DrawUnrotated(&g_Gui.msg.nameplates[i], &draw);
    }
}

static void M_DrawDigit(
    ANM_VM *const vm, const int32_t digit, const ANM_DRAW *const draw)
{
    vm->sprite_data = Anm_GetSpriteEntry(M_SCORE_DIGIT_SPRITE + digit);
    Anm_DrawUnrotated(vm, draw);
}

// Draws count, up to 99, after a gap of gap pixels: its tens unless 0,
// then its units.
static void M_DrawCount(
    ANM_VM *const vm, const float gap, int32_t count,
    const ANM_DRAW *const draw)
{
    if (count > 99) {
        count = 99;
    }
    vm->pos.x += gap;
    if (count / 10 != 0) {
        M_DrawDigit(vm, count / 10, draw);
    }
    vm->pos.x += 7.0f;
    M_DrawDigit(vm, count % 10, draw);
}

// Draws the bonus of the running spell card, then its captures and its
// attempts.
static void M_DrawSpellScore(const ANM_DRAW *const draw)
{
    ANM_VM *const digit = &g_Gui.banners.spell_score_digit;
    int32_t score =
        g_Enemies.spell_card.bonus + g_Enemies.spell_card.graze_bonus;
    if (!g_Enemies.spell_card.capturable) {
        score = 0;
    }
    digit->pos = g_Gui.banners.spell_score_bar.pos;
    digit->pos.x -= 40.0f;
    bool shown = false;
    for (int32_t divisor = 10000000; divisor > 0; divisor /= 10) {
        const int32_t value = score / divisor;
        if (value != 0) {
            shown = true;
        }
        if (shown || divisor == 1) {
            M_DrawDigit(digit, value, draw);
        }
        digit->pos.x += 7.0f;
        score %= divisor;
    }
    int32_t captures;
    int32_t attempts;
    ResultScreen_GetSpellCardRecord(
        g_Enemies.spell_card.index, &captures, &attempts);
    M_DrawCount(digit, 36.0f, captures, draw);
    M_DrawCount(digit, 14.0f, attempts, draw);
}

static void M_DrawBossBar(const ANM_DRAW *const draw)
{
    static const uint32_t timer_colors[] = {
        COLOR_ARGB(0x00, 0xa0, 0xd0, 0xff),
        COLOR_ARGB(0x00, 0xa0, 0x80, 0xff),
        COLOR_ARGB(0x00, 0xe0, 0x80, 0xc0),
        COLOR_ARGB(0x00, 0xff, 0x40, 0x40),
    };
    const uint32_t alpha = g_Gui.hud.boss_bar_alpha << 24;
    const float shown = g_Gui.hud.boss_health_shown;
    M_Bar(
        64.0f, 19.0f, shown * 320.0f + 64.0f, 23.0f, alpha | 0xffffffu,
        alpha | 0x202060u);
    for (int32_t i = 0; i < GUI_BOSS_SECTION_COUNT; i++) {
        const float start = g_Gui.boss.sections[i].start;
        const float end = g_Gui.boss.sections[i].end;
        // A section at start 0 is unused; one past the shown health is
        // not drawn.
        if (start == 0.0f || end >= shown) {
            continue;
        }
        const float top = shown < start ? shown : start;
        const int32_t color = g_Gui.boss.sections[i].color;
        M_Bar(
            end * 320.0f + 64.0f, 19.0f, top * 320.0f + 64.0f, 23.0f,
            alpha | ((uint32_t)color & 0xffffffu),
            alpha | ((uint32_t)(color >> 2) & 0x3f3f3fu));
    }
    Anm_DrawUnrotated(&g_Gui.hud.sprites[M_BOSS_BAR], draw);
    const int32_t lives = g_Gui.boss.lives;
    const float shrink = lives <= 5 ? 2.0f : 1.0f;
    for (int32_t i = 0; i < lives; i++) {
        M_Bar(
            (float)i * 26.0f / (float)lives + 35.0f, 19.0f,
            (float)(i + 1) * 26.0f / (float)lives + 35.0f - shrink, 23.0f,
            alpha | (uint32_t)(0xffffff - i * 0xff / 9), alpha | 0x202020u);
    }
    const int32_t seconds = g_Gui.boss.seconds;
    const uint32_t timer_color = timer_colors
        [seconds >= 20       ? 0
             : seconds >= 10 ? 1
             : seconds >= 5  ? 2
                             : 3];
    AsciiText_SetColor(alpha | timer_color);
    const int32_t shown_seconds = seconds > 99 ? 99 : seconds;
    if (shown_seconds < 10 && g_Gui.hud.boss_seconds_drawn != seconds) {
        Sound_Queue(SOUND_TIMEOUT);
    }
    AsciiText_AddFormat((VEC3) { 384.0f, 16.0f, 0.0f }, "%.2d", shown_seconds);
    AsciiText_SetColor(COLOR_WHITE);
    g_Gui.hud.boss_seconds_drawn = seconds;
}

static void M_DrawBanners(void)
{
    ANM_DRAW draw = M_ScreenDraw(ANM_VIEW_PLAY_AREA);
    draw.interpolate = true;
    for (int32_t i = 0; i < GUI_TITLE_COUNT; i++) {
        draw.part = (uint32_t)i;
        Anm_Draw(&g_Gui.banners.titles[i], &draw);
    }
    draw.part = 0;
    if (g_Gui.banners.bomb_portrait.shown) {
        Anm_DrawUnrotated(&g_Gui.banners.bomb_portrait, &draw);
        Anm_DrawUnrotated(&g_Gui.banners.bomb_glow, &draw);
        Anm_Draw(&g_Gui.banners.bomb_frame, &draw);
    }
    if (g_Gui.banners.spell_portrait.shown) {
        M_DrawOffset(&g_Gui.banners.spell_portrait, &draw);
        Anm_DrawUnrotated(&g_Gui.banners.spell_glow, &draw);
        Anm_Draw(&g_Gui.banners.spell_frame, &draw);
    }
    if (g_Gui.banners.bomb_name.shown) {
        g_Gui.banners.bomb_bar.pos = g_Gui.banners.bomb_name.pos;
        Anm_DrawUnrotated(&g_Gui.banners.bomb_bar, &draw);
        Anm_Draw(&g_Gui.banners.bomb_name, &draw);
    }
    if (g_Gui.banners.spell_name.shown) {
        g_Gui.banners.spell_bar.pos = g_Gui.banners.spell_name.pos;
        Anm_DrawUnrotated(&g_Gui.banners.spell_bar, &draw);
        Anm_Draw(&g_Gui.banners.spell_name, &draw);
        Anm_DrawUnrotated(&g_Gui.banners.spell_score_bar, &draw);
        draw.interpolate = false;
        M_DrawSpellScore(&draw);
        draw.interpolate = true;
    }
    if (g_Gui.loading.icon.sprite >= 0) {
        Anm_DrawUnrotated(&g_Gui.loading.icon, &draw);
        Anm_DrawUnrotated(&g_Gui.loading.snapshot, &draw);
        if (g_Gui.loading.caption.sprite >= 0) {
            g_Gui.loading.caption.pos = (VEC3) { 304.0f, 448.0f, 0.0f };
            Anm_DrawUnrotated(&g_Gui.loading.caption, &draw);
        }
    }
    if (g_Gui.loading.tiles_running != 0) {
        for (int32_t i = 0; i < GUI_TILE_COUNT; i++) {
            draw.part = (uint32_t)i;
            Anm_DrawProjected3D(&g_Gui.loading.tiles[i], &draw);
        }
        draw.part = 0;
    }
    if (g_Gui.msg.script < 0
        && (int32_t)g_Gui.boss.present + (int32_t)g_Gui.hud.boss_bar > 0) {
        M_DrawBossBar(&draw);
    }
}

static void M_HudSprite(
    const int32_t index, const float x, const float y, const float z,
    ANM_DRAW *const draw)
{
    ANM_VM *const vm = &g_Gui.hud.sprites[index];
    vm->pos = (VEC3) { x, y, z };
    Anm_DrawUnrotated(vm, draw);
    draw->part++;
}

// Draws score, nine digits from 100000000 at a smaller scale, and the
// continues after it.
static void M_DrawScore(
    const float y, const uint32_t score, const uint8_t continues)
{
    VEC3 pos = { 496.0f, y, 0.0f };
    if (score < 100000000) {
        AsciiText_AddFormat(pos, "%.8d", (int32_t)score);
        pos.x += 112.0f;
        AsciiText_AddFormat(pos, "%1d", continues);
        return;
    }
    g_AsciiText.scale_y = 0.9f;
    g_AsciiText.scale_x = 1.0f;
    AsciiText_AddFormat(pos, "%.9d", (int32_t)score);
    // Nine digits 14 wide at the 0.9 scale; the product is one ulp below
    // 113.4f.
    pos.x += 126.0f * 0.9f;
    AsciiText_AddFormat(pos, "%1d", continues);
    g_AsciiText.scale_y = 1.0f;
    g_AsciiText.scale_x = 1.0f;
}

static void M_DrawHud(void)
{
    const GAME_MANAGER *const gm = &g_GameManager;
    g_GameManager.camera.viewport.x = 0.0f;
    g_GameManager.camera.viewport.y = 0.0f;
    g_GameManager.camera.viewport.width = 640.0f;
    g_GameManager.camera.viewport.height = 480.0f;
    ANM_DRAW draw = M_ScreenDraw(ANM_VIEW_HUD);
    for (float y = 0.0f; y < 464.0f; y += 32.0f) {
        M_HudSprite(M_FRAME_SIDE, 0.0f, y, M_FRAME_Z, &draw);
    }
    for (float x = 416.0f; x < 624.0f; x += 32.0f) {
        for (float y = 16.0f; y < 464.0f; y += 32.0f) {
            M_HudSprite(M_FRAME_SIDE, x, y, M_FRAME_Z, &draw);
        }
    }
    for (float x = 0.0f; x < 624.0f; x += 128.0f) {
        M_HudSprite(M_FRAME_EDGE, x, 0.0f, M_FRAME_Z, &draw);
        M_HudSprite(M_FRAME_EDGE, x, 464.0f, M_FRAME_Z, &draw);
    }
    Anm_DrawUnrotated(&g_Gui.hud.sprites[0], &draw);
    Anm_Draw(&g_Gui.hud.sprites[1], &draw);
    for (int32_t i = 2; i <= 8; i++) {
        Anm_DrawUnrotated(&g_Gui.hud.sprites[i], &draw);
    }
    static const float item_rows[] = { 96.0f, 112.0f, 144.0f, 160.0f, 176.0f };
    M_HudSprite(M_FRAME_EDGE, 496.0f, 48.0f, M_FRAME_Z, &draw);
    M_HudSprite(M_FRAME_EDGE, 496.0f, 64.0f, M_FRAME_Z, &draw);
    for (size_t i = 0; i < sizeof(item_rows) / sizeof(item_rows[0]); i++) {
        M_HudSprite(M_FRAME_EDGE, 496.0f, item_rows[i], M_FRAME_ITEM_Z, &draw);
    }
    M_HudSprite(M_FRAME_EDGE, 512.0f, 464.0f, M_FRAME_ITEM_Z, &draw);
    const int32_t lives = X87_Ftol(gm->player.lives);
    float x = 496.0f;
    for (int32_t i = 0; i < lives; i++, x += 16.0f) {
        M_HudSprite(M_LIFE_ICON, x, 96.0f, M_ICON_Z, &draw);
    }
    const int32_t bombs = X87_Ftol(gm->player.bombs);
    x = 496.0f;
    for (int32_t i = 0; i < bombs; i++, x += 16.0f) {
        M_HudSprite(M_BOMB_ICON, x, 112.0f, M_ICON_Z, &draw);
    }
    for (x = 32.0f; x < 368.0f; x += 128.0f) {
        M_HudSprite(M_FRAME_EDGE, x, 464.0f, M_FRAME_Z, &draw);
    }
    M_DrawScore(64.0f, gm->score.shown, gm->stats.continues_used);
    M_DrawScore(48.0f, gm->score.high, gm->score.high_continues);
    AsciiText_AddFormat(
        (VEC3) { 496.0f, 160.0f, 0.0f }, "%d", gm->player.graze_total);
    AsciiText_AddFormat(
        (VEC3) { 496.0f, 176.0f, 0.0f }, "%d/%d", gm->player.point_items_total,
        gm->player.next_extend_items);
    const int32_t power = X87_Ftol(gm->player.power);
    if (power > 0) {
        const float right = (float)(power + 496);
        M_Gradient(
            496.0f, 144.0f, right, 160.0f, 0.1f,
            COLOR_ARGB(0xe0, 0xe0, 0xe0, 0xff),
            COLOR_ARGB(0x80, 0xe0, 0xe0, 0xff),
            COLOR_ARGB(0xe0, 0xe0, 0xe0, 0xff),
            COLOR_ARGB(0x80, 0xe0, 0xe0, 0xff));
    }
    if (power < M_MAX_POWER) {
        AsciiText_AddFormat((VEC3) { 496.0f, 144.0f, 0.0f }, "%d", power);
    } else {
        AsciiText_AddString((VEC3) { 496.0f, 144.0f, 0.0f }, "MAX");
    }
}

// Draws text at the narrow style of the Border notices.
static void M_DrawNarrow(const VEC3 pos, const char *const text)
{
    g_AsciiText.scale_y = 0.9f;
    g_AsciiText.scale_x = 1.0f;
    g_AsciiText.glyph_width = 11;
    AsciiText_SetColor(COLOR_ARGB(0xff, 0xe0, 0xb0, 0xff));
    AsciiText_AddString(pos, text);
    AsciiText_SetColor(COLOR_WHITE);
    g_AsciiText.scale_y = 1.0f;
    g_AsciiText.scale_x = 1.0f;
    g_AsciiText.glyph_width = 14;
}

static void M_DrawNotice(void)
{
    const GUI_POPUP *const notice = &g_Gui.popups.notice;
    if (!notice->shown) {
        return;
    }
    char buffer[32];
    switch (g_Gui.popups.notice_kind) {
    case GUI_NOTICE_FULL_POWER:
        AsciiText_SetColor(COLOR_ARGB(0xff, 0xc0, 0xb0, 0xff));
        AsciiText_AddString(notice->pos, "Full Power Mode!");
        AsciiText_SetColor(COLOR_WHITE);
        break;
    case GUI_NOTICE_BORDER_START:
        M_DrawNarrow(notice->pos, "Supernatural Border!!");
        break;
    case GUI_NOTICE_CHERRY_MAX:
        AsciiText_SetColor(COLOR_ARGB(0xff, 0xc0, 0xb0, 0xff));
        AsciiText_AddString(notice->pos, "CherryPoint Max!");
        AsciiText_SetColor(COLOR_WHITE);
        break;
    case GUI_NOTICE_BORDER_BONUS:
        snprintf(buffer, sizeof(buffer), "Border Bonus %7d", notice->value);
        M_DrawNarrow(notice->pos, buffer);
        break;
    default:
        break;
    }
}

static void M_DrawPopups(void)
{
    g_AsciiText.in_arena = true;
    if (g_Gui.popups.bonus.shown) {
        AsciiText_SetColor(COLOR_ARGB(0xff, 0xff, 0xff, 0x80));
        AsciiText_AddFormat(
            g_Gui.popups.bonus.pos, "BONUS %8d", g_Gui.popups.bonus.value);
        AsciiText_SetColor(COLOR_WHITE);
    }
    M_DrawNotice();
    if (g_Gui.popups.spell_bonus.shown) {
        GUI_POPUP *const popup = &g_Gui.popups.spell_bonus;
        AsciiText_SetColor(COLOR_ARGB(0xff, 0xff, 0x00, 0x00));
        popup->pos.x =
            (384.0f - (float)strlen("Spell Card Bonus!") * 16.0f) / 2.0f
            + 32.0f;
        popup->pos.y = 80.0f;
        AsciiText_AddString(popup->pos, "Spell Card Bonus!");
        popup->pos.y += 16.0f;
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "+%d", popup->value);
        popup->pos.x = (384.0f - (float)strlen(buffer) * 32.0f) / 2.0f + 32.0f;
        g_AsciiText.scale_y = 2.0f;
        g_AsciiText.scale_x = 2.0f;
        AsciiText_SetColor(COLOR_ARGB(0xff, 0xff, 0x80, 0x80));
        AsciiText_AddString(popup->pos, buffer);
        g_AsciiText.scale_y = 1.0f;
        g_AsciiText.scale_x = 1.0f;
        AsciiText_SetColor(COLOR_WHITE);
    }
    g_AsciiText.in_arena = false;
}

CHAIN_STATUS GuiDraw_Frame(void)
{
    Anm_SetShake(0.0f, 0.0f);
    M_DrawStageClear();
    M_DrawMsg();
    M_DrawBanners();
    M_DrawHud();
    M_DrawPopups();
    return CHAIN_CONTINUE;
}
