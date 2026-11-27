#include "replay/harness/backend.h"
#include "tests/harness_game.h"
#include "tests/test.h"
#include "tests/test_dir.h"

#include "core/archive/zip.h"
#include "core/base/string.h"
#include "core/base/utils.h"
#include "core/io/file.h"
#include "core/vfs.h"

#define M_ARENA_SIZE (1024 * 1024)

typedef struct {
    uint32_t frame;
    uint32_t score;
    uint32_t bias;
    // Gives the first byte of every screenshot.
    uint8_t shade;
    // Holds the frames that start with a full repaint, one bit each.
    uint32_t repaints;
} M_GAME;

static void M_Sample(void *const context, uint32_t *const row)
{
    const M_GAME *const game = context;
    row[0] = game->score;
    row[1] = (uint32_t)-1;
    row[2] = game->frame;
    row[3] = game->bias;
    row[4] = 2;
}

static bool M_Idle(void *const context)
{
    const M_GAME *const game = context;
    return game->frame >= 8;
}

static void M_Seed(void *const context)
{
}

// Marks the next frame, since the driver asks at the end of the frame before
// the repainted one.
static void M_RequestFullRepaint(void *const context)
{
    M_GAME *const game = context;
    game->repaints |= 1u << (game->frame + 1);
}

// Runs the fake game under driver, one calc per frame, and returns how
// the run ended. A person holds shoot from frame 2 to 4 and quits after
// frame 9.
static bool M_Run(const PORT *const port, const RUN *const run)
{
    DRIVER *driver;
    CHECK(Result_Ignore(Driver_Open(port, run, "native", &driver)));
    M_GAME *const game = port->context;
    *game = (M_GAME) { .bias = game->bias, .shade = game->shade };
    IMAGE screen = Image_Create(4, 2, &g_HeapAllocator);
    screen.pixels[0] = game->shade;
    while (!Driver_Done(driver) && game->frame < 10) {
        const uint32_t held = game->frame >= 2 && game->frame < 5 ? 0x1 : 0;
        game->score += Driver_Buttons(driver, held);
        Driver_EndCalc(driver);
        Driver_Fps(driver, (FPS) { game->frame >= 3, 60.0f });
        CHECK(Result_Ignore(Driver_EndFrame(
            driver, Driver_WantsShot(driver) ? &screen : nullptr)));
        game->frame++;
    }
    Image_Free(&screen);
    bool differs = true;
    CHECK(Result_Ignore(Driver_Close(driver, true, &differs)));
    return differs;
}

static char *M_ReadText(const char *const dir, const char *const name)
{
    char *const path = TestDir_Join(dir, name);
    READER reader;
    char *text = nullptr;
    if (Result_Ignore(Reader_OpenFile(&reader, path))) {
        const size_t size = Reader_Remaining(&reader);
        text = Mem_AllocZ(size + 1);
        Reader_ReadBytes(&reader, text, size);
        Reader_Close(&reader);
    }
    Mem_Free(path);
    return text;
}

// Returns the entry name of the case file path as text, or nullptr.
static char *M_ReadEntry(const char *const path, const char *const name)
{
    READER file;
    ARCHIVE *archive;
    if (!Result_Ignore(Reader_OpenFile(&file, path))
        || !Result_Ignore(
            Archive_Open(&g_ZipFormat, &file, &g_HeapAllocator, &archive))) {
        return nullptr;
    }
    char *text = nullptr;
    const ARCHIVE_ENTRY *const entry = Archive_Find(archive, name);
    uint8_t *data;
    if (entry != nullptr
        && Result_Ignore(Archive_Read(archive, entry, &data))) {
        const size_t size = ArchiveEntry_GetSize(entry);
        text = Mem_AllocZ(size + 1);
        memcpy(text, data, size);
        Allocator_Free(&g_HeapAllocator, data, size);
    }
    Archive_Close(archive);
    return text;
}

static void M_WriteText(
    const char *const dir, const char *const name, const char *const text)
{
    CHECK(Result_Ignore(File_MakeDir(dir)));
    char *const path = TestDir_Join(dir, name);
    WRITER writer;
    CHECK(Result_Ignore(Writer_OpenFile(&writer, path)));
    Writer_WriteBytes(&writer, text, strlen(text));
    CHECK(Result_Ignore(Writer_Close(&writer)));
    Mem_Free(path);
}

TEST(a_live_capture_records_the_held_buttons)
{
    char *const root = TestDir_Prepare("driver_live");
    char *const out_dir = TestDir_Join(root, "out");
    char *const case_path = TestDir_Join(root, "live.othtc");
    M_GAME game = {};
    const PORT port = {
        .game = &m_TestGame,
        .context = &game,
        .sample = M_Sample,
        .idle = M_Idle,
        .seed = M_Seed,
    };
    const RUN capture = {
        .role = RUN_CAPTURE,
        .game_dir = root,
        .case_path = case_path,
        .out_dir = out_dir,
        .live = true,
    };
    CHECK(!M_Run(&port, &capture));
    char *const inputs = M_ReadEntry(case_path, "inputs.txt");
    CHECK_EQ_STR(inputs, "input 2 shoot\ninput 5 none\nfps 3 60\n");
    Mem_Free(inputs);
    Mem_Free(case_path);
    Mem_Free(out_dir);
    Mem_Free(root);
}

TEST(a_capture_records_date_and_clock_and_replay_returns_them)
{
    char *const root = TestDir_Prepare("driver_date_clock");
    char *const capture_dir = TestDir_Join(root, "capture");
    char *const case_path = TestDir_Join(root, "test.othtc");
    char *const replay_dir = TestDir_Join(root, "replay");
    M_GAME game = {};
    const PORT port = {
        .game = &m_TestGame,
        .context = &game,
        .sample = M_Sample,
        .idle = M_Idle,
        .seed = M_Seed,
    };
    const RUN capture = {
        .role = RUN_CAPTURE,
        .game_dir = root,
        .case_path = case_path,
        .out_dir = capture_dir,
        .live = true,
    };
    DRIVER *driver;
    CHECK(Result_Ignore(Driver_Open(&port, &capture, "native", &driver)));
    while (!Driver_Done(driver) && game.frame < 10) {
        Driver_Buttons(driver, 0);
        CHECK_EQ_INT(Driver_Date(driver, 20261127), 20261127);
        if (game.frame == 1) {
            Driver_RecordClock(driver, 7, 12345);
        }
        uint64_t microseconds;
        CHECK_EQ_INT(Driver_Clock(driver, 7, &microseconds), game.frame == 1);
        Driver_EndCalc(driver);
        CHECK(Result_Ignore(Driver_EndFrame(driver, nullptr)));
        game.frame++;
    }
    bool differs = true;
    CHECK(Result_Ignore(Driver_Close(driver, true, &differs)));
    CHECK(!differs);

    char *const inputs = M_ReadEntry(case_path, "inputs.txt");
    CHECK(
        inputs != nullptr && strstr(inputs, "date 0 2026-11-27\n") != nullptr);
    CHECK(inputs != nullptr && strstr(inputs, "clock 1 7 12345\n") != nullptr);
    Mem_Free(inputs);

    const RUN replay = {
        .role = RUN_REPLAY,
        .game_dir = root,
        .case_path = case_path,
        .out_dir = replay_dir,
    };
    M_GAME replay_game = {};
    const PORT replay_port = {
        .game = &m_TestGame,
        .context = &replay_game,
        .sample = M_Sample,
        .idle = M_Idle,
        .seed = M_Seed,
    };
    CHECK(Result_Ignore(Driver_Open(&replay_port, &replay, "native", &driver)));
    while (!Driver_Done(driver) && replay_game.frame < 10) {
        Driver_Buttons(driver, 0);
        CHECK_EQ_INT(Driver_Date(driver, 0), 20261127);
        uint64_t microseconds;
        CHECK_EQ_INT(
            Driver_Clock(driver, 7, &microseconds), replay_game.frame == 1);
        if (replay_game.frame == 1) {
            CHECK_EQ_INT(microseconds, 12345);
        }
        Driver_EndCalc(driver);
        CHECK(Result_Ignore(Driver_EndFrame(driver, nullptr)));
        replay_game.frame++;
    }
    CHECK(Result_Ignore(Driver_Close(driver, true, &differs)));
    CHECK(!differs);

    Mem_Free(replay_dir);
    Mem_Free(case_path);
    Mem_Free(capture_dir);
    Mem_Free(root);
}

TEST(a_capture_replays_to_a_match)
{
    char *const root = TestDir_Prepare("driver");
    char *const script_dir = TestDir_Join(root, "script");
    char *const script = TestDir_Join(script_dir, "inputs.txt");
    char *const stage_dir = TestDir_Join(root, "stage");
    char *const slot = TestDir_Join(stage_dir, "slot.bin");
    char *const score_file = TestDir_Join(stage_dir, "score.dat");
    char *const capture_dir = TestDir_Join(root, "capture");
    char *const case_path = TestDir_Join(root, "test.othtc");
    char *const replay_dir = TestDir_Join(root, "replay");
    M_WriteText(script_dir, "inputs.txt", "input 2 shoot\ninput 5 none\n");
    M_WriteText(stage_dir, "score.dat", "score");
    M_WriteText(stage_dir, "slot.bin", "staged");
    M_GAME game = {};
    const PORT port = {
        .game = &m_TestGame,
        .context = &game,
        .sample = M_Sample,
        .idle = M_Idle,
        .seed = M_Seed,
    };
    RUN capture = {
        .role = RUN_CAPTURE,
        .game_dir = root,
        .script = script,
        .case_path = case_path,
        .out_dir = capture_dir,
        .files = { { slot, "save/slot.bin" }, { score_file, "score.dat" } },
        .file_count = 2,
        .note = "test",
    };
    CHECK(!M_Run(&port, &capture));
    char *const inputs = M_ReadEntry(case_path, "inputs.txt");
    CHECK_EQ_STR(inputs, "input 2 shoot\ninput 5 none\nfps 3 60\n");
    char *const manifest = M_ReadEntry(case_path, "case.ini");
    CHECK(manifest != nullptr && strstr(manifest, "frames = 8") != nullptr);
    CHECK(
        manifest != nullptr
        && strstr(manifest, "1 = stage files/slot.bin -> save/slot.bin")
            != nullptr);
    char *const file = M_ReadEntry(case_path, "files/slot.bin");
    CHECK_EQ_STR(file, "staged");
    char *const score = M_ReadEntry(case_path, "files/score.dat");
    CHECK_EQ_STR(score, "score");

    RUN replay = {
        .role = RUN_REPLAY,
        .game_dir = root,
        .case_path = case_path,
        .out_dir = replay_dir,
    };
    Array_Init(&replay.shots.frames, &g_HeapAllocator);
    Array_Append(&replay.shots.frames, 1u);
    replay.shots.every = 4;
    CHECK(!M_Run(&port, &replay));
    char *const report = M_ReadText(replay_dir, "report.ini");
    CHECK(report != nullptr && strstr(report, "[difference]") == nullptr);
    CHECK(report != nullptr && strstr(report, "shots = 3") != nullptr);
    char *const shot = TestDir_Join(replay_dir, "shots/000004.png");
    CHECK(File_Exists(shot));

    game.bias = 1;
    CHECK(M_Run(&port, &replay));
    Mem_Free(report);
    char *const different = M_ReadText(replay_dir, "report.ini");
    CHECK_EQ_STR(
        different,
        "[report]\n"
        "frames = 8\n"
        "rows = 8\n"
        "shots = 3\n"
        "different_choices = 0\n"
        "\n"
        "[difference]\n"
        "row = 0\n"
        "frame = 0\n"
        "running_hash = expected 1, actual 0\n"
        "\n"
        "[check]\n"
        "row = 7\n"
        "bullets = expected 00000000, actual 00000001\n");

    Array_Free(&replay.shots.frames);
    Mem_Free(different);
    Mem_Free(score);
    Mem_Free(shot);
    Mem_Free(file);
    Mem_Free(manifest);
    Mem_Free(inputs);
    Mem_Free(replay_dir);
    Mem_Free(case_path);
    Mem_Free(capture_dir);
    Mem_Free(score_file);
    Mem_Free(slot);
    Mem_Free(stage_dir);
    Mem_Free(script);
    Mem_Free(script_dir);
    Mem_Free(root);
}

TEST(a_staged_path_must_stay_below_the_game_directory)
{
    char *const root = TestDir_Prepare("driver_stage");
    char *const out_dir = TestDir_Join(root, "out");
    char *const script = TestDir_Join(root, "inputs.txt");
    M_WriteText(root, "inputs.txt", "frames 1\n");
    M_GAME game = {};
    const PORT port = { .game = &m_TestGame, .context = &game };
    const char *const paths[] = { "../x", "Save/x", "save//x" };
    for (size_t i = 0; i < ARRAY_SIZE(paths); i++) {
        const RUN capture = {
            .role = RUN_CAPTURE,
            .game_dir = root,
            .script = script,
            .case_path = out_dir,
            .out_dir = out_dir,
            .files = { { script, paths[i] } },
            .file_count = 1,
        };
        DRIVER *driver;
        CHECK(!Result_Ignore(Driver_Open(&port, &capture, "native", &driver)));
    }
    Mem_Free(script);
    Mem_Free(out_dir);
    Mem_Free(root);
}

TEST(a_failed_run_writes_no_case_or_report)
{
    char *const root = TestDir_Prepare("driver_failed");
    char *const out_dir = TestDir_Join(root, "out");
    char *const case_path = TestDir_Join(root, "failed.othtc");
    char *const script = TestDir_Join(root, "inputs.txt");
    M_WriteText(root, "inputs.txt", "frames 3\n");
    M_GAME game = {};
    const PORT port = {
        .game = &m_TestGame,
        .context = &game,
        .sample = M_Sample,
        .idle = M_Idle,
        .seed = M_Seed,
    };
    const RUN capture = {
        .role = RUN_CAPTURE,
        .game_dir = root,
        .script = script,
        .case_path = case_path,
        .out_dir = out_dir,
    };
    DRIVER *driver;
    CHECK(Result_Ignore(Driver_Open(&port, &capture, "native", &driver)));
    Driver_EndCalc(driver);
    CHECK(Result_Ignore(Driver_EndFrame(driver, nullptr)));
    bool differs = true;
    CHECK(Result_Ignore(Driver_Close(driver, false, &differs)));
    CHECK(!differs);
    CHECK(!File_Exists(case_path));
    CHECK(!M_Run(&port, &capture));
    const RUN replay = {
        .role = RUN_REPLAY,
        .game_dir = root,
        .case_path = case_path,
        .out_dir = out_dir,
    };
    CHECK(Result_Ignore(Driver_Open(&port, &replay, "native", &driver)));
    CHECK(Result_Ignore(Driver_Close(driver, false, &differs)));
    char *const report = M_ReadText(out_dir, "report.ini");
    CHECK(report == nullptr);
    Mem_Free(script);
    Mem_Free(case_path);
    Mem_Free(out_dir);
    Mem_Free(root);
}

TEST(the_port_files_lie_beneath_the_staged_ones)
{
    char *const root = TestDir_Prepare("driver_port_files");
    char *const out_dir = TestDir_Join(root, "out");
    char *const script = TestDir_Join(root, "inputs.txt");
    M_WriteText(root, "inputs.txt", "frames 1\n");
    static const STAGED_FILE files[] = {
        { "game.cfg", (const uint8_t *)"port", 4 },
        { "other.cfg", (const uint8_t *)"other", 5 },
    };
    M_GAME game = {};
    const PORT port = {
        .game = &m_TestGame,
        .context = &game,
        .files = files,
        .file_count = ARRAY_SIZE(files),
    };
    const RUN capture = {
        .role = RUN_CAPTURE,
        .game_dir = root,
        .script = script,
        .case_path = out_dir,
        .out_dir = out_dir,
        .files = { { script, "game.cfg" } },
        .file_count = 1,
    };
    DRIVER *driver;
    CHECK(Result_Ignore(Driver_Open(&port, &capture, "native", &driver)));
    VFS *const vfs = Vfs_Create(&g_HeapAllocator);
    Driver_MountStaged(driver, vfs);
    READER file;
    CHECK(Result_Ignore(Vfs_Open(vfs, "game.cfg", VFS_IN_MEMORY, &file)));
    CHECK_EQ_INT(Reader_Remaining(&file), 9);
    Reader_Close(&file);
    CHECK(Result_Ignore(Vfs_Open(vfs, "other.cfg", VFS_IN_MEMORY, &file)));
    CHECK_EQ_INT(Reader_Remaining(&file), 5);
    Reader_Close(&file);
    Vfs_Destroy(vfs);
    bool differs;
    Result_Ignore(Driver_Close(driver, false, &differs));
    Mem_Free(script);
    Mem_Free(out_dir);
    Mem_Free(root);
}

TEST(the_overlay_serves_the_staged_files_and_takes_the_writes)
{
    char *const root = TestDir_Prepare("driver_overlay");
    char *const out_dir = TestDir_Join(root, "out");
    char *const work = TestDir_Join(out_dir, "work");
    char *const script = TestDir_Join(root, "inputs.txt");
    M_WriteText(root, "inputs.txt", "frames 1\n");
    M_GAME game = {};
    const PORT port = { .game = &m_TestGame, .context = &game };
    const RUN capture = {
        .role = RUN_CAPTURE,
        .game_dir = root,
        .script = script,
        .case_path = out_dir,
        .out_dir = out_dir,
        .files = { { script, "save/a/x.txt" } },
        .file_count = 1,
    };
    DRIVER *driver;
    CHECK(Result_Ignore(Driver_Open(&port, &capture, "native", &driver)));
    size_t count;
    const STAGED_FILE *const staged = Driver_Staged(driver, &count);
    CHECK_EQ_INT(count, 1);
    CHECK_EQ_STR(staged[0].path, "save/a/x.txt");
    VFS *const vfs = Vfs_Create(&g_HeapAllocator);
    Driver_MountStaged(driver, vfs);
    CHECK(Result_Ignore(Driver_MountWork(driver, vfs)));
    READER file;
    CHECK(Result_Ignore(Vfs_Open(vfs, "Save/A/X.txt", VFS_IN_MEMORY, &file)));
    CHECK_EQ_INT(Reader_Remaining(&file), 9);
    Reader_Close(&file);
    CHECK(Result_Ignore(Vfs_Write(vfs, "save/y.txt", "y", 1)));
    Vfs_Destroy(vfs);
    char *const written = M_ReadText(work, "save/y.txt");
    CHECK_EQ_STR(written, "y");
    bool differs;
    Result_Ignore(Driver_Close(driver, true, &differs));
    Mem_Free(written);
    Mem_Free(script);
    Mem_Free(work);
    Mem_Free(out_dir);
    Mem_Free(root);
}

// Parses args, which end at nullptr, into run.
static bool M_Parse(char **const args, ARENA *const arena, RUN *const run)
{
    int argc = 0;
    while (args[argc] != nullptr) {
        argc++;
    }
    bool help;
    return Result_Ignore(Run_Parse("test", argc, args, arena, run, &help));
}

TEST(a_command_line_gives_every_option)
{
    char *args[] = {
        "test",       "capture", "/c.othtc", "--data",     "/g",
        "--out",      "/o",      "--script", "/s.txt",     "--file",
        "/x/a=b.rpy", "--file",  "/y=c/d",   "--headless", nullptr,
    };
    ARENA arena;
    Arena_Init(&arena, "test", M_ARENA_SIZE);
    RUN run;
    CHECK(M_Parse(args, &arena, &run));
    CHECK_EQ_INT(run.role, RUN_CAPTURE);
    CHECK(run.headless);
    CHECK_EQ_STR(run.game_dir, "/g");
    CHECK_EQ_STR(run.case_path, "/c.othtc");
    CHECK_EQ_INT(run.file_count, 2);
    CHECK_EQ_STR(run.files[0].file, "/x/a");
    CHECK_EQ_STR(run.files[0].path, "b.rpy");
    CHECK_EQ_STR(run.files[1].path, "c/d");
    char *replay[] = {
        "test", "replay",  "/c.othtc",          "--data", "/g", "--out",
        "/o",   "--shots", "100,200,every:600", nullptr,
    };
    CHECK(M_Parse(replay, &arena, &run));
    CHECK_EQ_INT(run.role, RUN_REPLAY);
    CHECK(run.against_dir == nullptr);
    CHECK_EQ_INT(run.shots.frames.count, 2);
    CHECK_EQ_INT(run.shots.every, 600);
    Arena_Free(&arena);
}

TEST(a_replay_against_another_run_differs_by_its_shots)
{
    char *const root = TestDir_Prepare("driver_against");
    char *const script = TestDir_Join(root, "inputs.txt");
    char *const case_path = TestDir_Join(root, "against.othtc");
    char *const capture_dir = TestDir_Join(root, "capture");
    char *const first = TestDir_Join(root, "first");
    char *const second = TestDir_Join(root, "second");
    M_WriteText(root, "inputs.txt", "input 2 shoot\ninput 5 none\n");
    M_GAME game = {};
    const PORT port = {
        .game = &m_TestGame,
        .context = &game,
        .sample = M_Sample,
        .idle = M_Idle,
        .seed = M_Seed,
    };
    const RUN capture = {
        .role = RUN_CAPTURE,
        .game_dir = root,
        .script = script,
        .case_path = case_path,
        .out_dir = capture_dir,
    };
    CHECK(!M_Run(&port, &capture));
    char *args[] = {
        "test", "replay",  case_path, "--data",    root, "--out",
        first,  "--shots", "1,4",     "--against", "",   nullptr,
    };
    ARENA arena;
    Arena_Init(&arena, "test", M_ARENA_SIZE);
    RUN replay;
    args[9] = nullptr;
    CHECK(M_Parse(args, &arena, &replay));
    CHECK(!M_Run(&port, &replay));

    args[6] = second;
    args[9] = "--against";
    args[10] = first;
    CHECK(M_Parse(args, &arena, &replay));
    CHECK(!M_Run(&port, &replay));
    char *const same = M_ReadText(second, "report.ini");
    CHECK(same != nullptr && strstr(same, "different_shots = 0") != nullptr);

    game.shade = 1;
    CHECK(M_Run(&port, &replay));
    char *const report = M_ReadText(second, "report.ini");
    CHECK(report != nullptr && strstr(report, "[difference]") == nullptr);
    CHECK(
        report != nullptr && strstr(report, "different_shots = 2") != nullptr);
    char *const xor = TestDir_Join(second, "xor/000004.png");
    CHECK(File_Exists(xor));

    Mem_Free(xor);
    Mem_Free(report);
    Mem_Free(same);
    Arena_Free(&arena);
    Mem_Free(second);
    Mem_Free(first);
    Mem_Free(capture_dir);
    Mem_Free(case_path);
    Mem_Free(script);
    Mem_Free(root);
}

TEST(a_command_line_must_fit_its_role)
{
    char *missing[] = { "test", "capture", "/c", "--data", "/g", nullptr };
    char *no_script[] = {
        "test", "capture", "/c", "--data", "/g", "--out", "/o", nullptr,
    };
    char *live_replay[] = {
        "test",  "replay", "/c",     "--data", "/g",
        "--out", "/o",     "--live", nullptr,
    };
    char *headless_live[] = {
        "test",  "capture", "/c",     "--data",     "/g",
        "--out", "/o",      "--live", "--headless", nullptr,
    };
    char *capture_shots[] = {
        "test", "capture", "/c",      "--data", "/g",    "--out",
        "/o",   "--live",  "--shots", "1",      nullptr,
    };
    char *bad_file[] = {
        "test", "capture", "/c",     "--data", "/g",    "--out",
        "/o",   "--live",  "--file", "x",      nullptr,
    };
    char **const lines[] = {
        missing, no_script, live_replay, headless_live, capture_shots, bad_file,
    };
    for (size_t i = 0; i < ARRAY_SIZE(lines); i++) {
        ARENA arena;
        Arena_Init(&arena, "test", M_ARENA_SIZE);
        RUN run;
        CHECK(!M_Parse(lines[i], &arena, &run));
        Arena_Free(&arena);
    }
}

TEST(a_case_of_another_version_fails)
{
    char *const root = TestDir_Prepare("driver_reject");
    char *const script_dir = TestDir_Join(root, "script");
    char *const script = TestDir_Join(script_dir, "inputs.txt");
    char *const capture_dir = TestDir_Join(root, "capture");
    char *const case_path = TestDir_Join(root, "test.othtc");
    char *const replay_dir = TestDir_Join(root, "replay");
    M_WriteText(script_dir, "inputs.txt", "frames 2\n");
    HARNESS_GAME other = m_TestGame;
    other.version = "0.9";
    M_GAME game = {};
    PORT port = {
        .game = &other,
        .context = &game,
        .sample = M_Sample,
        .idle = M_Idle,
        .seed = M_Seed,
    };
    const RUN capture = {
        .role = RUN_CAPTURE,
        .game_dir = root,
        .script = script,
        .case_path = case_path,
        .out_dir = capture_dir,
    };
    CHECK(!M_Run(&port, &capture));

    port.game = &m_TestGame;
    const RUN replay = {
        .role = RUN_REPLAY,
        .game_dir = root,
        .case_path = case_path,
        .out_dir = replay_dir,
    };
    DRIVER *driver;
    CHECK(!Result_Ignore(Driver_Open(&port, &replay, "native", &driver)));
    Mem_Free(replay_dir);
    Mem_Free(case_path);
    Mem_Free(capture_dir);
    Mem_Free(script);
    Mem_Free(script_dir);
    Mem_Free(root);
}

// Replays the case at case_path into out_dir with screenshots of the
// frames of shots, and returns the frames that draw, one bit each.
static uint32_t M_Draws(
    const PORT *const port, const char *const case_path,
    const char *const out_dir, const bool headless, const bool fast,
    const uint32_t shots)
{
    RUN replay = {
        .role = RUN_REPLAY,
        .game_dir = out_dir,
        .case_path = case_path,
        .out_dir = out_dir,
        .headless = headless,
        .fast = fast,
    };
    Array_Init(&replay.shots.frames, &g_HeapAllocator);
    for (uint32_t frame = 0; frame < 32; frame++) {
        if ((shots >> frame & 1) != 0) {
            Array_Append(&replay.shots.frames, frame);
        }
    }
    DRIVER *driver;
    CHECK(Result_Ignore(Driver_Open(port, &replay, "native", &driver)));
    M_GAME *const game = port->context;
    *game = (M_GAME) {};
    IMAGE screen = Image_Create(4, 2, &g_HeapAllocator);
    uint32_t draws = 0;
    while (!Driver_Done(driver)) {
        game->score += Driver_Buttons(driver, 0);
        Driver_EndCalc(driver);
        Driver_Fps(driver, (FPS) { game->frame >= 3, 60.0f });
        draws |= Driver_WantsDraw(driver) ? 1u << game->frame : 0;
        CHECK(Result_Ignore(Driver_EndFrame(
            driver, Driver_WantsShot(driver) ? &screen : nullptr)));
        game->frame++;
    }
    Image_Free(&screen);
    bool differs;
    CHECK(Result_Ignore(Driver_Close(driver, true, &differs)));
    CHECK(!differs);
    Array_Free(&replay.shots.frames);
    return draws;
}

TEST(a_replay_draws_only_before_its_shots)
{
    char *const root = TestDir_Prepare("driver_draws");
    char *const script = TestDir_Join(root, "inputs.txt");
    char *const case_path = TestDir_Join(root, "draws.othtc");
    char *const out_dir = TestDir_Join(root, "out");
    M_WriteText(root, "inputs.txt", "input 2 shoot\ninput 5 none\n");
    M_GAME game = {};
    const PORT port = {
        .game = &m_TestGame,
        .context = &game,
        .sample = M_Sample,
        .idle = M_Idle,
        .seed = M_Seed,
        .request_full_repaint = M_RequestFullRepaint,
    };
    const RUN capture = {
        .role = RUN_CAPTURE,
        .game_dir = root,
        .script = script,
        .case_path = case_path,
        .out_dir = out_dir,
    };
    CHECK(!M_Run(&port, &capture));
    CHECK_EQ_INT(M_Draws(&port, case_path, out_dir, false, false, 0), 0xff);
    CHECK_EQ_INT(game.repaints, 0);
    CHECK_EQ_INT(M_Draws(&port, case_path, out_dir, false, false, 0x10), 0x1f);
    CHECK_EQ_INT(game.repaints, 0);
    CHECK_EQ_INT(M_Draws(&port, case_path, out_dir, false, true, 0x10), 0x18);
    CHECK_EQ_INT(game.repaints, 0x08);
    CHECK_EQ_INT(M_Draws(&port, case_path, out_dir, false, true, 0x14), 0x1e);
    CHECK_EQ_INT(game.repaints, 0x02);
    CHECK_EQ_INT(M_Draws(&port, case_path, out_dir, true, false, 0), 0);
    Mem_Free(out_dir);
    Mem_Free(case_path);
    Mem_Free(script);
    Mem_Free(root);
}

TEST(a_shot_list_can_come_from_a_file)
{
    char *const root = TestDir_Prepare("driver_shots");
    M_WriteText(root, "shots.txt", "# shots\n2, 5\nevery:3 # tail\n");
    char *const path = TestDir_Join(root, "shots.txt");
    char *const spec = Str_Format("@%s", path);
    char *args[] = {
        "test",  "replay", "/c.othtc", "--data", "/g",
        "--out", "/o",     "--shots",  spec,     nullptr,
    };
    ARENA arena;
    Arena_Init(&arena, "test", M_ARENA_SIZE);
    RUN run;
    CHECK(M_Parse(args, &arena, &run));
    CHECK_EQ_INT(run.shots.frames.count, 2);
    CHECK_EQ_INT(run.shots.frames.items[0], 2);
    CHECK_EQ_INT(run.shots.frames.items[1], 5);
    CHECK_EQ_INT(run.shots.every, 3);
    Arena_Free(&arena);
    Mem_Free(spec);
    Mem_Free(path);
    Mem_Free(root);
}
