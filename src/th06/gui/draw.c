#include "th06/gui/private.h"

#include "th06/ascii.h"
#include "th06/game_manager.h"
#include "th06/sound.h"
#include "th06/translation.h"

#include "core/base/utils.h"

#include <stdio.h>
#include <string.h>

#define M_SET_SPRITE_OPCODE 1

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

// Returns the width of a dialogue line: the width of the sprite that the
// script of the first line shows first, which is 256 in the original.
static float M_LineWidth(void)
{
    const ANM_SCRIPT *const script = Anm_GetScript(M_LINE_SCRIPT);
    for (int32_t i = 0; script != nullptr && i < script->count; i++) {
        if (script->instrs[i].opcode == M_SET_SPRITE_OPCODE) {
            ANM_VM vm;
            Anm_CreateVm(&vm);
            Anm_SetSprite(
                &vm,
                (int32_t)script->instrs[i].args[0] + script->sprite_offset);
            if (vm.sprite_data != nullptr) {
                return vm.sprite_data->width;
            }
            break;
        }
    }
    return 256.0f;
}

static void M_DrawMsg(void)
{
    if (g_Gui.msg.script < 0
        || (g_GameManager.run.stage == 6
            && (g_Gui.msg.script == 1 || g_Gui.msg.script == 11))) {
        return;
    }
    ANM_DRAW draw = M_ScreenDraw(ANM_VIEW_PLAY_AREA);
    draw.interpolate = true;
    Anm_DrawUnrotated(&g_Gui.msg.portraits[0], &draw);
    Anm_DrawUnrotated(&g_Gui.msg.portraits[1], &draw);
    const float height = g_Gui.msg.timer.current < 60
        ? (float)g_Gui.msg.timer.current * 48.0f / 60.0f
        : 48.0f;
    const float width = M_LineWidth();
    const float center = (g_GameManager.play_area.width - width) / 2.0f
        + g_GameManager.play_area.x;
    M_Gradient(
        center - 16.0f, 384.0f, center + width + 16.0f, 384.0f + height, 0.0f,
        0xd0000000u, 0xd0000000u, 0x90000000u, 0x90000000u);
    for (int32_t i = 0; i < GUI_MSG_LINE_COUNT; i++) {
        Anm_DrawUnrotated(&g_Gui.msg.lines[i], &draw);
    }
    const bool reversed = Translation_HasBaseTsa();
    for (int32_t i = 0; i < GUI_MSG_LINE_COUNT; i++) {
        Anm_DrawUnrotated(
            &g_Gui.msg.nameplates[reversed ? GUI_MSG_LINE_COUNT - 1 - i : i],
            &draw);
    }
}

static void M_DrawStageClear(void)
{
    if (g_Gui.stage_clear.state == GUI_STAGE_CLEAR_NONE) {
        return;
    }
    VEC3 cursor = { 74.0f, 128.0f, 0.0f };
    AsciiText_SetColor(0xffffff40u);
    AsciiText_AddString(
        cursor,
        g_GameManager.run.stage < 6 ? "Stage Clear\n\n" : "All Clear!\n\n");
    cursor.y += 32.0f;
    AsciiText_SetColor(0xffffffffu);
    AsciiText_AddFormat(
        cursor, "Stage * 1000 = %5d\n", g_GameManager.run.stage * 1000);
    cursor.y += 16.0f;
    AsciiText_SetColor(0xffe0e0ffu);
    AsciiText_AddFormat(
        cursor, "Power *  100 = %5d\n", g_GameManager.player.power * 100);
    cursor.y += 16.0f;
    AsciiText_SetColor(0xffd0d0ffu);
    AsciiText_AddFormat(
        cursor, "Graze *   10 = %5d\n", g_GameManager.player.graze * 10);
    cursor.y += 16.0f;
    AsciiText_SetColor(0xffff8080u);
    AsciiText_AddFormat(
        cursor, "    * Point Item %3d\n", g_GameManager.player.point_items);
    if (g_GameManager.run.stage >= 6) {
        cursor.y += 16.0f;
        AsciiText_SetColor(0xffffff80u);
        AsciiText_AddFormat(
            cursor, "Player    = %8d\n", g_GameManager.player.lives * 3000000);
        cursor.y += 16.0f;
        AsciiText_AddFormat(
            cursor, "Bomb      = %8d\n", g_GameManager.player.bombs * 1000000);
    }
    cursor.y += 32.0f;
    static const char *const ranks[] = {
        "Easy Rank      * 0.5\n", "Normal Rank    * 1.0\n",
        "Hard Rank      * 1.2\n", "Lunatic Rank   * 1.5\n",
        "Extra Rank     * 2.0\n",
    };
    AsciiText_SetColor(0xffff8080u);
    if (g_GameManager.run.difficulty >= 0
        && (size_t)g_GameManager.run.difficulty < ARRAY_SIZE(ranks)) {
        AsciiText_AddString(cursor, ranks[g_GameManager.run.difficulty]);
    }
    cursor.y += 16.0f;
    if (g_GameManager.run.difficulty < 4 && !g_GameManager.mode.in_practice) {
        if (g_GameManager.run.start_lives == 3
            || g_GameManager.run.start_lives == 4) {
            AsciiText_AddString(
                cursor,
                g_GameManager.run.start_lives == 3 ? "Player Penalty * 0.5\n"
                                                   : "Player Penalty * 0.2\n");
            cursor.y += 16.0f;
        }
    }
    AsciiText_SetColor(0xffffffffu);
    AsciiText_AddFormat(cursor, "Total     = %8d", g_Gui.stage_clear.total);
}

// Draws a bomb or spell card name and its bar as base_tsa's executable
// patches do. The name row is cropped to the bar width less 16 pixels from
// its left edge. The crop moves by the texture width less half the crop
// width: to the right for a spell card name, and to the left for a bomb
// name. The bar ends at the right edge of the crop.
static void M_DrawPatchedBanner(
    ANM_VM *const bar, const ANM_VM *const name, const float bar_width,
    const bool spell, const ANM_DRAW *const draw)
{
    if (!name->shown || name->sprite_data == nullptr) {
        return;
    }
    const float width = bar_width - 16.0f;
    ANM_SPRITE sprite = *name->sprite_data;
    sprite.x1 = sprite.x0 + width;
    sprite.width = width;
    sprite.u1 = sprite.x1 / sprite.texture_width;
    ANM_VM cropped = *name;
    cropped.sprite_data = &sprite;
    const float shift = sprite.texture_width - width * 0.5f;
    cropped.pos.x = spell ? shift + name->pos.x : name->pos.x - shift;
    bar->pos = cropped.pos;
    bar->pos.x = (width - bar_width) * 0.5f + cropped.pos.x;
    bar->scale_x = bar_width / 14.0f;
    Anm_DrawUnrotated(bar, draw);
    Anm_DrawUnrotated(&cropped, draw);
}

static void M_DrawBanners(void)
{
    ANM_DRAW world = GameManager_GetDrawContext();
    world.interpolate = true;
    ANM_DRAW screen = M_ScreenDraw(ANM_VIEW_PLAY_AREA);
    screen.interpolate = true;
    if (g_Gui.banners.stage_name.shown) {
        VEC3 pos = { 168.0f, 198.0f, 0.0f };
        AsciiText_SetColor(
            (g_Gui.banners.stage_name.color & 0xff000000u) | 0xffff40u);
        if (!g_GameManager.mode.in_demo) {
            Anm_Draw3DRounded(&g_Gui.banners.stage_name, &world);
            if (g_GameManager.run.stage < 6) {
                AsciiText_AddFormat(pos, "STAGE %d", g_GameManager.run.stage);
            } else {
                pos.x = 136.0f;
                AsciiText_AddString(
                    pos,
                    g_GameManager.run.stage == 6 ? "FINAL STAGE"
                                                 : "EXTRA STAGE");
            }
        } else {
            pos.x = 136.0f;
            AsciiText_AddString(pos, " DEMO PLAY");
        }
        AsciiText_SetColor(0xffffffffu);
    }
    if (g_Gui.banners.song_name.shown && !g_GameManager.mode.in_demo) {
        Anm_Draw3DRounded(&g_Gui.banners.song_name, &world);
    }
    Anm_DrawUnrotated(&g_Gui.banners.bomb_portrait, &screen);
    Anm_DrawUnrotated(&g_Gui.banners.spell_portrait, &screen);
    if (Translation_HasBaseTsa()) {
        M_DrawPatchedBanner(
            &g_Gui.banners.bomb_bar, &g_Gui.banners.bomb_name,
            g_Gui.banners.bomb_width, false, &screen);
        M_DrawPatchedBanner(
            &g_Gui.banners.spell_bar, &g_Gui.banners.spell_name,
            g_Gui.banners.spell_width, true, &screen);
    } else if (g_Gui.banners.bomb_name.shown) {
        g_Gui.banners.bomb_bar.pos = g_Gui.banners.bomb_name.pos;
        g_Gui.banners.bomb_bar.pos.x +=
            g_Gui.banners.bomb_width * 16.0f / 15.0f / 2.0f + -128.0f - 16.0f;
        g_Gui.banners.bomb_bar.scale_x = g_Gui.banners.bomb_width / 14.0f;
        Anm_DrawUnrotated(&g_Gui.banners.bomb_bar, &screen);
        Anm_DrawUnrotated(&g_Gui.banners.bomb_name, &screen);
    }
    if (!Translation_HasBaseTsa() && g_Gui.banners.spell_name.shown) {
        g_Gui.banners.spell_bar.pos = g_Gui.banners.spell_name.pos;
        g_Gui.banners.spell_bar.pos.x +=
            128.0f - g_Gui.banners.spell_width * 16.0f / 15.0f / 2.0f;
        g_Gui.banners.spell_bar.scale_x = g_Gui.banners.spell_width / 14.0f;
        Anm_DrawUnrotated(&g_Gui.banners.spell_bar, &screen);
        Anm_DrawUnrotated(&g_Gui.banners.spell_name, &screen);
    }
    if (g_Gui.loading.sprite >= 0) {
        Anm_DrawUnrotated(&g_Gui.loading, &screen);
    }
}

static void M_HudSprite(const int32_t index, const float x, const float y)
{
    const ANM_DRAW draw = M_ScreenDraw(ANM_VIEW_HUD);
    ANM_VM *const vm = &g_Gui.hud.sprites[index];
    vm->pos = (VEC3) { x, y, M_FRAME_Z };
    Anm_DrawUnrotated(vm, &draw);
}

static void M_DrawHud(void)
{
    ANM_VM *const sprites = g_Gui.hud.sprites;
    ANM_DRAW draw = M_ScreenDraw(ANM_VIEW_PLAY_AREA);
    if (g_Gui.msg.script < 0
        && (int32_t)g_Gui.boss.present + (int32_t)g_Gui.hud.boss_bar > 0) {
        Anm_DrawUnrotated(&sprites[M_BOSS_BAR], &draw);
        ANM_VM *const health = &sprites[M_BOSS_HEALTH_BAR];
        health->anchor = 3;
        health->scale_x = g_Gui.hud.boss_health_shown * 288.0f / 14.0f;
        health->pos = (VEC3) { 96.0f, 24.0f, 0.0f };
        Anm_DrawUnrotated(health, &draw);
        AsciiText_SetColor((g_Gui.hud.boss_bar_alpha << 24) | 0xffff80u);
        AsciiText_AddFormat((VEC3) { 80, 16, 0 }, "%d", g_Gui.boss.lives);
        static const uint32_t colors[] = { 0x00a0d0ff, 0x00a080ff, 0x00e080c0,
                                           0x00ff4040 };
        const uint32_t color = colors
            [g_Gui.boss.seconds >= 20       ? 0
                 : g_Gui.boss.seconds >= 10 ? 1
                 : g_Gui.boss.seconds >= 5  ? 2
                                            : 3];
        AsciiText_SetColor((g_Gui.hud.boss_bar_alpha << 24) | color);
        const int32_t seconds =
            g_Gui.boss.seconds > 99 ? 99 : g_Gui.boss.seconds;
        if (seconds < 10
            && g_Gui.hud.boss_seconds_drawn != g_Gui.boss.seconds) {
            Sound_Queue(SOUND_TIMEOUT);
        }
        AsciiText_AddFormat((VEC3) { 384, 16, 0 }, "%.2d", seconds);
        AsciiText_SetColor(0xffffffffu);
        g_Gui.hud.boss_seconds_drawn = g_Gui.boss.seconds;
    }
    g_GameManager.camera.viewport.x = 0.0f;
    g_GameManager.camera.viewport.y = 0.0f;
    g_GameManager.camera.viewport.width = 640.0f;
    g_GameManager.camera.viewport.height = 480.0f;
    draw = M_ScreenDraw(ANM_VIEW_HUD);
    for (float y = 0.0f; y < 464.0f; y += 32.0f) {
        M_HudSprite(M_FRAME_SIDE, 0.0f, y);
    }
    for (float x = 416.0f; x < 624.0f; x += 32.0f) {
        for (float y = 0.0f; y < 464.0f; y += 32.0f) {
            M_HudSprite(M_FRAME_SIDE, x, y);
        }
    }
    for (float x = 32.0f; x < 416.0f; x += 32.0f) {
        M_HudSprite(M_FRAME_TOP, x, 0.0f);
    }
    for (float x = 32.0f; x < 416.0f; x += 32.0f) {
        M_HudSprite(M_FRAME_BOTTOM, x, 464.0f);
    }
    static const int32_t labels[] = { 5, 0, 1, 3, 4, 2 };
    for (size_t i = 0; i < ARRAY_SIZE(labels); i++) {
        Anm_Draw(&sprites[labels[i]], &draw);
    }
    for (int32_t i = 9; i <= 15; i++) {
        Anm_DrawUnrotated(&sprites[i], &draw);
    }
    static const float backgrounds[] = { 58, 82, 122, 146, 186, 206, 226 };
    for (size_t i = 0; i < ARRAY_SIZE(backgrounds); i++) {
        M_HudSprite(M_VALUE_BACKGROUND, 496, backgrounds[i]);
    }
    M_HudSprite(M_VALUE_BACKGROUND, 488, 464);
    M_HudSprite(M_VALUE_BACKGROUND, 0, 464);
    for (int32_t i = 0; i < g_GameManager.player.lives; i++) {
        M_HudSprite(0x10, 496.0f + (float)i * 16.0f, 122.0f);
    }
    for (int32_t i = 0; i < g_GameManager.player.bombs; i++) {
        M_HudSprite(0x11, 496.0f + (float)i * 16.0f, 146.0f);
    }
    if (g_GameManager.player.power > 0) {
        M_Gradient(
            496, 186, (float)g_GameManager.player.power + 496.0f, 202, 0.1f,
            0xe0e0e0ffu, 0x80e0e0ffu, 0xe0e0e0ffu, 0x80e0e0ffu);
        if (g_GameManager.player.power >= M_MAX_POWER) {
            sprites[M_FULL_POWER_SPRITE].pos = (VEC3) { 496, 186, 0 };
            Anm_DrawUnrotated(&sprites[M_FULL_POWER_SPRITE], &draw);
        }
    }
    if (g_GameManager.player.power < M_MAX_POWER) {
        AsciiText_AddFormat(
            (VEC3) { 496, 186, 0 }, "%d", g_GameManager.player.power);
    }
    AsciiText_AddFormat(
        (VEC3) { 496, 82, 0 }, "%.9u", g_GameManager.score.shown);
    AsciiText_AddFormat(
        (VEC3) { 496, 58, 0 }, "%.9u", g_GameManager.score.high);
    AsciiText_AddFormat(
        (VEC3) { 496, 206, 0 }, "%d", g_GameManager.player.graze);
    AsciiText_AddFormat(
        (VEC3) { 496, 226, 0 }, "%d", g_GameManager.player.point_items);
}

static void M_DrawPopups(void)
{
    g_AsciiText.in_arena = true;
    if (g_Gui.popups.bonus.shown) {
        AsciiText_SetColor(0xffffff80u);
        AsciiText_AddFormat(
            g_Gui.popups.bonus.pos, "BONUS %8d", g_Gui.popups.bonus.value);
        AsciiText_SetColor(0xffffffffu);
    }
    if (g_Gui.popups.full_power.shown) {
        AsciiText_SetColor(0xffc0b0ffu);
        AsciiText_AddString(g_Gui.popups.full_power.pos, "Full Power Mode!!");
        AsciiText_SetColor(0xffffffffu);
    }
    if (g_Gui.popups.spell_bonus.shown) {
        AsciiText_SetColor(0xffff0000u);
        GUI_POPUP *const popup = &g_Gui.popups.spell_bonus;
        popup->pos.x =
            (384.0f - (float)strlen("Spell Card Bonus!") * 16.0f) / 2.0f
            + 32.0f;
        popup->pos.y = 80.0f;
        AsciiText_AddString(popup->pos, "Spell Card Bonus!");
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "+%d", g_Gui.popups.spell_bonus.value);
        g_AsciiText.scale_x = 2.0f;
        g_AsciiText.scale_y = 2.0f;
        AsciiText_SetColor(0xffff8080u);
        popup->pos.x = (384.0f - (float)strlen(buffer) * 32.0f) / 2.0f + 32.0f;
        popup->pos.y += 16.0f;
        AsciiText_AddString(popup->pos, buffer);
        g_AsciiText.scale_x = 1.0f;
        g_AsciiText.scale_y = 1.0f;
        AsciiText_SetColor(0xffffffffu);
    }
    g_AsciiText.in_arena = false;
}

CHAIN_STATUS GuiDraw_Frame(void)
{
    M_DrawStageClear();
    M_DrawMsg();
    M_DrawBanners();
    M_DrawHud();
    M_DrawPopups();
    return CHAIN_CONTINUE;
}
