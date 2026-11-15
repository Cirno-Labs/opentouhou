#include "th07/replay.h"

#include "th07/game.h"
#include "th07/game_manager.h"
#include "th07/player.h"
#include "th07/stubs.h"

#include "core/base/log.h"
#include "core/base/memory.h"
#include "core/math/x87.h"

#include <limits.h>
#include <string.h>

#define M_PLAY_PRIORITY 5
#define M_MARK_PRIORITY 6
#define M_RECORD_PRIORITY 16
#define M_RESTART_PRIORITY 17
#define M_DRAW_PRIORITY 14
#define M_FPS_FRAMES 30
// The port runs at the rate the original aims for and keeps no clock
// streak, so recording stores 60 without the fast clock bit.
#define M_RECORD_FPS 60
#define M_CONFIG_LIVES 0x1c
#define M_CONFIG_SLOWDOWN 0x25
#define M_BORDER_PLUS 50000

typedef struct {
    REPLAY_FILE *file;
    REPLAY_MODE mode;
    char *path;
    int32_t frame;
    int32_t cursor;
    int32_t fps_cursor;
    int32_t capacities[8];
    int32_t fps_capacities[8];
    uint16_t events;
    bool past_end_logged;
    CHAIN_ELEM calc;
    CHAIN_ELEM draw;
    CHAIN_ELEM restart;
    CHAIN_ELEM mark;
} M_REPLAY;

static M_REPLAY *m_Replay;

// Extra and Phantasm share slot 7.
static int32_t M_Slot(void)
{
    return g_GameManager.run.stage >= 7 ? 7 : g_GameManager.run.stage;
}

static bool M_IsReady(const M_REPLAY *const replay)
{
    return replay->file != nullptr && g_GameManager.run.stage >= 1
        && g_GameManager.run.stage <= 8
        && replay->file->stages[M_Slot()] != nullptr;
}

static void *M_Grow(
    void *const items, int32_t *const capacity, const int32_t count,
    const size_t size)
{
    if (*capacity >= count) {
        return items;
    }
    *capacity = count <= INT32_MAX / 2 ? count * 2 : count;
    return Mem_ReallocArr(items, (size_t)*capacity, size);
}

static RESULT M_Load(M_REPLAY *const replay)
{
    size_t size;
    const uint8_t *const data = Replay_FindFile(replay->path, &size);
    FAIL_IF(data == nullptr, "no replay is kept for %s", replay->path);
    READER reader;
    Reader_OpenBuffer(&reader, data, size, nullptr);
    const RESULT result = Replay_Read(&reader, &replay->file);
    Reader_Close(&reader);
    MUST(result, "%s", replay->path);
    return OK;
}

static RESULT M_PlayAdded(void *const arg)
{
    M_REPLAY *const replay = arg;
    replay->frame = 0;
    replay->past_end_logged = false;
    if (replay->file == nullptr) {
        MUST(M_Load(replay));
    }
    const int32_t stage_number = g_GameManager.run.stage;
    FAIL_IF(
        stage_number < 1 || stage_number > 8, "no replay stage %d",
        stage_number);
    const REPLAY_FILE *const file = replay->file;
    const REPLAY_STAGE *const stage = file->stages[M_Slot()];
    FAIL_IF(stage == nullptr, "replay has no slot %d", M_Slot());
    GAME_MANAGER *const gm = &g_GameManager;
    gm->run.character = file->shot / 2;
    gm->run.shot_type = file->shot % 2;
    gm->run.shot_index = file->shot;
    gm->run.difficulty = file->difficulty;
    gm->player.point_items_total = stage->point_items_total;
    gm->rank.value = stage->rank;
    gm->player.lives = (float)stage->lives;
    GameManager_DrawChecksumKeys();
    gm->player.bombs = (float)stage->bombs;
    gm->player.power = (float)stage->power;
    GameManager_DrawChecksumKeys();
    gm->player.graze_total = stage->graze_total;
    replay->cursor = 0;
    gm->player.power_item_combo = (int8_t)stage->power_item_combo;
    gm->cherry.value = stage->cherry + gm->cherry.offset;
    gm->cherry.max = stage->cherry_max + gm->cherry.offset;
    gm->cherry.plus = stage->cherry_plus + gm->cherry.offset;
    if (gm->cherry.plus >= gm->cherry.offset + M_BORDER_PLUS) {
        gm->cherry.plus = gm->cherry.offset + M_BORDER_PLUS;
        Player_StartBorder();
    }
    // The original puts the replay's whole configuration in place.
    gm->run.start_lives = file->config[M_CONFIG_LIVES];
    gm->run.score_disabled = file->config[M_CONFIG_SLOWDOWN] != 0;
    Rng_SetSeed(&g_Rng, stage->rng_seed);
    gm->stats.spell_cards_captured = stage->spell_cards;
    gm->player.extends = stage->point_extends;
    gm->player.next_extend_items = stage->next_extend_points;
    replay->fps_cursor = 0;
    if (stage_number >= 2 && stage_number <= 6
        && file->stages[stage_number - 1] != nullptr) {
        gm->score.value = (uint32_t)file->stages[stage_number - 1]->score;
        gm->score.shown = gm->score.value;
    }
    return OK;
}

static RESULT M_RecordAdded(void *const arg)
{
    M_REPLAY *const replay = arg;
    GAME_MANAGER *const gm = &g_GameManager;
    replay->frame = 0;
    const int32_t stage_number = gm->run.stage;
    FAIL_IF(
        stage_number < 1 || stage_number > 8, "no replay stage %d",
        stage_number);
    if (replay->file == nullptr) {
        REPLAY_FILE *const file = Mem_AllocZ(sizeof(REPLAY_FILE));
        replay->file = file;
        file->shot = gm->run.shot_index;
        file->difficulty = (uint8_t)gm->run.difficulty;
        // The original copies four bytes of "NO NAME".
        memcpy(file->name, "NO N", 4);
        memcpy(file->exe_version, "0100b", 6);
        file->config[M_CONFIG_LIVES] = gm->run.start_lives;
        file->config[M_CONFIG_SLOWDOWN] = gm->run.score_disabled;
    } else if (replay->file->stages[stage_number - 1] != nullptr) {
        // Stage n's previous slot is n - 1, so Phantasm (stage 8) writes
        // into its own slot, as the original does.
        replay->file->stages[stage_number - 1]->score =
            (int32_t)gm->score.value;
    }
    const int32_t slot = M_Slot();
    REPLAY_FILE *const file = replay->file;
    if (file->stages[slot] != nullptr) {
        Mem_Free(file->stages[slot]->frames);
        Mem_Free(file->stages[slot]);
    }
    Mem_Free(file->fps[slot]);
    file->fps[slot] = nullptr;
    file->fps_counts[slot] = 0;
    replay->capacities[slot] = 0;
    replay->fps_capacities[slot] = 0;
    REPLAY_STAGE *const stage = Mem_AllocZ(sizeof(REPLAY_STAGE));
    file->stages[slot] = stage;
    stage->graze_total = gm->player.graze_total;
    stage->bombs = (uint8_t)X87_Ftol(gm->player.bombs);
    stage->lives = (uint8_t)X87_Ftol(gm->player.lives);
    stage->power = (uint8_t)X87_Ftol(gm->player.power);
    stage->rank = (uint8_t)gm->rank.value;
    stage->point_items_total = gm->player.point_items_total;
    stage->rng_seed = gm->run.rng_seed;
    stage->power_item_combo = (uint8_t)gm->player.power_item_combo;
    stage->cherry = gm->cherry.value - gm->cherry.offset;
    stage->cherry_max = gm->cherry.max - gm->cherry.offset;
    stage->cherry_plus = gm->cherry.plus - gm->cherry.offset;
    stage->spell_cards = (uint8_t)gm->stats.spell_cards_captured;
    stage->point_extends = gm->player.extends;
    stage->next_extend_points = gm->player.next_extend_items;
    replay->cursor = 0;
    replay->fps_cursor = 0;
    stage->frames = M_Grow(
        stage->frames, &replay->capacities[slot], 1, sizeof(REPLAY_FRAME));
    stage->frames[0] = (REPLAY_FRAME) {};
    stage->frame_count = 1;
    return OK;
}

static CHAIN_STATUS M_Mark(void *const arg)
{
    M_REPLAY *const replay = arg;
    replay->events = 0;
    g_Rng.generated = 0;
    if (g_GameManager.mode.pause_requested) {
        replay->events |= REPLAY_EVENT_PAUSE;
    }
    g_GameManager.mode.pause_requested = false;
    return CHAIN_CONTINUE;
}

static CHAIN_STATUS M_Record(void *const arg)
{
    M_REPLAY *const replay = arg;
    if (!g_GameManager.mode.playing || !M_IsReady(replay)) {
        return CHAIN_CONTINUE;
    }
    g_Buttons.previous_play_word = g_Buttons.play_word;
    g_Buttons.play_word = g_Buttons.word;
    if (g_GameManager.run.score_disabled) {
        return CHAIN_CONTINUE;
    }
    const int32_t slot = M_Slot();
    REPLAY_FILE *const file = replay->file;
    REPLAY_STAGE *const stage = file->stages[slot];
    replay->cursor++;
    stage->frames = M_Grow(
        stage->frames, &replay->capacities[slot], replay->cursor + 1,
        sizeof(REPLAY_FRAME));
    stage->frames[replay->cursor] =
        (REPLAY_FRAME) { g_Buttons.word, replay->events };
    stage->frame_count = replay->cursor + 1;
    if (replay->frame % M_FPS_FRAMES == 0) {
        // Each sample also writes the byte after it, which the next
        // sample overwrites.
        file->fps[slot] = M_Grow(
            file->fps[slot], &replay->fps_capacities[slot],
            replay->fps_cursor + 2, 1);
        file->fps[slot][replay->fps_cursor] = M_RECORD_FPS;
        file->fps[slot][replay->fps_cursor + 1] = M_RECORD_FPS;
        file->fps_counts[slot] = replay->fps_cursor + 2;
        replay->fps_cursor++;
    }
    replay->frame++;
    return CHAIN_CONTINUE;
}

static CHAIN_STATUS M_Play(void *const arg)
{
    M_REPLAY *const replay = arg;
    if (!g_GameManager.mode.playing || g_GameManager.run.score_disabled
        || !M_IsReady(replay)) {
        return CHAIN_CONTINUE;
    }
    const int32_t slot = M_Slot();
    const REPLAY_FILE *const file = replay->file;
    const REPLAY_STAGE *const stage = file->stages[slot];
    if (replay->cursor < stage->frame_count) {
        Buttons_Play(&g_Buttons, stage->frames[replay->cursor].buttons);
        replay->cursor++;
    } else {
        // The original reads on into whatever follows the stage.
        if (!replay->past_end_logged) {
            LOG_WARNING("replay slot %d has no more frames", slot);
            replay->past_end_logged = true;
        }
        Buttons_Play(&g_Buttons, 0);
    }
    // Each sample restores the byte after the current one, the rate 30
    // calcs later. Past the last byte the rate stays.
    if (replay->frame % M_FPS_FRAMES == 0) {
        if (replay->fps_cursor + 1 < file->fps_counts[slot]) {
            g_GameManager.mode.replay_fps =
                file->fps[slot][replay->fps_cursor + 1] & 0x7f;
        }
        replay->fps_cursor++;
    }
    replay->frame++;
    return CHAIN_CONTINUE;
}

static CHAIN_STATUS M_Restart(void *const arg)
{
    const M_REPLAY *const replay = arg;
    if (!g_GameManager.mode.playing || !M_IsReady(replay)) {
        return CHAIN_CONTINUE;
    }
    if (Gui_IsMsgRunning() && Gui_IsMsgSkippable() && replay->frame % 3 != 2) {
        return CHAIN_RESTART;
    }
    if (g_GameManager.mode.replay_speed == 2 && !Enemy_IsBossActive()
        && replay->frame % 5 != 4) {
        return CHAIN_RESTART;
    }
    return CHAIN_CONTINUE;
}

static CHAIN_STATUS M_Draw(void *const arg)
{
    return CHAIN_CONTINUE;
}

static void M_Deleted(void *const arg)
{
    M_REPLAY *const replay = arg;
    Chain_Remove(&replay->draw);
    Chain_Remove(&replay->restart);
    Chain_Remove(&replay->mark);
    Replay_Free(replay->file);
    Mem_Free(replay->path);
    Mem_Free(replay);
    m_Replay = nullptr;
}

RESULT Replay_Register(const REPLAY_MODE mode, const char *const path)
{
    FAIL_IF(
        mode != REPLAY_RECORD && mode != REPLAY_PLAY, "unknown replay mode %d",
        mode);
    g_Buttons.previous_play_word = 0;
    g_Buttons.play_word = 0;
    if (m_Replay != nullptr) {
        if (mode == REPLAY_RECORD) {
            Result_Ignore(M_RecordAdded(m_Replay));
            return OK;
        }
        return M_PlayAdded(m_Replay);
    }
    FAIL_IF(mode == REPLAY_PLAY && path == nullptr, "replay path is missing");
    M_REPLAY *const replay = Mem_AllocZ(sizeof(M_REPLAY));
    m_Replay = replay;
    replay->mode = mode;
    replay->path = path != nullptr ? Mem_Dup(path, strlen(path) + 1) : nullptr;
    if (mode == REPLAY_RECORD) {
        Chain_InitElem(&replay->calc, M_Record, replay);
        replay->calc.added = M_RecordAdded;
        replay->calc.deleted = M_Deleted;
        MUST(Chain_AddCalc(&g_Chain, &replay->calc, M_RECORD_PRIORITY));
        Chain_InitElem(&replay->mark, M_Mark, replay);
        MUST(Chain_AddCalc(&g_Chain, &replay->mark, M_MARK_PRIORITY));
    } else {
        Chain_InitElem(&replay->calc, M_Play, replay);
        replay->calc.added = M_PlayAdded;
        replay->calc.deleted = M_Deleted;
        MUST(Chain_AddCalc(&g_Chain, &replay->calc, M_PLAY_PRIORITY));
        Chain_InitElem(&replay->restart, M_Restart, replay);
        MUST(Chain_AddCalc(&g_Chain, &replay->restart, M_RESTART_PRIORITY));
    }
    Chain_InitElem(&replay->draw, M_Draw, replay);
    return Chain_AddDraw(&g_Chain, &replay->draw, M_DRAW_PRIORITY);
}

void Replay_EndStage(void)
{
    M_REPLAY *const replay = m_Replay;
    if (replay == nullptr || replay->mode != REPLAY_RECORD
        || !M_IsReady(replay)) {
        return;
    }
    const int32_t slot = M_Slot();
    REPLAY_STAGE *const stage = replay->file->stages[slot];
    replay->cursor++;
    stage->frames = M_Grow(
        stage->frames, &replay->capacities[slot], replay->cursor + 1,
        sizeof(REPLAY_FRAME));
    stage->frames[replay->cursor] = (REPLAY_FRAME) {};
    stage->frame_count = replay->cursor + 1;
}

void Replay_Discard(void)
{
    if (m_Replay != nullptr) {
        Chain_Remove(&m_Replay->calc);
    }
}

void Replay_MarkEvent(const REPLAY_EVENT event)
{
    if (m_Replay != nullptr && m_Replay->mode == REPLAY_RECORD) {
        m_Replay->events |= (uint16_t)event;
    }
}
