// Uses realpath and mkdtemp of POSIX 2008.
#define _XOPEN_SOURCE 700

#include "dosbox.h"

#include "core/base/log.h"
#include "core/base/string.h"
#include "core/hash/sha1.h"
#include "core/io/file.h"
#include "core/io/path.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define M_ARENA_SIZE (1024 * 1024)
#define M_CONFIG "dosbox-x.conf"

// GAMEdosbox, with the command line of Run_Parse and absolute paths,
// checks the files of game_dir, writes a configuration of dosbox-x.conf
// with the autoexec commands into a temporary directory and runs dosbox-x
// from PATH with GAMEdosbox.so, which takes the same command line after
// "--".

static RESULT M_HashFile(
    const char *const path, ARENA *const arena, const char **const out)
{
    READER reader;
    MUST(Reader_OpenFile(&reader, path));
    SHA1 sha1;
    Sha1_Init(&sha1);
    uint8_t buffer[65536];
    for (size_t left = Reader_Remaining(&reader); left > 0;) {
        const size_t size = left < sizeof(buffer) ? left : sizeof(buffer);
        Reader_ReadBytes(&reader, buffer, size);
        Sha1_Update(&sha1, buffer, size);
        left -= size;
    }
    const RESULT result = Reader_Result(&reader);
    Reader_Close(&reader);
    MUST(result);
    char *const hex = Arena_Alloc(arena, SHA1_HEX_SIZE);
    Sha1_ToHex(Sha1_Final(&sha1), hex);
    *out = hex;
    return OK;
}

static RESULT M_CheckFiles(
    const DOSBOX_PROGRAM *const program, const RUN *const run,
    ARENA *const arena)
{
    for (size_t i = 0; i < program->file_count; i++) {
        const DOSBOX_FILE *const file = &program->files[i];
        const char *const path =
            Path_Join(Arena_Allocator(arena), run->game_dir, file->name);
        const char *sha1;
        MUST(M_HashFile(path, arena, &sha1));
        FAIL_IF(
            strcmp(sha1, file->sha1) != 0, "%s has SHA-1 %s, not %s", path,
            sha1, file->sha1);
    }
    return OK;
}

// The overlay takes every write, so work/ must not lie in game_dir, where
// the writes would land in the game's files.
static RESULT M_CheckOut(const RUN *const run)
{
    char game[PATH_MAX];
    char out[PATH_MAX];
    MUST(File_MakeDir(run->out_dir));
    FAIL_IF(
        realpath(run->game_dir, game) == nullptr
            || realpath(run->out_dir, out) == nullptr,
        "cannot resolve %s or %s", run->game_dir, run->out_dir);
    const size_t length = strlen(game);
    FAIL_IF(
        strncmp(game, out, length) == 0
            && (out[length] == '/' || out[length] == '\0'),
        "out_dir lies in game_dir");
    return OK;
}

// Writes the configuration of the run: dosbox-x.conf, then the autoexec
// section.
static RESULT M_WriteConfig(
    const DOSBOX_PROGRAM *const program, const char *const base,
    const char *const path)
{
    READER reader;
    MUST(Reader_OpenFile(&reader, base));
    const size_t size = Reader_Remaining(&reader);
    char *const text = Mem_Alloc(size + 1);
    Reader_ReadBytes(&reader, text, size);
    const RESULT read = Reader_Result(&reader);
    Reader_Close(&reader);
    FILE *file = nullptr;
    RESULT result = read;
    if (IS_OK(result)) {
        result = File_CreateText(path, &file);
    }
    if (IS_OK(result)) {
        fwrite(text, 1, size, file);
        fprintf(file, "\n[autoexec]\nC:\n");
        for (size_t i = 0; i < program->command_count; i++) {
            fprintf(file, "%s\n", program->commands[i]);
        }
        fprintf(file, "exit\n");
        if (fclose(file) != 0) {
            result = FAIL("cannot write %s", path);
        }
    }
    Mem_Free(text);
    return result;
}

static RESULT M_Run(
    const char *const *const args, const char *const dir, int *const status)
{
    fflush(stdout);
    fflush(stderr);
    const pid_t child = fork();
    FAIL_IF(child < 0, "cannot start dosbox-x");
    if (child == 0) {
        if (chdir(dir) == 0) {
            execvp(args[0], (char *const *)args);
        }
        perror("dosbox-x");
        _exit(127);
    }
    int wait_status = 0;
    FAIL_IF(waitpid(child, &wait_status, 0) < 0, "cannot wait for dosbox-x");
    FAIL_IF(!WIFEXITED(wait_status), "dosbox-x crashed");
    *status = WEXITSTATUS(wait_status);
    FAIL_IF(*status == 127, "cannot run dosbox-x");
    return OK;
}

static RESULT M_Launch(
    const DOSBOX_PROGRAM *const program, const int argc, char **const argv,
    const RUN *const run, ARENA *const arena, int *const status)
{
    FAIL_IF(run->live, "the dosbox backend takes no --live");
    MUST(M_CheckFiles(program, run, arena));
    MUST(M_CheckOut(run));
    const ALLOCATOR *const allocator = Arena_Allocator(arena);
    const char *const exe_dir = Path_GetExeDir();
    FAIL_IF(exe_dir == nullptr, "cannot find the backend program");
    const char *const library = Path_Join(allocator, exe_dir, program->library);
    const char *const base = Path_Join(allocator, exe_dir, M_CONFIG);

    char dir[] = "/tmp/dosbox-XXXXXX";
    FAIL_IF(mkdtemp(dir) == nullptr, "cannot make a temporary directory");
    const char *const config = Path_Join(allocator, dir, M_CONFIG);
    RESULT result = M_WriteConfig(program, base, config);
    if (IS_OK(result)) {
        const char **const args =
            Arena_AllocArrZ(arena, (size_t)argc + 7, sizeof(char *));
        size_t count = 0;
        args[count++] = "dosbox-x";
        args[count++] = "-conf";
        args[count++] = config;
        args[count++] = "-harness";
        args[count++] = library;
        args[count++] = "--";
        for (int i = 0; i < argc; i++) {
            args[count++] = argv[i];
        }
        result = M_Run(args, dir, status);
    }
    Result_Ignore(File_Remove(config));
    Result_Ignore(File_Remove(dir));
    MUST(result);
    FAIL_IF(*status > 2, "dosbox-x exited with %d", *status);
    return OK;
}

int Dosbox_Launch(
    const DOSBOX_PROGRAM *const program, const int argc, char **const argv)
{
    ARENA arena;
    Arena_Init(&arena, "dosbox launch", M_ARENA_SIZE);
    int status = 2;
    RUN run;
    bool help = false;
    RESULT result = Run_Parse(argv[0], argc, argv, &arena, &run, &help);
    if (IS_OK(result) && !help) {
        result = M_Launch(program, argc, argv, &run, &arena, &status);
    }
    if (!Result_Log(result, LOG_LEVEL_ERROR)) {
        status = 2;
    } else if (help) {
        status = 0;
    }
    Arena_Free(&arena);
    return status;
}
