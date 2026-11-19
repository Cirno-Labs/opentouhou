#include "backend.h"
#include "case.h"
#include "script.h"

#include "core/base/string.h"
#include "core/io/reader.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    const char *role;
    const char *case_path;
    const char *game_dir;
    const char *out_dir;
    const char *script;
    ARG_ITEMS files;
    const char *note;
    const char *shots;
    const char *against_dir;
    bool headless;
    bool fast;
    bool live;
} M_ARGS;

static const ARG_OPTION m_Options[] = {
    ARG_STRING_OF(
        M_ARGS, "role", role, "capture|replay", "what the run does",
        ARG_REQUIRED),
    ARG_STRING_OF(
        M_ARGS, "case", case_path, "<case>",
        "the case file that a capture writes and a replay reads", ARG_REQUIRED),
    ARG_STRING_OF(
        M_ARGS, "--data", game_dir, "<dir>", "the game's directory",
        ARG_REQUIRED),
    ARG_STRING_OF(
        M_ARGS, "--out", out_dir, "<dir>", "the output directory",
        ARG_REQUIRED),
    ARG_STRING_OF(
        M_ARGS, "--script", script, "<file>", "a capture's input script"),
    ARG_LIST_OF(
        M_ARGS, "--file", files, "<file>=<path>",
        "a capture stages file at path among the game's files"),
    ARG_STRING_OF(M_ARGS, "--note", note, "<text>", "a capture's note"),
    ARG_STRING_OF(
        M_ARGS, "--shots", shots, "<frames>",
        "a replay's screenshot frames, such as 21345 or 0,100,every:300, or "
        "@file with such frames"),
    ARG_STRING_OF(
        M_ARGS, "--against", against_dir, "<dir>",
        "a replay compares its screenshots with this earlier run's"),
    ARG_FLAG_OF(
        M_ARGS, "--headless", headless, "draw nothing and take no screenshots"),
    ARG_FLAG_OF(
        M_ARGS, "--fast", fast,
        "draw only the frames just before each screenshot"),
    ARG_FLAG_OF(M_ARGS, "--live", live, "a capture records a person's play"),
};

static const ARG_RULE m_Rules[] = {
    { ARG_EXCLUSIVE, { "--script", "--live" } },
    { ARG_EXCLUSIVE, { "--live", "--headless" } },
    { ARG_EXCLUSIVE, { "--headless", "--fast" } },
    { ARG_EXCLUSIVE, { "--headless", "--shots" } },
};

// Reads one item of a screenshot list into out.
static RESULT M_ReadShot(const char *const item, SHOTS *const out)
{
    uint32_t frame;
    if (Str_StartsWith(item, "every:")) {
        FAIL_IF(
            !Str_ParseU32(item + 6, &out->every) || out->every == 0,
            "bad screenshot interval %s", item);
    } else {
        FAIL_IF(!Str_ParseU32(item, &frame), "bad screenshot frame %s", item);
        Array_Append(&out->frames, frame);
    }
    return OK;
}

// Reads the items of text, separated by commas or white space, with '#'
// comments to the end of the line. Changes text.
static RESULT M_ReadShotList(char *text, SHOTS *const out)
{
    for (char *line; (line = Script_NextLine(&text)) != nullptr;) {
        for (char *item = strtok(line, ", \t\r"); item != nullptr;
             item = strtok(nullptr, ", \t\r")) {
            MUST(M_ReadShot(item, out));
        }
    }
    return OK;
}

// Reads a --shots spec: a list, or @FILE that holds one.
static RESULT M_ReadShots(
    const char *const spec, ARENA *const arena, SHOTS *const out)
{
    Array_Init(&out->frames, Arena_Allocator(arena));
    out->every = 0;
    if (spec == nullptr || spec[0] == '\0') {
        return OK;
    }
    if (spec[0] != '@') {
        return M_ReadShotList(Str_DupWith(Arena_Allocator(arena), spec), out);
    }
    READER reader;
    MUST(Reader_OpenFile(&reader, spec + 1));
    size_t size;
    char *const text =
        Reader_ReadCString(&reader, Arena_Allocator(arena), &size);
    const RESULT read = Reader_Result(&reader);
    Reader_Close(&reader);
    MUST(read);
    return Result_Prefix(M_ReadShotList(text, out), "%s", spec + 1);
}

static RESULT M_ParseFiles(
    const ARG_SCHEMA *const schema, const ARG_ITEMS *const items,
    ARENA *const arena, RUN *const run)
{
    for (size_t i = 0; i < items->count; i++) {
        char *const copy = Str_DupWith(Arena_Allocator(arena), items->items[i]);
        char *const sep = strrchr(copy, '=');
        if (sep == nullptr || sep == copy || sep[1] == '\0') {
            return Args_Fail(
                schema, "--file %s is not FILE=PATH", items->items[i]);
        }
        *sep = '\0';
        run->files[run->file_count++] =
            (RUN_FILE) { .file = copy, .path = sep + 1 };
    }
    return OK;
}

RESULT Run_Parse(
    const char *const program, const int argc, char *const argv[],
    ARENA *const arena, RUN *const out, bool *const help)
{
    const ARG_SCHEMA schema = ARG_SCHEMA_OF(
        program, "Runs a game under the replay harness.", m_Options, m_Rules);
    M_ARGS args;
    MUST(Args_Parse(&schema, argc, argv, &args, help));
    if (*help) {
        Args_PrintHelp(&schema, stdout);
        return OK;
    }
    RUN run = {
        .case_path = args.case_path,
        .game_dir = args.game_dir,
        .out_dir = args.out_dir,
        .script = args.script,
        .note = args.note,
        .against_dir = args.against_dir,
        .headless = args.headless,
        .fast = args.fast,
        .live = args.live,
    };
    if (strcmp(args.role, "capture") == 0) {
        run.role = RUN_CAPTURE;
        if (run.script == nullptr && !run.live) {
            return Args_Fail(&schema, "a capture takes --script or --live");
        }
        if (args.shots != nullptr || run.against_dir != nullptr || run.fast) {
            return Args_Fail(&schema, "a capture takes no screenshots");
        }
        MUST(M_ParseFiles(&schema, &args.files, arena, &run));
    } else if (strcmp(args.role, "replay") == 0) {
        run.role = RUN_REPLAY;
        if (run.script != nullptr || run.live || args.files.count > 0
            || run.note != nullptr) {
            return Args_Fail(
                &schema, "a replay takes its script and files from its case");
        }
    } else {
        return Args_Fail(&schema, "the role is capture or replay");
    }
    MUST(M_ReadShots(args.shots, arena, &run.shots));
    *out = run;
    return OK;
}
