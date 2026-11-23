#include "th01/stage.h"
#include "th01/rng.h"

#include "pc98/zun/math16.h"
#include "pc98/zun/master_rng.h"

static void M_ScorePopups(const STAGE_CONTEXT *const context)
{
    STAGE_OBJECTS *const objects = context->objects;
    for (int32_t i = 0; i < objects->card_count; i++) {
        STAGE_CARD *const card = &objects->cards[i];
        if (card->score == 0 || card->flip_frame < 15) {
            continue;
        }
        const int16_t popup_y =
            card->flip_frame >= 21 ? card->flip_frame - 20 : 0;
        const int16_t offset = card->score > 10000 ? -8
            : card->score > 1000                   ? 0
                                                   : 8;
        if (popup_y != 0) {
            Sprite_Restore16(
                card->left + offset, card->top + 8 - (popup_y - 1), 40, 16);
        }
        if (card->flip_frame >= 28) {
            card->score = 0;
        } else {
            char text[6];
            int32_t count = 0;
            uint32_t value = card->score;
            do {
                text[count++] = (char)('0' + value % 10);
                value /= 10;
            } while (value != 0);
            for (int32_t x = 0; x < count / 2; x++) {
                const char tmp = text[x];
                text[x] = text[count - 1 - x];
                text[count - 1 - x] = tmp;
            }
            text[count] = '\0';
            const GAME_TEXT_STYLE style = { .weight = GAME_TEXT_BOLD };
            GameText_Draw(
                context->font, text, card->left + offset,
                card->top + 8 - popup_y, 7, &style);
        }
    }
}

RESULT Stage_ValidateScoreFont(const GAME_TEXT_FONT *const font)
{
    FAIL_IF(font == nullptr, "Missing stage score font");
    for (uint32_t c = '0'; c <= '9'; c++) {
        const GAME_TEXT_GLYPH *const glyph = GameText_Find(font, c);
        FAIL_IF(
            glyph == nullptr || glyph->width != 8,
            "Missing halfwidth stage score digit");
    }
    return OK;
}

void Stage_HitTestCards(const STAGE_CONTEXT *const context)
{
    STAGE_OBJECTS *const objects = context->objects;
    STAGE_GAMEPLAY *const game = context->gameplay;
    for (int32_t i = 0; i < objects->card_count; i++) {
        STAGE_CARD *const card = &objects->cards[i];
        const int32_t dx = card->left - g_Orb.left;
        const int32_t dy = card->top - g_Orb.top;
        if (((dx > -24 && dx < 24 && dy > -24 && dy < 24)
             && card->flag == STAGE_CARD_ALIVE)
            || (context->test_damage && card->flag == STAGE_CARD_ALIVE)
            || (g_Player.bomb_damaging
                && g_Player.bomb_frame % (uint32_t)objects->card_count
                    == (uint32_t)i
                && MasterRng_Next(&g_Rng) % 4 != 0
                && card->flag == STAGE_CARD_ALIVE)) {
            card->flag = STAGE_CARD_FLIPPING;
            card->score = (uint32_t)(context->stage_id / 5 * 100 + 100)
                + (uint32_t)(int32_t)Math16_Wrap(
                      (int32_t)game->combo * game->combo)
                    * (context->difficulty == SETTINGS_DIFFICULTY_LUNATIC
                           ? 35u
                           : 20u);
            if (card->score > 25600) {
                card->score = 25600;
            }
            game->score += card->score;
            game->combo = Math16_Wrap(game->combo + 1);
            if (game->maximum_combo < game->combo) {
                game->maximum_combo = game->combo;
            }
            context->hud_score_and_combo(context->user);
            const uint8_t previous_cycle = game->flip_cycle++;
            if (previous_cycle % 10 == 0) {
                const bool bomb = game->flip_cycle >= 140;
                if (bomb) {
                    game->flip_cycle = 1;
                }
                context->item_add(
                    context->user, i, bomb && !g_Player.bomb_damaging);
            }
            context->effect(context->user, 2);
        }
    }
}

void Stage_UpdateCards(const STAGE_CONTEXT *const context)
{
    STAGE_OBJECTS *const objects = context->objects;
    M_ScorePopups(context);
    int32_t removed = 0;
    for (int32_t i = 0; i < objects->card_count; i++) {
        STAGE_CARD *const card = &objects->cards[i];
        if (card->flag == STAGE_CARD_FLIPPING) {
            if (card->flip_frame < 25 && card->flip_frame % 6 == 0) {
                Video_SelectDrawingPage(1);
                StageObjects_PutBackgroundAndObject(
                    objects, context->sprites, card->left, card->top,
                    StageObjects_CardImage(card->hp, card->flip_frame), i);
                Video_SelectDrawingPage(0);
                StageObjects_PutBackgroundAndObject(
                    objects, context->sprites, card->left, card->top,
                    StageObjects_CardImage(card->hp, card->flip_frame), i);
            }
            card->flip_frame++;
            if (card->flip_frame >= 24) {
                if (context->difficulty == SETTINGS_DIFFICULTY_LUNATIC
                    && card->flip_frame == 25) {
                    context->pellet_group(
                        context->user,
                        (STAGE_PELLET_SPAWN) {
                            .left = card->left + 12,
                            .top = card->top + 12,
                            .group = context->stage_num < 10
                                ? STAGE_PELLET_1_AIMED
                                : STAGE_PELLET_1_RANDOM_NARROW_AIMED,
                            .speed = 56,
                        });
                }
                if (card->flip_frame >= 30) {
                    card->flag =
                        card->hp == 0 ? STAGE_CARD_REMOVED : STAGE_CARD_ALIVE;
                    card->flip_frame = 0;
                    if (card->hp != 0) {
                        card->hp--;
                    }
                }
            }
        }
        removed += card->flag == STAGE_CARD_REMOVED;
    }
    if (removed == objects->card_count) {
        context->gameplay->cleared = true;
        g_Player.is_hit = true;
    }
}
