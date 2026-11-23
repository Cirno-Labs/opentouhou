#include "th01/route_select.h"

#include "core/base/assert.h"
#include "th01/scale2x.h"

#include <string.h>

typedef struct {
    const char *text;
    int16_t left, top;
    uint8_t color;
} ROUTE_LINE;

static const ROUTE_LINE m_Lines[] = {
    { "ＲＯＵＮＤ　１　ＣＬＥＡＲ", 64, 64, 7 },
    { "Ｃｏｎｇｒａｔｕｌａｔｉｏｎ！！", 32, 96, 7 },
    { "Ｓｅｌｅｃｔ　ｎｅｘｔ　Ｒｏｕｎｄ", 32, 180, 7 },
    { "魔界へ", 256, 250, 1 },
    { "地獄へ", 256, 300, 3 },
};

static void M_Palette(const ROUTE_SELECT_CONTEXT *const c, const uint8_t route)
{
    Palette_SetColor(c->palette, route == 0 ? 1 : 3, 15, 15, 15);
    Palette_SetColor(c->palette, route == 0 ? 3 : 1, 9, 9, 9);
}

static void M_Draw(const ROUTE_SELECT_CONTEXT *const c)
{
    for (int32_t color = 0; color < 16; color++) {
        for (int32_t component = 0; component < 3; component++) {
            int8_t *const value = &c->palette->saved.colors[color][component];
            if (*value > 0) {
                *value -= 4;
            }
        }
    }
    for (int32_t component = 0; component < 3; component++) {
        c->palette->saved.colors[7][component] = 15;
    }
    Palette_Set(c->palette, &c->palette->saved);
    Video_SelectDrawingPage(1);
    Video_Clear(0);
    const GAME_TEXT_STYLE style = { .clear_background = true };
    for (int32_t line = 0; line < 5; line++) {
        GameText_Draw(
            c->assets->font, m_Lines[line].text, 0, line * 32,
            m_Lines[line].color, &style);
    }
    Video_SelectDrawingPage(0);
    M_Palette(c, 0);
    for (int32_t line = 0; line < 5; line++) {
        const int16_t width = (int16_t)(strlen(m_Lines[line].text) / 3 * 16);
        Scale2x_Copy(
            m_Lines[line].left, m_Lines[line].top, 0, (int16_t)(line * 32),
            width, 16);
    }
    Video_Copy(0, (GFX_RECT) { 0, 0, VIDEO_WIDTH, VIDEO_HEIGHT }, 1, 0, 0);
    Sprite_PtnPut(c->assets->stage, ORB_FIRST_IMAGE, -1, 128, 250);
}

RESULT RouteSelect_LoadFont(
    const FONT_ROM *const rom, const ALLOCATOR *const allocator,
    GAME_TEXT_FONT *const out)
{
    char characters[256] = "";
    size_t used = 0;
    for (int32_t line = 0; line < 5; line++) {
        const size_t size = strlen(m_Lines[line].text);
        ASSERT(used + size < sizeof(characters));
        memcpy(characters + used, m_Lines[line].text, size + 1);
        used += size;
    }
    return GameText_Load(rom, characters, allocator, out);
}

RESULT RouteSelect_ValidateAssets(const ROUTE_SELECT_ASSETS *const assets)
{
    FAIL_IF(
        assets->font == nullptr || assets->stage == nullptr
            || assets->makai == nullptr || assets->jigoku == nullptr,
        "Missing route selection assets");
    FAIL_IF(
        assets->stage->pixels == nullptr
            || assets->stage->count <= ORB_FIRST_IMAGE,
        "Missing route cursor sprite");
    FAIL_IF(
        assets->makai->id != 1 || assets->jigoku->id != 2
            || assets->makai->layouts.stages == nullptr
            || assets->jigoku->layouts.stages == nullptr,
        "Missing cached route scenes");
    for (int32_t line = 0; line < 5; line++) {
        // Every static route label uses three-byte UTF-8 fullwidth scalars.
        for (const uint8_t *p = (const uint8_t *)m_Lines[line].text; *p != 0;
             p += 3) {
            const uint32_t cp = ((uint32_t)(p[0] & 15) << 12)
                | ((uint32_t)(p[1] & 63) << 6) | (p[2] & 63);
            const GAME_TEXT_GLYPH *const glyph =
                GameText_Find(assets->font, cp);
            FAIL_IF(
                glyph == nullptr || glyph->width != 16,
                "Missing fullwidth route glyph");
        }
    }
    return OK;
}

bool RouteSelect_Begin(
    ROUTE_SELECT *const selection, const ROUTE_SELECT_CONTEXT *const c,
    const TH01_INPUT *const raw)
{
    ASSERT(selection->phase == ROUTE_SELECT_DONE);
    *selection = (ROUTE_SELECT) { .phase = ROUTE_SELECT_INITIAL_WAIT };
    M_Draw(c);
    GameInput_ResetGame();
    return RouteSelect_Resume(selection, c, raw);
}

bool RouteSelect_Resume(
    ROUTE_SELECT *const selection, const ROUTE_SELECT_CONTEXT *const c,
    const TH01_INPUT *const raw)
{
    for (;;) {
        switch (selection->phase) {
        case ROUTE_SELECT_DONE:
            return true;
        case ROUTE_SELECT_INITIAL_WAIT:
            if (!Frame_DelayReady(50)) {
                return false;
            }
            selection->phase = ROUTE_SELECT_CHOOSE;
            break;
        case ROUTE_SELECT_CHOOSE: {
            if (GameInput_SenseGame(raw, false)) {
                c->player->is_hit = true;
            }
            if ((g_GameInput.up && !selection->holding_up)
                || (g_GameInput.down && !selection->holding_down)) {
                Sprite_Restore16(
                    128, selection->selection == 0 ? 250 : 300, 32, 32);
                selection->selection ^= 1;
                Sprite_PtnPut(
                    c->assets->stage, ORB_FIRST_IMAGE, -1, 128,
                    selection->selection == 0 ? 250 : 300);
                M_Palette(c, selection->selection);
            }
            selection->holding_up = g_GameInput.up;
            selection->holding_down = g_GameInput.down;
            if (g_GameInput.shot || g_GameInput.ok) {
                c->effect(c->user, 4);
                Scene_Activate(
                    c->scene,
                    selection->selection == 0 ? c->assets->makai
                                              : c->assets->jigoku);
                *c->route = selection->selection;
                *c->stage_cleared = true;
                c->player->is_hit = true;
                selection->phase = ROUTE_SELECT_DONE;
                return true;
            }
            selection->phase = ROUTE_SELECT_REPEAT_WAIT;
            break;
        }
        case ROUTE_SELECT_REPEAT_WAIT:
            if (!Frame_DelayReady(5)) {
                return false;
            }
            selection->phase = ROUTE_SELECT_CHOOSE;
            break;
        }
    }
}
