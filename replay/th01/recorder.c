#include "recorder.h"

#include "harness.h"
#include "start.h"

#include "th01/boss.h"
#include "th01/hooks.h"
#include "th01/input.h"
#include "th01/main.h"
#include "th01/missiles.h"
#include "th01/pellets.h"
#include "th01/player.h"
#include "th01/session.h"
#include "th01/settings.h"
#include "th01/shootout_lasers.h"
#include "th01/stage.h"
#include "th01/stage_objects.h"

#include "core/base/utils.h"
#include "core/config.h"
#include "core/io/file.h"
#include "shell/game/builtin.h"
#include "shell/game/files.h"
#include "shell/game/fonts.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

// Records a th01 run as th01record, from boot, and saves it as a case
// script. The start values are those of a case script's start lines, and
// the run presses shot at frames 760 and 1000, as the case scripts do, to
// get from the title into gameplay before the bot plays.

#define M_MAX_THREATS 64
#define M_PLAYER_ROW (PLAYER_TOP + 16)
#define M_PLAY_WIDTH 640
#define M_BOT_FRAME 1002

static START_VALUE m_Starts[TH01_START_COUNT];
static size_t m_StartCount;
static bool m_Mounted;
static bool m_Initialized;
static bool m_StartApplied;
static bool m_InGameplay;
static int16_t m_StartStage;

static void M_OnStart(void *const arg)
{
    if (!m_StartApplied) {
        m_StartApplied = true;
        Th01Start_Apply(m_Starts, m_StartCount);
    }
}

static void M_OnBoss(void *const arg, int16_t *const hp)
{
    Th01Start_ApplyBoss(m_Starts, m_StartCount, hp);
}

static RESULT M_SetStart(const char *const name, const char *const value)
{
    const int32_t field = Game_FindStart(&g_Th01Harness, name);
    FAIL_IF(field < 0, "there is no start value %s", name);
    START_VALUE start = { .field = (uint32_t)field };
    FAIL_IF(
        !Game_ParseValue(g_Th01Harness.starts[field].kind, value, &start.value),
        "bad start value %s", value);
    for (size_t i = 0; i < m_StartCount; i++) {
        if (m_Starts[i].field == start.field) {
            m_Starts[i] = start;
            return OK;
        }
    }
    m_Starts[m_StartCount++] = start;
    return OK;
}

// Sets up the game as the native backend does, without the debouncing fix
// that the original lacks.
static RESULT M_Start(void)
{
    if (!m_Mounted) {
        MUST(ShellBuiltin_Mount(Shell_GetFiles()));
        MUST(ShellFonts_Reset(false));
        m_Mounted = true;
    }
    Config_SetDefaults(&g_SettingsSchema, &g_Settings);
    g_Settings.fix_debouncing = false;
    m_StartApplied = false;
    m_InGameplay = false;
    Th01Hooks_Set((TH01_HOOKS) { .on_start = M_OnStart, .on_boss = M_OnBoss });
    m_Initialized = true;
    return g_GameDesc.init();
}

static bool M_ScriptInput(const int32_t frame, void *const input)
{
    if (frame >= M_BOT_FRAME && m_InGameplay) {
        return false;
    }
    *(TH01_INPUT *)input = (TH01_INPUT) {
        .shot = frame == 760 || frame == 1000 || frame == 1001,
    };
    return true;
}

static bool M_Tick(const void *const input)
{
    const bool running = g_GameDesc.tick(input);
    if (!m_InGameplay && g_Th01Scene == TH01_SCENE_GAMEPLAY) {
        m_InGameplay = true;
        m_StartStage = g_Session.stage_id;
    }
    return running;
}

// Inserts offset into offsets, which holds *count offsets nearest first,
// keeping at most max.
static void M_Insert(
    VEC2 *const offsets, float *const distances, int32_t *const count,
    const int32_t max, const VEC2 offset)
{
    const float distance = offset.x * offset.x + offset.y * offset.y;
    int32_t i = *count;
    if (i < max) {
        (*count)++;
    } else if (distance >= distances[max - 1]) {
        return;
    } else {
        i = max - 1;
    }
    for (; i > 0 && distances[i - 1] > distance; i--) {
        offsets[i] = offsets[i - 1];
        distances[i] = distances[i - 1];
    }
    offsets[i] = offset;
    distances[i] = distance;
}

// Returns the offset from the player's center to the pixel at x, y. A
// threat on the player's row moves half a pixel up, since the bot takes an
// offset with a y of 0 for an edge.
static VEC2 M_Offset(const int32_t x, const int32_t y)
{
    const float dy = (float)(y - M_PLAYER_ROW);
    return (VEC2) {
        (float)(x - (g_Player.left + 16)),
        dy == 0.0f ? -0.5f : dy,
    };
}

// Returns the point of a laser nearest the player's row: where it crosses
// the row, or its head when it ends above the row.
static VEC2 M_LaserOffset(const SHOOTOUT_LASER *const laser)
{
    const int64_t head_x =
        laser->start_left + (int64_t)laser->step_x * laser->length;
    const int64_t head_y =
        laser->start_y + (int64_t)laser->step_y * laser->length;
    const int64_t row = (int64_t)M_PLAYER_ROW * 256;
    if (head_y < row || laser->step_y <= 0) {
        return M_Offset((int32_t)(head_x / 256), (int32_t)(head_y / 256));
    }
    const int64_t x = laser->start_left
        + (row - laser->start_y) * laser->step_x / laser->step_y;
    return M_Offset((int32_t)(x / 256), M_PLAYER_ROW);
}

static int32_t M_ListThreats(VEC2 *const offsets, const int32_t max)
{
    float distances[M_MAX_THREATS];
    const int32_t limit = MIN(max, M_MAX_THREATS);
    int32_t count = 0;
    for (int32_t i = 0; i < PELLETS_COUNT; i++) {
        const PELLET *const pellet = &g_Pellets.slots[i];
        if (pellet->moving) {
            M_Insert(
                offsets, distances, &count, limit,
                M_Offset(pellet->left + 4, pellet->top + 4));
        }
    }
    for (int32_t i = 0; i < MISSILES_COUNT; i++) {
        const MISSILE *const missile = &g_Missiles.slots[i];
        if (missile->flag != 0) {
            M_Insert(
                offsets, distances, &count, limit,
                M_Offset(
                    Math16_ToPixel(missile->left) + 8,
                    Math16_ToPixel(missile->top) + 8));
        }
    }
    for (int32_t i = 0; i < SHOOTOUT_LASERS_COUNT; i++) {
        const SHOOTOUT_LASER *const laser = &g_ShootoutLasers.slots[i];
        if (laser->alive && laser->damaging) {
            M_Insert(offsets, distances, &count, limit, M_LaserOffset(laser));
        }
    }
    const float center = (float)(g_Player.left + 16);
    M_Insert(offsets, distances, &count, limit, (VEC2) { -center, 0.0f });
    M_Insert(
        offsets, distances, &count, limit,
        (VEC2) { (float)M_PLAY_WIDTH - center, 0.0f });
    return count;
}

// Gives the boss's phase while a boss runs, and the cards removed
// otherwise.
static RECORDER_STATE M_State(void)
{
    RECORDER_STATE state = { .lives = g_Session.lives };
    if (!m_InGameplay) {
        return state;
    }
    if (g_Stage.boss != BOSS_NONE && g_Boss != nullptr) {
        state.progress = g_Boss->phase;
        return state;
    }
    for (int32_t i = 0; i < g_StageObjects.card_count; i++) {
        state.progress += g_StageObjects.cards[i].flag == STAGE_CARD_REMOVED;
    }
    return state;
}

// Ends the run when gameplay ends, as at a game over, or moves on to
// another stage.
static bool M_HasEnded(void)
{
    return m_InGameplay
        && (g_Th01Scene != TH01_SCENE_GAMEPLAY
            || g_Session.stage_id != m_StartStage);
}

static uint32_t M_Buttons(const uint8_t *const input)
{
    uint32_t buttons = 0;
    for (size_t i = 0; i < g_Th01Harness.binding_count; i++) {
        const BINDING *const binding = &g_Th01Harness.bindings[i];
        for (size_t j = 0; j < g_InputSchema.count; j++) {
            const INPUT_ACTION *const action = &g_InputSchema.actions[j];
            if (strcmp(action->name, binding->name) == 0
                && *(const bool *)(input + action->offset)) {
                buttons |= binding->mask;
            }
        }
    }
    return buttons;
}

static void M_WriteButtons(FILE *const file, const uint32_t buttons)
{
    if (buttons == 0) {
        fputs("none", file);
        return;
    }
    bool first = true;
    for (size_t i = 0; i < g_Th01Harness.binding_count; i++) {
        const BINDING *const binding = &g_Th01Harness.bindings[i];
        if ((buttons & binding->mask) != 0) {
            fprintf(file, "%s%s", first ? "" : "+", binding->name);
            first = false;
        }
    }
}

// Writes the run as a case script: its frames, its start lines and a line
// at each frame whose held buttons differ from the frame before.
static RESULT M_Save(
    const char *const path, const void *const inputs, const int32_t frames)
{
    FILE *file;
    MUST(File_CreateText(path, &file));
    fprintf(file, "frames %" PRId32 "\n", frames);
    for (size_t i = 0; i < m_StartCount; i++) {
        const COLUMN *const field = &g_Th01Harness.starts[m_Starts[i].field];
        if (field->kind == COLUMN_I32) {
            fprintf(
                file, "start %s %" PRId32 "\n", field->name,
                (int32_t)m_Starts[i].value);
        } else {
            fprintf(
                file, "start %s %" PRIu32 "\n", field->name, m_Starts[i].value);
        }
    }
    uint32_t held = 0;
    for (int32_t i = 0; i < frames; i++) {
        const uint32_t buttons =
            M_Buttons((const uint8_t *)inputs + (size_t)i * g_InputSchema.size);
        if (buttons != held) {
            fprintf(file, "input %" PRId32 " ", i);
            M_WriteButtons(file, buttons);
            fputc('\n', file);
            held = buttons;
        }
    }
    const bool failed = ferror(file) != 0;
    FAIL_IF(fclose(file) != 0 || failed, "cannot write %s", path);
    return OK;
}

static void M_Stop(void)
{
    if (m_Initialized) {
        g_GameDesc.shutdown();
        m_Initialized = false;
    }
}

int main(int argc, char **argv)
{
    const RECORDER_GAME game = {
        .program = "th01record",
        .input_schema = &g_InputSchema,
        .bot_ini = "replay/th01/bot.ini",
        .set_start = M_SetStart,
        .start = M_Start,
        .script_input = M_ScriptInput,
        .tick = M_Tick,
        .list_threats = M_ListThreats,
        .state = M_State,
        .has_ended = M_HasEnded,
        .save = M_Save,
        .stop = M_Stop,
    };
    return Recorder_Main(&game, argc, argv);
}
