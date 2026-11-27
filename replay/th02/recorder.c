#include "recorder.h"

#include "harness.h"
#include "start.h"

#include "th02/boss.h"
#include "th02/boss/fight.h"
#include "th02/bullets.h"
#include "th02/clock.h"
#include "th02/hooks.h"
#include "th02/input.h"
#include "th02/main.h"
#include "th02/player.h"
#include "th02/score.h"
#include "th02/settings.h"
#include "th02/stage.h"

#include "core/base/memory.h"
#include "core/config.h"
#include "core/io/file.h"
#include "core/io/reader.h"
#include "core/vfs.h"
#include "core/vfs/normalize.h"
#include "shell/game/builtin.h"
#include "shell/game/files.h"
#include "shell/game/fonts.h"

#include <SDL3/SDL_stdinc.h>

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

// Records a th02 run as th02record, from boot, and saves it as a case
// script. The start values are those of a case script's start lines, and
// the run presses ok at frame 700 and shot at frames 780 and 1000, as the
// case scripts do, to get from the title through shot selection into
// gameplay before the bot plays.

#define M_BOT_FRAME 1002
#define M_USER_FILES 8

// Holds the files that the game writes, such as HUUHI.DAT, in memory, so
// that each run starts without them, as a capture's empty work directory
// does.
typedef struct {
    VFS_SOURCE base;
    struct {
        char *path;
        void *data;
        size_t size;
    } files[M_USER_FILES];
    size_t count;
} M_USER_SOURCE;

static START_VALUE m_Starts[TH02_START_COUNT];
static size_t m_StartCount;
static bool m_Initialized;
static bool m_StartApplied;
static bool m_InGameplay;
static int32_t m_Frame;
static int32_t m_Lives;
static int32_t m_Setbacks;
static uint8_t m_Stage;
static int32_t m_BossPhases;
static int32_t m_Damage;

static void M_OnStart(void *const arg, TH02_SESSION *const session)
{
    if (!m_StartApplied) {
        m_StartApplied = true;
        Th02Start_Apply(m_Starts, m_StartCount, session);
    }
}

// Counts the boss's damage resets, one as the boss starts and one for each
// phase that it clears.
static void M_OnBoss(
    void *const arg, int16_t *const damage, const int16_t threshold)
{
    m_BossPhases++;
    m_Damage = 0;
    Th02Start_ApplyBoss(m_Starts, m_StartCount, damage, threshold);
}

static size_t M_FindUserFile(
    const VFS_SOURCE *const base, const char *const path)
{
    const M_USER_SOURCE *const source = (const M_USER_SOURCE *)base;
    for (size_t i = 0; i < source->count; i++) {
        if (SDL_strcasecmp(source->files[i].path, path) == 0) {
            return i;
        }
    }
    return SIZE_MAX;
}

static bool M_UserContains(
    const VFS_SOURCE *const source, const char *const path)
{
    return M_FindUserFile(source, path) != SIZE_MAX;
}

static RESULT M_UserOpen(
    VFS_SOURCE *const base, const char *const path, const VFS_MODE mode,
    READER *const out)
{
    const M_USER_SOURCE *const source = (const M_USER_SOURCE *)base;
    const size_t i = M_FindUserFile(base, path);
    FAIL_IF(i == SIZE_MAX, "%s: not written", path);
    Reader_OpenBuffer(out, source->files[i].data, source->files[i].size, path);
    return OK;
}

static void M_UserList(
    const VFS_SOURCE *const base, const char *const dir,
    void (*const visit)(void *user, const char *name), void *const user)
{
    const M_USER_SOURCE *const source = (const M_USER_SOURCE *)base;
    for (size_t i = 0; i < source->count; i++) {
        const char *const name =
            VfsNormalize_NameBelow(source->files[i].path, dir);
        if (name != nullptr) {
            visit(user, name);
        }
    }
}

static RESULT M_UserWrite(
    VFS_SOURCE *const base, const char *const path, const void *const data,
    const size_t size)
{
    M_USER_SOURCE *const source = (M_USER_SOURCE *)base;
    size_t i = M_FindUserFile(base, path);
    if (i == SIZE_MAX) {
        FAIL_IF(source->count == M_USER_FILES, "%s: too many files", path);
        i = source->count++;
        source->files[i].path = Mem_Dup(path, strlen(path) + 1);
    } else {
        Mem_Free(source->files[i].data);
    }
    source->files[i].data = Mem_Dup(data, size);
    source->files[i].size = size;
    return OK;
}

static RESULT M_UserRemove(VFS_SOURCE *const base, const char *const path)
{
    M_USER_SOURCE *const source = (M_USER_SOURCE *)base;
    const size_t i = M_FindUserFile(base, path);
    if (i != SIZE_MAX) {
        Mem_Free(source->files[i].path);
        Mem_Free(source->files[i].data);
        source->files[i] = source->files[--source->count];
    }
    return OK;
}

static void M_UserClose(VFS_SOURCE *const base)
{
    M_USER_SOURCE *const source = (M_USER_SOURCE *)base;
    for (size_t i = 0; i < source->count; i++) {
        Mem_Free(source->files[i].path);
        Mem_Free(source->files[i].data);
    }
    Mem_Free(source);
}

static const VFS_SOURCE_FUNCS m_UserFuncs = {
    .contains = M_UserContains,
    .open = M_UserOpen,
    .list = M_UserList,
    .write = M_UserWrite,
    .close = M_UserClose,
    .remove = M_UserRemove,
};

static void M_MountUserFiles(void)
{
    M_USER_SOURCE *const source = Mem_Alloc(sizeof(*source));
    *source = (M_USER_SOURCE) {
        .base = { .funcs = &m_UserFuncs, .name = "memory" },
    };
    Vfs_MountSource(Shell_GetUserFiles(), "", &source->base, 0);
}

static RESULT M_SetStart(const char *const name, const char *const value)
{
    const int32_t field = Game_FindStart(&g_Th02Harness, name);
    FAIL_IF(field < 0, "there is no start value %s", name);
    START_VALUE start = { .field = (uint32_t)field };
    FAIL_IF(
        !Game_ParseValue(g_Th02Harness.starts[field].kind, value, &start.value),
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

// Sets up the game as the native backend does for a case without the
// bomb profile.
static RESULT M_Start(void)
{
    MUST(ShellBuiltin_Mount(Shell_GetFiles()));
    MUST(ShellFonts_Reset(false));
    M_MountUserFiles();
    Config_SetDefaults(&g_Th02SettingsSchema, &g_Th02Settings);
    g_Th02Settings.fix_bomb_transition_overread = true;
    m_StartApplied = false;
    m_InGameplay = false;
    m_Frame = 0;
    m_Setbacks = 0;
    m_BossPhases = 0;
    m_Damage = 0;
    g_Th02Hooks = (TH02_HOOKS) { .on_start = M_OnStart, .on_boss = M_OnBoss };
    m_Initialized = true;
    return g_GameDesc.init();
}

static bool M_ScriptInput(const int32_t frame, void *const input)
{
    if (frame >= M_BOT_FRAME && m_InGameplay) {
        return false;
    }
    *(TH02_INPUT *)input = (TH02_INPUT) {
        .ok = frame == 700 || frame == 701,
        .shot = frame == 780 || frame == 781 || frame >= 1000,
    };
    return true;
}

static bool M_IsPlaying(const TH02_SCENE scene)
{
    return scene == TH02_SCENE_GAMEPLAY || scene == TH02_SCENE_BONUS
        || scene == TH02_SCENE_STAGE_LEAVE;
}

// Gives the game the clock samples that the native backend's frame clock
// gives it, so that a capture of the saved script plays the same run.
// Counts each life lost in play as a setback and keeps the boss's damage.
static bool M_Tick(const void *const input)
{
    TH02_INPUT raw = *(const TH02_INPUT *)input;
    const uint64_t frame = (uint64_t)ceil(m_Frame * 1000.0 / 60.0) * 1000;
    const TH02_CLOCK_SAMPLE clocks[] = {
        { .key = TH02_CLOCK_FRAME, .microseconds = frame },
        { .key = TH02_CLOCK_ORIGIN, .microseconds = 0 },
        { .key = TH02_CLOCK_SCANOUT_KEY, .microseconds = frame },
    };
    raw.clocks = clocks;
    raw.clock_count = sizeof(clocks) / sizeof(clocks[0]);
    m_Frame++;
    const bool running = g_GameDesc.tick(&raw);
    TH02_VIEW view;
    Th02_View(&view);
    if (!m_InGameplay && view.scene == TH02_SCENE_GAMEPLAY) {
        m_InGameplay = true;
        m_Stage = view.stage;
        m_Lives = g_Score->lives;
    }
    if (m_InGameplay && M_IsPlaying(view.scene)) {
        if (view.stage != m_Stage) {
            m_Stage = view.stage;
            m_BossPhases = 0;
            m_Damage = 0;
        }
        if (g_Stage->world.progression == BOSS_RUNNING) {
            m_Damage = BossFight_State()->damage;
        }
        m_Setbacks += g_Score->lives < m_Lives ? m_Lives - g_Score->lives : 0;
        m_Lives = g_Score->lives;
    }
    return running;
}

static int32_t M_Setbacks(void)
{
    return m_Setbacks;
}

// Gives the squared distance in pixels from the player's hitbox to the
// nearest active bullet, capped at 64 pixels, so that among runs that got
// as far a run that ends clear of bullets ranks higher.
static int64_t M_BulletMargin(void)
{
    const int32_t px = g_Player->current.x + 16;
    const int32_t py = g_Player->current.y + 24;
    int64_t nearest = 64 * 64;
    for (int i = 0; i < BULLET_COUNT; i++) {
        const BULLET *const bullet = &g_Bullets->slots[i];
        if (bullet->state != ENTITY_ACTIVE) {
            continue;
        }
        for (int page = 0; page < 2; page++) {
            const int32_t dx = bullet->position[page].x + 4 - px;
            const int32_t dy = bullet->position[page].y + 4 - py;
            const int64_t distance = (int64_t)dx * dx + (int64_t)dy * dy;
            if (distance < nearest) {
                nearest = distance;
            }
        }
    }
    return nearest;
}

// Gives the stage, the boss's damage resets so far in it and the damage
// dealt since the last one, which stays after the boss's end, then the
// margin to the nearest bullet. The game's score stays out, so that the
// run's length, which breaks ties, leads through the stage and past the
// boss.
static RECORDER_SCORE M_Score(void)
{
    RECORDER_SCORE score = { .count = 4 };
    if (!m_InGameplay) {
        return score;
    }
    score.values[0] = m_Stage;
    score.values[1] = m_BossPhases;
    score.values[2] = m_Damage;
    score.values[3] = M_BulletMargin();
    return score;
}

// Gives no idle frames while a boss fights: standing still for 90 frames
// under its patterns would kill every candidate.
static int32_t M_IdleFrames(void)
{
    return m_InGameplay && g_Stage->world.progression == BOSS_RUNNING ? 0 : 90;
}

// Ends the run when play ends, as at a game over or the ending.
static bool M_HasEnded(void)
{
    TH02_VIEW view;
    Th02_View(&view);
    return m_InGameplay && !M_IsPlaying(view.scene);
}

static uint32_t M_Buttons(const uint8_t *const input)
{
    uint32_t buttons = 0;
    for (size_t i = 0; i < g_Th02Harness.binding_count; i++) {
        const BINDING *const binding = &g_Th02Harness.bindings[i];
        for (size_t j = 0; j < g_Th02InputSchema.count; j++) {
            const INPUT_ACTION *const action = &g_Th02InputSchema.actions[j];
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
    for (size_t i = 0; i < g_Th02Harness.binding_count; i++) {
        const BINDING *const binding = &g_Th02Harness.bindings[i];
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
        const COLUMN *const field = &g_Th02Harness.starts[m_Starts[i].field];
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
        const uint32_t buttons = M_Buttons(
            (const uint8_t *)inputs + (size_t)i * g_Th02InputSchema.size);
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
        .program = "th02record",
        .input_schema = &g_Th02InputSchema,
        .bot_ini = "replay/th02/bot.ini",
        .max_frames = 400000,
        .set_start = M_SetStart,
        .start = M_Start,
        .script_input = M_ScriptInput,
        .tick = M_Tick,
        .setbacks = M_Setbacks,
        .score = M_Score,
        .idle_frames = M_IdleFrames,
        .has_ended = M_HasEnded,
        .save = M_Save,
        .stop = M_Stop,
    };
    return Recorder_Main(&game, argc, argv);
}
