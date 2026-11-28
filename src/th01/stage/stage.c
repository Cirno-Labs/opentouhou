#include "th01/stage/stage.h"

#include "th01/boss.h"
#include "th01/hud.h"
#include "th01/items.h"
#include "th01/pellets.h"
#include "th01/rng.h"
#include "th01/session.h"
#include "th01/sound.h"
#include "th01/stage/private.h"

#include "pc98/zun/master_rng.h"
#include "pc98/zun/math16.h"

#include <string.h>

static STAGE_GAMEPLAY m_Stage;
const STAGE_GAMEPLAY *const g_Stage = &m_Stage;
STAGE_GAMEPLAY *const p_Stage = &m_Stage;

static void M_ScorePopups(const GAME_TEXT_FONT *const font)
{
    const STAGE_OBJECTS *const objects = g_StageObjects;
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
                font, text, card->left + offset, card->top + 8 - popup_y, 7,
                &style);
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

void Stage_HitTestCards(void)
{
    const STAGE_OBJECTS *const objects = g_StageObjects;
    STAGE_GAMEPLAY *const game = p_Stage;
    for (int32_t i = 0; i < objects->card_count; i++) {
        STAGE_CARD *const card = &objects->cards[i];
        const int32_t dx = card->left - g_Orb->left;
        const int32_t dy = card->top - g_Orb->top;
        if (((dx > -24 && dx < 24 && dy > -24 && dy < 24)
             && card->flag == STAGE_CARD_ALIVE)
            || (*g_TestDamage && card->flag == STAGE_CARD_ALIVE)
            || (g_Player->bomb_damaging
                && g_Player->bomb_frame % (uint32_t)objects->card_count
                    == (uint32_t)i
                && MasterRng_Next(&g_Rng) % 4 != 0
                && card->flag == STAGE_CARD_ALIVE)) {
            card->flag = STAGE_CARD_FLIPPING;
            card->score = (uint32_t)(g_Session->stage_id / 5 * 100 + 100)
                + (uint32_t)(int32_t)Math16_Wrap(
                      (int32_t)game->combo * game->combo)
                    * ((SETTINGS_DIFFICULTY)g_Session->settings.difficulty
                               == SETTINGS_DIFFICULTY_LUNATIC
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
            Hud_ScoreAndCombo();
            const uint8_t previous_cycle = game->flip_cycle++;
            if (previous_cycle % 10 == 0) {
                const bool bomb = game->flip_cycle >= 140;
                if (bomb) {
                    game->flip_cycle = 1;
                }
                Items_Add(
                    card->left, card->top, bomb && !g_Player->bomb_damaging);
            }
            Sound_Effect(SOUND_EFFECT_HIT);
        }
    }
}

void Stage_UpdateCards(
    const PTN *const sprites, const GAME_TEXT_FONT *const font)
{
    const STAGE_OBJECTS *const objects = g_StageObjects;
    M_ScorePopups(font);
    int32_t removed = 0;
    for (int32_t i = 0; i < objects->card_count; i++) {
        STAGE_CARD *const card = &objects->cards[i];
        if (card->flag == STAGE_CARD_FLIPPING) {
            if (card->flip_frame < 25 && card->flip_frame % 6 == 0) {
                Video_SelectDrawingPage(1);
                StageObjects_PutBackgroundAndObject(
                    sprites, card->left, card->top,
                    StageObjects_CardImage(card->hp, card->flip_frame), i);
                Video_SelectDrawingPage(0);
                StageObjects_PutBackgroundAndObject(
                    sprites, card->left, card->top,
                    StageObjects_CardImage(card->hp, card->flip_frame), i);
            }
            card->flip_frame++;
            if (card->flip_frame >= 24) {
                if ((SETTINGS_DIFFICULTY)g_Session->settings.difficulty
                        == SETTINGS_DIFFICULTY_LUNATIC
                    && card->flip_frame == 25) {
                    Pellets_AddGroup(
                        g_Session->settings.difficulty, card->left + 12,
                        card->top + 12,
                        (PELLET_GROUP)(g_Session->stage_id + 1 < 10
                                           ? STAGE_PELLET_1_AIMED
                                           : STAGE_PELLET_1_RANDOM_NARROW_AIMED),
                        56);
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
        p_Stage->cleared = true;
        Player_Hit();
    }
}

void Stage_Init(void)
{
    memset(&m_Stage, 0, sizeof(m_Stage));
    p_Stage->first_stage = true;
}

void Stage_AddScore(const uint32_t points)
{
    p_Stage->score += points;
}

void Stage_Leave(void)
{
    p_Stage->frame = 0;
    p_Stage->boss = BOSS_NONE;
}

void Stage_SetBoss(const BOSS boss)
{
    p_Stage->boss = boss;
}

void Stage_SetFirstStage(const bool first_stage)
{
    p_Stage->first_stage = first_stage;
}

void Stage_SetFrame(const uint32_t frame)
{
    p_Stage->frame = frame;
}

void Stage_AdvanceFrame(void)
{
    p_Stage->frame++;
}

void Stage_SetCombo(const int16_t combo)
{
    p_Stage->combo = combo;
}

void Stage_SetMaximumCombo(const int16_t maximum_combo)
{
    p_Stage->maximum_combo = maximum_combo;
}

void Stage_MarkCleared(void)
{
    p_Stage->cleared = true;
}

bool Stage_TakeCleared(void)
{
    const bool cleared = p_Stage->cleared;
    p_Stage->cleared = false;
    return cleared;
}

void Stage_SetTimerInitialized(const bool timer_initialized)
{
    p_Stage->timer_initialized = timer_initialized;
}

void Stage_SetTime(const uint16_t time)
{
    p_Stage->time = time;
}

void Stage_SetGameCleared(void)
{
    p_Stage->game_cleared = true;
}

void Stage_SyncScore(const uint32_t score)
{
    p_Stage->score = score;
}

void Stage_SetFlipCycle(const uint8_t flip_cycle)
{
    p_Stage->flip_cycle = flip_cycle;
}
