#include "backend.h"

#include "case.h"
#include "compare.h"
#include "staged.h"

#include "core/archive/zip.h"
#include "core/base/log.h"
#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/ini.h"
#include "core/image/encode.h"
#include "core/io/file.h"
#include "core/io/path.h"
#include "core/vfs.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define M_ARENA_SIZE (64 * 1024 * 1024)
#define M_PROGRESS_FRAMES 600

struct DRIVER {
    const PORT *port;
    const RUN *run;
    const char *backend;
    const HARNESS_GAME *game;
    ARENA arena;
    // Holds inputs.txt of the case.
    SCRIPT script;
    // Holds what a capture played and showed, for its inputs.txt.
    SCRIPT recorded;
    CASE_MANIFEST manifest;
    // Holds the staging steps of a capture, for its manifest.
    ARRAY(STAGE_STEP) steps;
    // Holds the entry of each step, named files/NAME.
    ARRAY(ZIP_ENTRY) entries;
    // Holds the port's files, then those of the steps.
    ARRAY(STAGED_FILE) staged;
    WRITER state_out;
    STATE_WRITER *state;
    READER state_in;
    STATE_READER *expected;
    STATE_COMPARE *compare;
    // Holds the columns, then the choices, of the last row.
    uint32_t *row;
    uint32_t frame;
    uint32_t rows;
    uint32_t shots;
    STATE_DIFF diff;
    uint32_t different_choices;
    uint32_t different_shots;
};

static char *M_Join(
    ARENA *const arena, const char *const dir, const char *const name)
{
    return Path_Join(Arena_Allocator(arena), dir, name);
}

static bool M_SameText(const char *const a, const char *const b)
{
    return (a == nullptr) == (b == nullptr)
        && (a == nullptr || strcmp(a, b) == 0);
}

static const char *M_Or(const char *const text)
{
    return text != nullptr ? text : "";
}

static RESULT M_ReadScript(DRIVER *const driver, const ZIP_ENTRY *const entry)
{
    READER reader;
    Reader_OpenBuffer(&reader, entry->data, entry->size, entry->name);
    const RESULT result = Script_Read(&reader, driver->game, &driver->script);
    Reader_Close(&reader);
    return result;
}

static RESULT M_ReadFile(
    DRIVER *const driver, const char *const path, ZIP_ENTRY *const out)
{
    READER reader;
    MUST(Reader_OpenFile(&reader, path));
    const size_t size = Reader_Remaining(&reader);
    uint8_t *const data = Arena_Alloc(&driver->arena, size);
    Reader_ReadBytes(&reader, data, size);
    const RESULT result = Reader_Result(&reader);
    Reader_Close(&reader);
    *out = (ZIP_ENTRY) { .name = path, .data = data, .size = size };
    return result;
}

static RESULT M_CheckCase(const DRIVER *const driver)
{
    const CASE_MANIFEST *const manifest = &driver->manifest;
    FAIL_IF(
        strcmp(manifest->game, driver->game->id) != 0,
        "the case is of %s, not %s", manifest->game, driver->game->id);
    FAIL_IF(
        !M_SameText(manifest->game_version, driver->game->version),
        "the case is of %s %s, not %s; capture it again", manifest->game,
        M_Or(manifest->game_version), driver->game->version);
    FAIL_IF(
        manifest->columns_version != driver->game->columns_version,
        "the case has columns version %u, %s has %u; capture it again",
        manifest->columns_version, driver->game->id,
        driver->game->columns_version);
    return OK;
}

static void M_AddStaged(
    DRIVER *const driver, const STAGE_STEP *const step,
    const ZIP_ENTRY *const entry)
{
    Array_Append(
        &driver->staged,
        ((STAGED_FILE) {
            .path = step->path,
            .data = entry->data,
            .size = entry->size,
        }));
}

// Reads the entry of each step of the case.
static RESULT M_ReadSteps(DRIVER *const driver, ARCHIVE *const archive)
{
    const CASE_MANIFEST *const manifest = &driver->manifest;
    for (size_t i = 0; i < manifest->step_count; i++) {
        ZIP_ENTRY entry;
        MUST(Case_ReadEntry(archive, manifest->steps[i].source, &entry));
        M_AddStaged(driver, &manifest->steps[i], &entry);
    }
    return OK;
}

// Reads each file of a capture into the entry files/NAME, NAME being the
// file's own name, and makes a step that stages it at its path.
static RESULT M_ReadFiles(DRIVER *const driver)
{
    ARENA *const arena = &driver->arena;
    const RUN *const run = driver->run;
    for (size_t i = 0; i < run->file_count; i++) {
        const RUN_FILE *const file = &run->files[i];
        const char *const source =
            M_Join(arena, "files", Path_GetName(file->file));
        for (int32_t j = 0; j < driver->entries.count; j++) {
            FAIL_IF(
                strcmp(driver->entries.items[j].name, source) == 0,
                "two files are named %s", source);
        }
        const STAGE_STEP step = { .source = source, .path = file->path };
        MUST(Case_CheckStep(&step));
        ZIP_ENTRY entry;
        MUST(M_ReadFile(driver, file->file, &entry));
        entry.name = source;
        Array_Append(&driver->entries, entry);
        Array_Append(&driver->steps, step);
        M_AddStaged(driver, &step, &entry);
    }
    return OK;
}

static RESULT M_ReadCase(DRIVER *const driver, ARCHIVE *const archive)
{
    ARENA *const arena = &driver->arena;
    ZIP_ENTRY manifest;
    ZIP_ENTRY inputs;
    ZIP_ENTRY state;
    MUST(Case_ReadEntry(archive, "case.ini", &manifest));
    READER reader;
    Reader_OpenBuffer(&reader, manifest.data, manifest.size, "case.ini");
    RESULT result =
        Case_ReadManifest(&reader, driver->game, arena, &driver->manifest);
    Reader_Close(&reader);
    MUST(result);
    MUST(M_CheckCase(driver));
    MUST(Case_ReadEntry(archive, "inputs.txt", &inputs));
    MUST(M_ReadScript(driver, &inputs));
    MUST(M_ReadSteps(driver, archive));
    MUST(Case_ReadEntry(archive, "state.bin", &state));
    Reader_OpenBuffer(&driver->state_in, state.data, state.size, "state.bin");
    driver->expected = State_OpenReader(
        &driver->state_in, driver->game, driver->manifest.rows);
    driver->compare = Compare_OpenState(driver->expected, driver->game, arena);
    return OK;
}

static RESULT M_OpenReplay(DRIVER *const driver)
{
    READER file;
    MUST(Reader_OpenFile(&file, driver->run->case_path));
    ARCHIVE *archive;
    MUST(Archive_Open(
        &g_ZipFormat, &file, Arena_Allocator(&driver->arena), &archive));
    const RESULT result = M_ReadCase(driver, archive);
    Archive_Close(archive);
    return result;
}

static RESULT M_Open(DRIVER *const driver)
{
    const RUN *const run = driver->run;
    if (run->role == RUN_REPLAY) {
        MUST(M_OpenReplay(driver));
    } else {
        if (!run->live) {
            ZIP_ENTRY script;
            MUST(M_ReadFile(driver, run->script, &script));
            MUST(M_ReadScript(driver, &script));
        }
        MUST(M_ReadFiles(driver));
    }
    MUST(File_MakeDir(run->out_dir));
    if (run->role == RUN_REPLAY) {
        char *const shots = M_Join(&driver->arena, run->out_dir, "shots");
        MUST(File_MakeDir(shots));
    }
    Writer_OpenBuffer(&driver->state_out, &g_HeapAllocator, "state.bin");
    driver->state = State_OpenWriter(&driver->state_out, driver->game);
    return OK;
}

RESULT Driver_Open(
    const PORT *const port, const RUN *const run, const char *const backend,
    DRIVER **const out)
{
    DRIVER *const driver = Mem_Alloc(sizeof(*driver));
    *driver = (DRIVER) {
        .port = port, .run = run, .backend = backend, .game = port->game
    };
    Arena_Init(&driver->arena, "harness driver", M_ARENA_SIZE);
    Array_Init(&driver->script.inputs, &g_HeapAllocator);
    Array_Init(&driver->script.fps, &g_HeapAllocator);
    Array_Init(&driver->script.starts, &g_HeapAllocator);
    Array_Init(&driver->recorded.inputs, &g_HeapAllocator);
    Array_Init(&driver->recorded.fps, &g_HeapAllocator);
    Array_Init(&driver->recorded.starts, &g_HeapAllocator);
    Array_Init(&driver->steps, Arena_Allocator(&driver->arena));
    Array_Init(&driver->entries, Arena_Allocator(&driver->arena));
    Array_Init(&driver->staged, Arena_Allocator(&driver->arena));
    Array_AppendSlice(&driver->staged, port->files, (int32_t)port->file_count);
    driver->row = Arena_AllocArrZ(
        &driver->arena, port->game->column_count + port->game->choice_count,
        sizeof(uint32_t));
    const RESULT result = M_Open(driver);
    if (!IS_OK(result)) {
        bool differs;
        Result_Ignore(Driver_Close(driver, false, &differs));
        return result;
    }
    *out = driver;
    return OK;
}

const STAGED_FILE *Driver_Staged(
    const DRIVER *const driver, size_t *const count)
{
    *count = (size_t)driver->staged.count;
    return driver->staged.items;
}

void Driver_MountStaged(const DRIVER *const driver, VFS *const vfs)
{
    Vfs_MountSource(
        vfs, "",
        Staged_CreateSource(driver->staged.items, (size_t)driver->staged.count),
        INT32_MAX - 1);
}

RESULT Driver_MountWork(DRIVER *const driver, VFS *const vfs)
{
    char *const work = M_Join(&driver->arena, driver->run->out_dir, "work");
    MUST(File_MakeDir(work));
    return Vfs_MountDir(vfs, "", work, INT32_MAX, true);
}

const START_VALUE *Driver_Starts(
    const DRIVER *const driver, size_t *const count)
{
    if (driver->run->role == RUN_REPLAY) {
        *count = driver->manifest.start_count;
        return driver->manifest.starts;
    }
    *count = (size_t)driver->script.starts.count;
    return driver->script.starts.items;
}

uint32_t Driver_Buttons(DRIVER *const driver, const uint32_t held)
{
    const uint32_t buttons = driver->run->live
        ? held
        : Script_Buttons(&driver->script, driver->frame);
    if (driver->run->role == RUN_CAPTURE) {
        Script_RecordButtons(&driver->recorded, driver->frame, buttons);
    }
    return buttons;
}

FPS Driver_Fps(DRIVER *const driver, const FPS measured)
{
    if (driver->run->role == RUN_REPLAY) {
        return Script_Fps(&driver->script, driver->frame);
    }
    Script_RecordFps(&driver->recorded, driver->frame, measured);
    return measured;
}

void Driver_EndCalc(DRIVER *const driver)
{
    driver->port->sample(driver->port->context, driver->row);
    State_WriteRow(driver->state, driver->frame, driver->row);
    if (driver->compare != nullptr) {
        Compare_StateRow(driver->compare, driver->frame, driver->row);
    }
    driver->rows++;
}

// Returns the first screenshot frame at or after frame, or UINT32_MAX.
static uint32_t M_NextShot(const DRIVER *const driver, const uint32_t frame)
{
    const SHOTS *const shots = &driver->run->shots;
    uint32_t next = UINT32_MAX;
    if (driver->run->role != RUN_REPLAY) {
        return next;
    }
    if (shots->every != 0) {
        const uint64_t multiple =
            ((uint64_t)frame + shots->every - 1) / shots->every * shots->every;
        next = multiple < UINT32_MAX ? (uint32_t)multiple : next;
    }
    for (int32_t i = 0; i < shots->frames.count; i++) {
        const uint32_t shot = shots->frames.items[i];
        if (shot >= frame && shot < next) {
            next = shot;
        }
    }
    return next;
}

bool Driver_WantsShot(const DRIVER *const driver)
{
    return M_NextShot(driver, driver->frame) == driver->frame;
}

// Returns whether frame draws.
static bool M_WantsDraw(const DRIVER *const driver, const uint32_t frame)
{
    const RUN *const run = driver->run;
    if (run->headless) {
        return false;
    }
    const uint32_t next = M_NextShot(driver, frame);
    if (run->fast) {
        return next != UINT32_MAX && next - frame <= driver->game->draw_frames;
    }
    const bool shots = run->shots.every != 0 || run->shots.frames.count > 0;
    return run->role != RUN_REPLAY || !shots || next != UINT32_MAX;
}

bool Driver_WantsDraw(const DRIVER *const driver)
{
    return M_WantsDraw(driver, driver->frame);
}

static RESULT M_WriteShot(DRIVER *const driver, const IMAGE *const screen)
{
    FAIL_IF(
        screen->width != driver->game->width
            || screen->height != driver->game->height,
        "the screen is %dx%d, not %dx%d", screen->width, screen->height,
        driver->game->width, driver->game->height);
    WRITER writer;
    MUST(Writer_OpenFile(
        &writer,
        Compare_ShotPath(
            Arena_Allocator(&driver->arena), driver->run->out_dir, "shots",
            driver->frame)));
    Image_EncodePng(screen, &writer);
    driver->shots++;
    return Writer_Close(&writer);
}

RESULT Driver_EndFrame(DRIVER *const driver, const IMAGE *const screen)
{
    if (screen != nullptr) {
        MUST(M_WriteShot(driver, screen), "frame %" PRIu32, driver->frame);
    }
    driver->frame++;
    const PORT *const port = driver->port;
    if (driver->run->fast && port->request_full_repaint != nullptr
        && M_WantsDraw(driver, driver->frame)
        && !M_WantsDraw(driver, driver->frame - 1)) {
        port->request_full_repaint(port->context);
    }
    if (driver->frame % M_PROGRESS_FRAMES == 0) {
        printf("frame %" PRIu32 "\n", driver->frame);
        fflush(stdout);
    }
    return OK;
}

bool Driver_Done(const DRIVER *const driver)
{
    if (driver->run->role == RUN_REPLAY) {
        return driver->frame >= driver->manifest.frames;
    }
    if (driver->run->live) {
        return false;
    }
    if (driver->script.frames != 0) {
        return driver->frame >= driver->script.frames;
    }
    const SCRIPT *const script = &driver->script;
    const uint32_t last_input = script->inputs.count == 0
        ? 0
        : script->inputs.items[script->inputs.count - 1].frame;
    return driver->frame > last_input
        && driver->port->idle(driver->port->context);
}

static RESULT M_WriteFile(
    DRIVER *const driver, const char *const name,
    void (*write)(DRIVER *driver, WRITER *out))
{
    char *const path = M_Join(&driver->arena, driver->run->out_dir, name);
    WRITER writer;
    MUST(Writer_OpenFile(&writer, path));
    write(driver, &writer);
    return Writer_Close(&writer);
}

// Returns the bytes that write writes, from the driver's arena.
static ZIP_ENTRY M_Entry(
    DRIVER *const driver, const char *const name,
    void (*write)(DRIVER *driver, WRITER *out))
{
    WRITER writer;
    Writer_OpenBuffer(&writer, Arena_Allocator(&driver->arena), name);
    write(driver, &writer);
    size_t size;
    uint8_t *const data = Writer_TakeBuffer(&writer, &size);
    Result_Ignore(Writer_Close(&writer));
    return (ZIP_ENTRY) { .name = name, .data = data, .size = size };
}

static void M_WriteManifest(DRIVER *const driver, WRITER *const out)
{
    const RUN *const run = driver->run;
    CASE_MANIFEST manifest = {
        .format = CASE_FORMAT,
        .game = driver->game->id,
        .game_version = driver->game->version,
        .backend = driver->backend,
        .columns_version = driver->game->columns_version,
        .frames = driver->frame,
        .rows = driver->rows,
        .choices = driver->row + driver->game->column_count,
        .note = run->note,
        .steps = driver->steps.items,
        .step_count = (size_t)driver->steps.count,
        .starts = driver->script.starts.items,
        .start_count = (size_t)driver->script.starts.count,
    };
    Case_WriteManifest(out, driver->game, &manifest);
}

static void M_WriteInputs(DRIVER *const driver, WRITER *const out)
{
    Script_Write(out, driver->game, &driver->recorded);
}

static void M_FormatValue(
    char *const out, const size_t size, const COLUMN_KIND kind,
    const uint32_t value)
{
    switch (kind) {
    case COLUMN_I32:
        snprintf(out, size, "%" PRId32, (int32_t)value);
        return;
    case COLUMN_F32: {
        float number;
        memcpy(&number, &value, sizeof(number));
        snprintf(out, size, "%.9g (%08" PRIx32 ")", (double)number, value);
        return;
    }
    case COLUMN_HASH:
        snprintf(out, size, "%08" PRIx32, value);
        return;
    case COLUMN_U32:
        break;
    }
    snprintf(out, size, "%" PRIu32, value);
}

static void M_SetU64(
    INI *const ini, const char *const section, const char *const key,
    const uint64_t value)
{
    char text[32];
    snprintf(text, sizeof(text), "%" PRIu64, value);
    Ini_Set(ini, section, key, text);
}

static void M_SetValue(
    INI *const ini, const char *const section, const char *const key,
    const COLUMN_KIND kind, const uint32_t expected, const uint32_t actual)
{
    char a[40];
    char b[40];
    char text[96];
    M_FormatValue(a, sizeof(a), kind, expected);
    M_FormatValue(b, sizeof(b), kind, actual);
    snprintf(text, sizeof(text), "expected %s, actual %s", a, b);
    Ini_Set(ini, section, key, text);
}

// Adds the first row that differs to ini: its frame, each column but the
// HASH ones that differs, the running hash's low bits when they differ,
// and from the next check each HASH column that differs, or the side that
// lacks the row.
static void M_SetDifference(INI *const ini, const DRIVER *const driver)
{
    const HARNESS_GAME *const game = driver->game;
    const STATE_DIFF *const diff = &driver->diff;
    const uint32_t *const expected = diff->expected.values;
    const uint32_t *const actual = diff->actual.values;
    M_SetU64(ini, "difference", "row", diff->first_row);
    if (expected == nullptr || actual == nullptr) {
        M_SetU64(
            ini, "difference", "frame",
            (expected != nullptr ? expected : actual)[0]);
        Ini_Set(
            ini, "difference", "missing",
            expected == nullptr ? "expected" : "actual");
        return;
    }
    if (expected[0] != actual[0]) {
        M_SetValue(
            ini, "difference", "frame", COLUMN_U32, expected[0], actual[0]);
    } else {
        M_SetU64(ini, "difference", "frame", expected[0]);
    }
    for (size_t i = 0; i < game->column_count; i++) {
        if (game->columns[i].kind != COLUMN_HASH
            && expected[i + 1] != actual[i + 1]) {
            M_SetValue(
                ini, "difference", game->columns[i].name, game->columns[i].kind,
                expected[i + 1], actual[i + 1]);
        }
    }
    if (diff->expected.hash != diff->actual.hash) {
        M_SetValue(
            ini, "difference", "running_hash", COLUMN_U32, diff->expected.hash,
            diff->actual.hash);
    }
    if (diff->check_expected == nullptr) {
        return;
    }
    M_SetU64(ini, "check", "row", diff->check_row);
    for (size_t i = 0; i < game->column_count; i++) {
        const uint32_t a = diff->check_expected[i + 1];
        const uint32_t b = diff->check_actual[i + 1];
        if (game->columns[i].kind == COLUMN_HASH && a != b) {
            M_SetValue(ini, "check", game->columns[i].name, COLUMN_HASH, a, b);
        }
    }
}

// Counts the choices of the run's last row that differ from the case's.
static uint32_t M_CountChoices(const DRIVER *const driver)
{
    const HARNESS_GAME *const game = driver->game;
    const uint32_t *const actual = driver->row + game->column_count;
    uint32_t count = 0;
    for (size_t i = 0; i < game->choice_count; i++) {
        count += driver->manifest.choices[i] != actual[i];
    }
    return count;
}

// Adds each choice of the run that differs from the case's to ini.
static void M_SetChoices(INI *const ini, const DRIVER *const driver)
{
    const HARNESS_GAME *const game = driver->game;
    const uint32_t *const actual = driver->row + game->column_count;
    for (size_t i = 0; i < game->choice_count; i++) {
        const uint32_t expected = driver->manifest.choices[i];
        if (expected != actual[i]) {
            M_SetValue(
                ini, "choices", game->choices[i].name, game->choices[i].kind,
                expected, actual[i]);
        }
    }
}

static void M_WriteReport(DRIVER *const driver, WRITER *const out)
{
    INI *ini;
    if (!Result_Log(
            Ini_Parse("", 0, &g_HeapAllocator, &ini), LOG_LEVEL_ERROR)) {
        return;
    }
    const STATE_DIFF *const diff = &driver->diff;
    M_SetU64(ini, "report", "frames", driver->frame);
    M_SetU64(ini, "report", "rows", diff->rows);
    M_SetU64(ini, "report", "shots", driver->shots);
    if (driver->run->against_dir != nullptr) {
        M_SetU64(ini, "report", "different_shots", driver->different_shots);
    }
    M_SetChoices(ini, driver);
    M_SetU64(ini, "report", "different_choices", driver->different_choices);
    if (diff->differs) {
        M_SetDifference(ini, driver);
    }
    Ini_Write(ini, out);
    Ini_Free(ini);
}

static RESULT M_WriteCase(DRIVER *const driver, const ZIP_ENTRY *const state)
{
    ARRAY(ZIP_ENTRY) entries;
    Array_Init(&entries, Arena_Allocator(&driver->arena));
    Array_Append(&entries, M_Entry(driver, "case.ini", M_WriteManifest));
    Array_Append(&entries, M_Entry(driver, "inputs.txt", M_WriteInputs));
    Array_Append(&entries, *state);
    Array_AppendSlice(&entries, driver->entries.items, driver->entries.count);
    WRITER writer;
    MUST(Writer_OpenFile(&writer, driver->run->case_path));
    const RESULT result =
        Zip_Write(&writer, entries.items, (size_t)entries.count);
    const RESULT closed = Writer_Close(&writer);
    MUST(result);
    return closed;
}

static RESULT M_Finish(DRIVER *const driver, const ZIP_ENTRY *const state)
{
    const RUN *const run = driver->run;
    if (run->role == RUN_CAPTURE) {
        return M_WriteCase(driver, state);
    }
    char *const path = M_Join(&driver->arena, run->out_dir, "state.bin");
    WRITER writer;
    MUST(Writer_OpenFile(&writer, path));
    Writer_WriteBytes(&writer, state->data, state->size);
    MUST(Writer_Close(&writer));
    if (run->against_dir != nullptr) {
        MUST(Compare_Shots(
            run->out_dir, run->against_dir, run->out_dir,
            &driver->different_shots));
    }
    return M_WriteFile(driver, "report.ini", M_WriteReport);
}

RESULT Driver_Close(
    DRIVER *const driver, const bool completed, bool *const differs)
{
    RESULT result = OK;
    if (driver->compare != nullptr) {
        Compare_CloseState(driver->compare, &driver->diff);
        driver->different_choices = M_CountChoices(driver);
        result = State_CloseReader(driver->expected);
        Reader_Close(&driver->state_in);
    }
    if (driver->state != nullptr) {
        const RESULT closed = State_CloseWriter(driver->state);
        size_t size;
        uint8_t *const bytes = Writer_TakeBuffer(&driver->state_out, &size);
        const ZIP_ENTRY state = { "state.bin", bytes, size };
        if (IS_OK(result)) {
            result = closed;
        } else {
            Result_Ignore(closed);
        }
        if (IS_OK(result) && completed) {
            result = M_Finish(driver, &state);
        }
        Allocator_Free(&g_HeapAllocator, bytes, size);
        Result_Ignore(Writer_Close(&driver->state_out));
    }
    *differs = driver->diff.differs || driver->different_shots != 0
        || driver->different_choices != 0;
    Script_Free(&driver->script);
    Script_Free(&driver->recorded);
    Arena_Free(&driver->arena);
    Mem_Free(driver);
    return result;
}
