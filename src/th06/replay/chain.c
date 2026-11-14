#include "th06/replay.h"

#include "th06/game.h"
#include "th06/game_manager.h"
#include "th06/gui.h"
#include "th06/replay/format.h"

#include "core/base/assert.h"
#include "core/base/memory.h"

#include <limits.h>
#include <string.h>

#define M_END_FRAME 9999999
#define M_INPUT_MASK 0x1f7

typedef struct {
    REPLAY_FILE *file;
    REPLAY_MODE mode;
    char *path;
    int32_t frame;
    int32_t cursor;
    int32_t capacities[8];
    CHAIN_ELEM calc;
    CHAIN_ELEM draw;
    CHAIN_ELEM restart;
} M_REPLAY;

static M_REPLAY *m_Replay;

static REPLAY_STAGE *M_Stage(const M_REPLAY *const replay)
{
    const int32_t stage = g_GameManager.run.stage;
    ASSERT(stage >= 1 && stage <= 7);
    return replay->file->stages[stage];
}

static bool M_IsReady(const M_REPLAY *const replay)
{
    return replay->file != nullptr && g_GameManager.run.stage >= 1
        && g_GameManager.run.stage <= 7 && M_Stage(replay) != nullptr;
}

static void M_EnsureFrames(M_REPLAY *const replay, const int32_t count)
{
    const int32_t index = g_GameManager.run.stage;
    int32_t *const capacity = &replay->capacities[index];
    if (*capacity >= count) {
        return;
    }
    *capacity = count <= INT32_MAX / 2 ? count * 2 : count;
    REPLAY_STAGE *const stage = M_Stage(replay);
    stage->frames =
        Mem_ReallocArr(stage->frames, (size_t)*capacity, sizeof(REPLAY_FRAME));
}

static RESULT M_Load(M_REPLAY *const replay)
{
    size_t size;
    const uint8_t *const data = Replay_FindFile(replay->path, &size);
    FAIL_IF(data == nullptr, "no replay is kept for %s", replay->path);
    READER reader;
    Reader_OpenBuffer(&reader, data, size, nullptr);
    MUST(Replay_Read(&reader, &replay->file), "%s", replay->path);
    for (int32_t i = 1; i <= 7; i++) {
        if (replay->file->stages[i] != nullptr) {
            replay->capacities[i] = replay->file->stages[i]->frame_count;
        }
    }
    return OK;
}

static RESULT M_PlayAdded(void *const arg)
{
    M_REPLAY *const replay = arg;
    replay->frame = 0;
    if (replay->file == nullptr) {
        MUST(M_Load(replay));
    }
    const int32_t index = g_GameManager.run.stage;
    FAIL_IF(index < 1 || index > 7, "no replay stage %d", index);
    REPLAY_STAGE *const stage = replay->file->stages[index];
    FAIL_IF(stage == nullptr, "replay has no stage %d", index);
    g_GameManager.run.character = replay->file->shot / 2;
    g_GameManager.run.shot_type = replay->file->shot % 2;
    g_GameManager.run.difficulty = replay->file->difficulty;
    g_GameManager.player.point_items_total = stage->point_items_total;
    Rng_SetSeed(&g_Rng, stage->rng_seed);
    g_GameManager.rank.value = stage->rank;
    g_GameManager.player.lives = (int8_t)stage->lives;
    g_GameManager.player.bombs = (int8_t)stage->bombs;
    g_GameManager.player.power = stage->power;
    replay->cursor = 0;
    g_GameManager.player.power_item_combo = (int8_t)stage->power_item_combo;
    if (index >= 2 && replay->file->stages[index - 1] != nullptr) {
        g_GameManager.score.shown =
            (uint32_t)replay->file->stages[index - 1]->score;
        g_GameManager.score.value = g_GameManager.score.shown;
    }
    return OK;
}

static RESULT M_RecordAdded(void *const arg)
{
    M_REPLAY *const replay = arg;
    replay->frame = 0;
    const int32_t index = g_GameManager.run.stage;
    FAIL_IF(index < 1 || index > 7, "no replay stage %d", index);
    if (replay->file == nullptr) {
        replay->file = Mem_AllocZ(sizeof(REPLAY_FILE));
        replay->file->shot =
            g_GameManager.run.character * 2 + g_GameManager.run.shot_type;
        replay->file->difficulty = (uint8_t)g_GameManager.run.difficulty;
        memcpy(replay->file->name, "NO N", 4);
    } else {
        REPLAY_STAGE *const previous = replay->file->stages[index - 1];
        FAIL_IF(previous == nullptr, "replay has no previous stage");
        previous->score = (int32_t)g_GameManager.score.value;
    }
    if (replay->file->stages[index] != nullptr) {
        Mem_Free(replay->file->stages[index]->frames);
        Mem_Free(replay->file->stages[index]);
        replay->capacities[index] = 0;
    }
    REPLAY_STAGE *const stage = Mem_AllocZ(sizeof(REPLAY_STAGE));
    replay->file->stages[index] = stage;
    stage->bombs = (uint8_t)g_GameManager.player.bombs;
    stage->lives = (uint8_t)g_GameManager.player.lives;
    stage->power = (uint8_t)g_GameManager.player.power;
    stage->rank = (uint8_t)g_GameManager.rank.value;
    stage->point_items_total = g_GameManager.player.point_items_total;
    stage->rng_seed = g_GameManager.run.rng_seed;
    stage->power_item_combo = (uint8_t)g_GameManager.player.power_item_combo;
    replay->cursor = 0;
    M_EnsureFrames(replay, 1);
    stage->frames[0] = (REPLAY_FRAME) {};
    stage->frame_count = 1;
    return OK;
}

static CHAIN_STATUS M_Record(void *const arg)
{
    M_REPLAY *const replay = arg;
    if (!g_GameManager.mode.playing || !M_IsReady(replay)) {
        return CHAIN_CONTINUE;
    }
    REPLAY_STAGE *const stage = M_Stage(replay);
    const uint16_t buttons = g_Buttons.word & M_INPUT_MASK;
    if (buttons != stage->frames[replay->cursor].buttons) {
        replay->cursor++;
        M_EnsureFrames(replay, replay->cursor + 1);
        stage->frames[replay->cursor] =
            (REPLAY_FRAME) { replay->frame, buttons };
        stage->frame_count = replay->cursor + 1;
    }
    replay->frame++;
    return CHAIN_CONTINUE;
}

static CHAIN_STATUS M_Play(void *const arg)
{
    M_REPLAY *const replay = arg;
    if (!g_GameManager.mode.playing || !M_IsReady(replay)) {
        return CHAIN_CONTINUE;
    }
    const REPLAY_STAGE *const stage = M_Stage(replay);
    while (replay->cursor + 1 < stage->frame_count
           && replay->frame >= stage->frames[replay->cursor + 1].frame) {
        replay->cursor++;
    }
    Buttons_ApplyReplay(&g_Buttons, stage->frames[replay->cursor].buttons);
    replay->frame++;
    return CHAIN_CONTINUE;
}

static CHAIN_STATUS M_Restart(void *const arg)
{
    const M_REPLAY *const replay = arg;
    return M_IsReady(replay) && g_GameManager.mode.playing && Gui_IsMsgRunning()
            && Gui_IsMsgSkippable() && replay->frame % 3 != 2
        ? CHAIN_RESTART
        : CHAIN_CONTINUE;
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
    Chain_InitElem(
        &replay->calc, mode == REPLAY_RECORD ? M_Record : M_Play, replay);
    replay->calc.added = mode == REPLAY_RECORD ? M_RecordAdded : M_PlayAdded;
    replay->calc.deleted = M_Deleted;
    MUST(
        Chain_AddCalc(&g_Chain, &replay->calc, mode == REPLAY_RECORD ? 15 : 5));
    if (mode == REPLAY_PLAY) {
        Chain_InitElem(&replay->restart, M_Restart, replay);
        MUST(Chain_AddCalc(&g_Chain, &replay->restart, 16));
    }
    Chain_InitElem(&replay->draw, M_Draw, replay);
    return Chain_AddDraw(&g_Chain, &replay->draw, 13);
}

void Replay_EndStage(void)
{
    M_REPLAY *const replay = m_Replay;
    if (replay == nullptr || replay->file == nullptr
        || g_GameManager.run.stage < 1 || g_GameManager.run.stage > 7
        || M_Stage(replay) == nullptr) {
        return;
    }
    REPLAY_STAGE *const stage = M_Stage(replay);
    M_EnsureFrames(replay, replay->cursor + 3);
    stage->frames[++replay->cursor] = (REPLAY_FRAME) { replay->frame, 0 };
    stage->frames[++replay->cursor] = (REPLAY_FRAME) { M_END_FRAME, 0 };
    stage->frame_count = replay->cursor + 1;
}

RESULT Replay_Save(WRITER *const writer, const REPLAY_SAVE_INFO *const info)
{
    M_REPLAY *const replay = m_Replay;
    if (replay == nullptr) {
        return OK;
    }
    RESULT result = OK;
    if (replay->mode == REPLAY_RECORD) {
        FAIL_IF(
            writer == nullptr || info == nullptr,
            "replay save information is missing");
        FAIL_IF(
            g_GameManager.run.stage < 1 || g_GameManager.run.stage > 7,
            "no replay stage %d", g_GameManager.run.stage);
        FAIL_IF(
            replay->file == nullptr || M_Stage(replay) == nullptr,
            "replay has no stage to save");
        REPLAY_FORMAT_SAVE prepared;
        MUST(ReplayFormat_Prepare(info, &prepared));
        replay->file->score = g_GameManager.score.shown;
        Replay_EndStage();
        M_Stage(replay)->score = (int32_t)g_GameManager.score.value;
        uint8_t random[4];
        random[0] = (uint8_t)(Rng_U16InRange(&g_Rng, 128) + 64);
        random[1] = (uint8_t)Rng_U16InRange(&g_Rng, 256);
        random[2] = (uint8_t)Rng_U16InRange(&g_Rng, 256);
        random[3] = (uint8_t)Rng_U16InRange(&g_Rng, 256);
        result = ReplayFormat_Write(writer, replay->file, &prepared, random);
    }
    Replay_Discard();
    return result;
}

void Replay_Discard(void)
{
    if (m_Replay != nullptr) {
        Chain_Remove(&m_Replay->calc);
    }
}
