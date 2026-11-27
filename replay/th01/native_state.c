#include "replay/th01/native_state.h"

#include "replay/harness/state.h"
#include "th01/boss.h"
#include "th01/ending.h"
#include "th01/main.h"
#include "th01/opening.h"
#include "th01/palette.h"
#include "th01/pellets.h"
#include "th01/player.h"
#include "th01/rng.h"
#include "th01/session.h"
#include "th01/settings.h"
#include "th01/stage.h"
#include "th01/vsync_irq.h"

#include <string.h>

static uint32_t M_I32(const int32_t value)
{
    return (uint32_t)value;
}

static uint32_t M_HashCards(const STAGE_OBJECTS *const objects)
{
    uint32_t hash = STATE_HASH_INIT;
    for (int32_t i = 0; i < objects->card_count; i++) {
        const STAGE_CARD *const card = &objects->cards[i];
        hash = State_Hash(hash, (uint32_t)i);
        hash = State_Hash(hash, card->flag | (uint32_t)card->hp << 8);
        hash = State_Hash(hash, M_I32(card->flip_frame));
    }
    return hash;
}

static uint32_t M_HashPellets(const PELLETS *const pellets, uint32_t *count)
{
    uint32_t hash = STATE_HASH_INIT;
    *count = 0;
    for (uint32_t i = 0; i < PELLETS_COUNT; i++) {
        const PELLET *const pellet = &pellets->slots[i];
        if (!pellet->moving) {
            continue;
        }
        (*count)++;
        hash = State_Hash(hash, i);
        hash = State_Hash(
            hash,
            (uint32_t)pellet->moving | (uint32_t)pellet->motion_type << 8);
        hash = State_Hash(hash, M_I32(pellet->left));
        hash = State_Hash(hash, M_I32(pellet->top));
        hash = State_Hash(hash, M_I32(pellet->velocity.x));
        hash = State_Hash(hash, M_I32(pellet->velocity.y));
    }
    return hash;
}

static uint32_t M_HashShots(const SHOTS *const shots, uint32_t *count)
{
    uint32_t hash = STATE_HASH_INIT;
    *count = 0;
    for (uint32_t i = 0; i < SHOTS_COUNT; i++) {
        const SHOT *const shot = &shots->slots[i];
        if (!shot->moving) {
            continue;
        }
        (*count)++;
        hash = State_Hash(hash, i);
        hash = State_Hash(hash, M_I32(shot->left));
        hash = State_Hash(hash, M_I32(shot->top));
    }
    return hash;
}

static void M_Double(const double value, uint32_t *const lo, uint32_t *hi)
{
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    *lo = (uint32_t)bits;
    *hi = (uint32_t)(bits >> 32);
}

static void M_Boss(uint32_t *const row)
{
    if (g_Stage.boss != BOSS_NONE && (*g_Boss) != nullptr) {
        row[TH01_COLUMN_boss_hp] = M_I32((*g_Boss)->hp);
        row[TH01_COLUMN_boss_phase] = M_I32((*g_Boss)->phase);
        row[TH01_COLUMN_boss_phase_frame] = M_I32((*g_Boss)->frame);
    }
}

static void M_Gameplay(uint32_t *const row)
{
    row[TH01_COLUMN_vsync_count1] = M_I32(g_VSync->count1);
    row[TH01_COLUMN_frame_rand] = g_Session.frame_rand;
    row[TH01_COLUMN_rng] = g_Rng;
    row[TH01_COLUMN_game_frames] = g_Stage.frame;
    row[TH01_COLUMN_stage] = (uint32_t)g_Session.stage_id;
    row[TH01_COLUMN_route] = g_Session.route;
    row[TH01_COLUMN_score] = g_Stage.score;
    row[TH01_COLUMN_lives] = M_I32(g_Session.lives);
    row[TH01_COLUMN_bombs] = M_I32(g_Player->remaining_bombs);
    row[TH01_COLUMN_pellet_speed] = M_I32(g_Pellets->speed);
    row[TH01_COLUMN_stage_time] = g_Stage.time;

    const PLAYER *const player = g_Player;
    row[TH01_COLUMN_player_left] = M_I32(player->left);
    row[TH01_COLUMN_player_flags] = (uint32_t)player->is_hit
        | (uint32_t)player->invincible << 1 | (uint32_t)player->deflecting << 2
        | (uint32_t)player->sliding << 3;
    row[TH01_COLUMN_invincibility_time] = M_I32(player->invincibility_time);
    row[TH01_COLUMN_bomb_frame] = player->bomb_frame;

    const ORB *const orb = g_Orb;
    row[TH01_COLUMN_orb_left] = M_I32(orb->left);
    row[TH01_COLUMN_orb_top] = M_I32(orb->top);
    row[TH01_COLUMN_orb_velocity_x] = M_I32(orb->velocity_x);
    M_Double(
        orb->velocity_y, &row[TH01_COLUMN_orb_velocity_y_lo],
        &row[TH01_COLUMN_orb_velocity_y_hi]);
    M_Double(
        orb->force, &row[TH01_COLUMN_orb_force_lo],
        &row[TH01_COLUMN_orb_force_hi]);
    row[TH01_COLUMN_orb_force_frame] = M_I32(orb->force_frame);

    row[TH01_COLUMN_card_combo] = M_I32(g_Stage.combo);
    row[TH01_COLUMN_card_flip_cycle] = g_Stage.flip_cycle;
    row[TH01_COLUMN_card_count] = (uint32_t)g_StageObjects->card_count;
    row[TH01_COLUMN_card_hash] = M_HashCards(g_StageObjects);

    row[TH01_COLUMN_boss] = g_Stage.boss;
    M_Boss(row);

    uint32_t pellet_count;
    row[TH01_COLUMN_pellet_hash] = M_HashPellets(g_Pellets, &pellet_count);
    row[TH01_COLUMN_pellet_count] = pellet_count;
    uint32_t shot_count;
    row[TH01_COLUMN_shot_hash] = M_HashShots(g_Shots, &shot_count);
    row[TH01_COLUMN_shot_count] = shot_count;
}

void Th01Native_Sample(void *const context, uint32_t *const row)
{
    const TH01_NATIVE *const native = context;
    memset(row, 0, TH01_COLUMN_COUNT * sizeof(*row));
    row[TH01_COLUMN_buttons] = native->buttons;
    const bool ending_scores = (*g_Th01Scene) == TH01_SCENE_ENDING_SCORE;
    if ((*g_Th01Scene) == TH01_SCENE_LOGO) {
        row[TH01_COLUMN_program] = 1;
    } else if ((*g_Th01Scene) == TH01_SCENE_OPENING) {
        row[TH01_COLUMN_program] = 2;
        row[TH01_COLUMN_vsync_count1] = M_I32((int16_t)g_Opening->count1);
        row[TH01_COLUMN_frame_rand] = g_Opening->frame_rand;
        row[TH01_COLUMN_rng] = g_Opening->random;
        row[TH01_COLUMN_track] = (uint32_t)g_Opening->track;
        row[TH01_COLUMN_track_starts] = g_Opening->track_starts;
    } else if (
        (*g_Th01Scene) == TH01_SCENE_ENDING
        || (*g_Th01Scene) == TH01_SCENE_SWITCH || ending_scores) {
        const END_RESULT *const result = g_EndResult;
        row[TH01_COLUMN_program] = 4;
        row[TH01_COLUMN_vsync_count1] = M_I32(g_VSync->count1);
        row[TH01_COLUMN_rng] = result->random;
        row[TH01_COLUMN_route] = (uint32_t)result->route;
        row[TH01_COLUMN_score] = result->score;
    } else {
        row[TH01_COLUMN_program] = 3;
        M_Gameplay(row);
    }
    row[TH01_COLUMN_difficulty] = (uint32_t)g_Settings.difficulty;
    row[TH01_COLUMN_lives_extra] = (uint32_t)g_Settings.lives_extra;
    row[TH01_COLUMN_start_bombs] = (uint32_t)g_Settings.bombs;
    row[TH01_COLUMN_music] = (uint32_t)g_Settings.music;
}
