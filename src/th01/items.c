#include "th01/items.h"

#include "pc98/zun/math16.h"
#include "th01/shape.h"

ITEMS g_Items;

#define ITEM_BOMB_IMAGE 32
#define ITEM_POINT_IMAGE 33

static const char m_Characters[] = " BombExtend!!0123456789１萬";
static const GAME_TEXT_STYLE m_Style = { .weight = GAME_TEXT_NORMAL };

static bool M_Visible(const ITEM *const item)
{
    return item->flag > ITEM_SPLASH && item->flag < ITEM_COLLECTED;
}

static int16_t M_SecondLeft(const ITEM *const item)
{
    const int32_t left = item->left - 16;
    return left < 0 ? 0 : left > 576 ? 576 : (int16_t)left;
}

static void M_Text(
    const ITEM_CONTEXT *const context, const char *const text,
    const int16_t left, const int16_t top)
{
    GameText_Draw(context->font, text, left, top, 7, &m_Style);
}

static void M_Collect(
    ITEM *const item, const ITEM_CONTEXT *const context, const bool bomb)
{
    const int32_t delta = item->left - g_Player.left;
    if (!M_Visible(item) || delta <= -24 || delta >= 24 || item->top <= 344) {
        return;
    }
    Sprite_Restore16(item->left, item->top, 32, 32);
    item->velocity_y = -2;
    item->radius_or_time = 16;
    if (item->top >= 368) {
        item->top = 368;
    }
    if (bomb && g_Player.remaining_bombs < 5) {
        g_Player.remaining_bombs++;
        context->hud_bomb(context->user, g_Player.remaining_bombs - 1);
        item->flag = ITEM_COLLECTED;
    } else {
        if (bomb) {
            g_Stage.score += 10000;
        } else {
            uint16_t *const value = context->point_value;
            if (*value < 59999) {
                *value = (uint16_t)(*value + (*value < 10000 ? 1000 : 10000));
            }
            if (*value >= 60000) {
                *value = 65530;
            }
            g_Stage.score += *value;
        }
        context->hud_score_and_combo(context->user);
        item->flag = ITEM_COLLECTED_OVER_CAP;
    }
    context->effect(context->user, 15);
}

static void M_Popup(
    ITEM *const item, const ITEM_CONTEXT *const context, const bool bomb)
{
    const int16_t left = bomb ? item->left
        : item->left > 600    ? 600
                              : item->left;
    // The point wrapper expands its nominal 40-pixel width to 48 pixels.
    Sprite_Restore16(left, item->top, bomb ? 32 : 48, 16);
    if (bomb && item->flag == ITEM_COLLECTED) {
        Sprite_Restore16(
            M_SecondLeft(item), Math16_Wrap(item->top + 16), 64, 16);
    }
    item->top = Math16_Wrap(item->top + item->velocity_y);
    item->radius_or_time--;
    if (item->radius_or_time == 0) {
        item->flag = ITEM_FREE;
        return;
    }
    if (bomb) {
        M_Text(
            context, item->flag == ITEM_COLLECTED ? "Bomb" : "１萬", left,
            item->top);
        if (item->flag == ITEM_COLLECTED) {
            M_Text(
                context, "Extend!!", M_SecondLeft(item),
                Math16_Wrap(item->top + 16));
        }
    } else {
        char text[6] = "     ";
        uint16_t value = *context->point_value;
        int32_t i = 4;
        while (value != 0) {
            text[i--] = (char)('0' + value % 10);
            value /= 10;
        }
        M_Text(context, text, left, item->top);
    }
}

static void M_Update(
    ITEM *const item, const ITEM_CONTEXT *const context, const bool bomb)
{
    if (item->flag == ITEM_FREE) {
        return;
    }
    M_Collect(item, context, bomb);
    const int32_t image = bomb ? ITEM_BOMB_IMAGE : ITEM_POINT_IMAGE;
    if (item->flag == ITEM_SPLASH) {
        const int16_t x = Math16_Wrap(item->left + 16);
        const int16_t y = Math16_Wrap(item->top + 16);
        Shape_EllipseArcErase(
            x, y, item->radius_or_time, item->radius_or_time, 8, 0, 255);
        item->radius_or_time += 4;
        if (item->radius_or_time >= 48) {
            item->flag = ITEM_FALL;
            return;
        }
        Shape_EllipseArcPut(
            x, y, item->radius_or_time, item->radius_or_time, 7, 8, 0, 255);
    } else if (item->flag == ITEM_FALL || item->flag == ITEM_BOUNCE) {
        Sprite_PtnErase(context->sprites, image, -1, item->left, item->top);
        if (item->flag == ITEM_FALL) {
            item->top = Math16_Wrap(item->top + item->velocity_y);
            if (item->top >= 368) {
                item->velocity_y = -8;
                item->flag = ITEM_BOUNCE;
            }
        }
        if (item->flag == ITEM_BOUNCE) {
            item->top = Math16_Wrap(item->top + item->velocity_y);
            item->velocity_y = Math16_Wrap(item->velocity_y + 1);
            if (item->top >= 400) {
                item->flag = ITEM_FREE;
                if (!bomb) {
                    *context->point_value = 0;
                }
                return;
            }
        }
        Sprite_PtnPut(context->sprites, image, -1, item->left, item->top);
    } else if (item->flag >= ITEM_COLLECTED) {
        M_Popup(item, context, bomb);
    }
}

RESULT Items_LoadFont(
    const FONT_ROM *const rom, const ALLOCATOR *const allocator,
    GAME_TEXT_FONT *const out)
{
    return GameText_Load(rom, m_Characters, allocator, out);
}

RESULT Items_ValidateAssets(
    const PTN *const sprites, const GAME_TEXT_FONT *const font)
{
    FAIL_IF(
        sprites == nullptr || sprites->pixels == nullptr || sprites->count < 34,
        "Missing item sprites");
    FAIL_IF(font == nullptr, "Missing item popup font");
    for (const char *c = m_Characters; *c != '\0' && (uint8_t)*c < 128; c++) {
        const GAME_TEXT_GLYPH *const glyph = GameText_Find(font, (uint32_t)*c);
        FAIL_IF(
            glyph == nullptr || glyph->width != 8, "Missing item popup glyph");
    }
    const GAME_TEXT_GLYPH *const one = GameText_Find(font, 0xff11);
    const GAME_TEXT_GLYPH *const ten_thousand = GameText_Find(font, 0x842c);
    FAIL_IF(
        one == nullptr || one->width != 16 || ten_thousand == nullptr
            || ten_thousand->width != 16,
        "Missing item cap glyph");
    return OK;
}

void Items_Add(const int16_t left, const int16_t top, const bool bomb)
{
    ITEM *const slots = bomb ? g_Items.bombs : g_Items.points;
    const int32_t count = bomb ? ITEMS_BOMB_COUNT : ITEMS_POINT_COUNT;
    for (int32_t i = 0; i < count; i++) {
        if (slots[i].flag == ITEM_FREE) {
            slots[i] = (ITEM) { .left = left,
                                .top = top,
                                .velocity_y = 2,
                                .flag = ITEM_SPLASH,
                                .radius_or_time = 4 };
            return;
        }
    }
}

void Items_Reset(void)
{
    for (int32_t i = 0; i < ITEMS_BOMB_COUNT; i++) {
        g_Items.bombs[i].flag = ITEM_FREE;
    }
    for (int32_t i = 0; i < ITEMS_POINT_COUNT; i++) {
        g_Items.points[i].flag = ITEM_FREE;
    }
}

void Items_Render(const PTN *const sprites)
{
    for (int32_t i = 0; i < ITEMS_BOMB_COUNT + ITEMS_POINT_COUNT; i++) {
        const bool bomb = i < ITEMS_BOMB_COUNT;
        const ITEM *const item =
            bomb ? &g_Items.bombs[i] : &g_Items.points[i - ITEMS_BOMB_COUNT];
        if (M_Visible(item)) {
            Sprite_PtnPut(
                sprites, bomb ? ITEM_BOMB_IMAGE : ITEM_POINT_IMAGE, -1,
                item->left, item->top);
        }
    }
}

void Items_UpdateAndRender(const ITEM_CONTEXT *const context)
{
    for (int32_t i = 0; i < ITEMS_BOMB_COUNT; i++) {
        M_Update(&g_Items.bombs[i], context, true);
    }
    for (int32_t i = 0; i < ITEMS_POINT_COUNT; i++) {
        M_Update(&g_Items.points[i], context, false);
    }
}
