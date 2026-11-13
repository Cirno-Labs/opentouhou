#include "othpatch/context.h"

#include "core/base/memory.h"

#include "test.h"

#include <stdlib.h>
#include <string.h>

// Returns what was written to out, and closes it. Mem_Free releases the
// text.
static char *M_Read(FILE *const out)
{
    const long size = ftell(out);
    rewind(out);
    char *const text = Mem_AllocZ((size_t)size + 1);
    CHECK(fread(text, 1, (size_t)size, out) == (size_t)size);
    fclose(out);
    return text;
}

TEST(ansi_patch_shows_four_files_and_collapses)
{
    APP_CONTEXT run = { .out = tmpfile(), .terminal = true, .ansi = true };
    static const char *const files[] = { "a", "b", "c", "d", "e" };
    CONSOLE_PATCH patch;
    Console_StartPatch(&patch, &run, "Patch", 6, 5);
    for (int32_t i = 0; i < 5; i++) {
        Console_ReceiveFile(&patch, files[i], 1024, 2048);
    }
    Console_EndFile(&patch, files[0]);
    Console_ReceiveFile(&patch, files[4], 12 * 1024, 0);
    Console_ReceiveFile(&patch, files[1], 300 * 1024, 4 * 1024 * 1024);
    Console_EndPatch(&patch);
    char *const text = M_Read(run.out);
    CHECK_EQ_STR(
        text,
        "\x1b[2KPatch  [....................] 0/5 files\n"
        "\x1b[1A\x1b[2KPatch  [....................] 0/5 files\n"
        "\x1b[2K  a    [##########..........] 1/2 KB\n"
        "\x1b[2A\x1b[2KPatch  [....................] 0/5 files\n"
        "\x1b[2K  a    [##########..........] 1/2 KB\n"
        "\x1b[2K  b    [##########..........] 1/2 KB\n"
        "\x1b[3A\x1b[2KPatch  [....................] 0/5 files\n"
        "\x1b[2K  a    [##########..........] 1/2 KB\n"
        "\x1b[2K  b    [##########..........] 1/2 KB\n"
        "\x1b[2K  c    [##########..........] 1/2 KB\n"
        "\x1b[4A\x1b[2KPatch  [....................] 0/5 files\n"
        "\x1b[2K  a    [##########..........] 1/2 KB\n"
        "\x1b[2K  b    [##########..........] 1/2 KB\n"
        "\x1b[2K  c    [##########..........] 1/2 KB\n"
        "\x1b[2K  d    [##########..........] 1/2 KB\n"
        "\x1b[5A\x1b[2KPatch  [####................] 1/5 files\n"
        "\x1b[2K  b    [##########..........] 1/2 KB\n"
        "\x1b[2K  c    [##########..........] 1/2 KB\n"
        "\x1b[2K  d    [##########..........] 1/2 KB\n"
        "\x1b[2K\n\x1b[1A"
        "\x1b[4A\x1b[2KPatch  [####................] 1/5 files\n"
        "\x1b[2K  e    [....................] 12 KB\n"
        "\x1b[2K  b    [##########..........] 1/2 KB\n"
        "\x1b[2K  c    [##########..........] 1/2 KB\n"
        "\x1b[2K  d    [##########..........] 1/2 KB\n"
        "\x1b[5A\x1b[2KPatch  [####................] 1/5 files\n"
        "\x1b[2K  e    [....................] 12 KB\n"
        "\x1b[2K  b    [#...................] 0.3/4.0 MB\n"
        "\x1b[2K  c    [##########..........] 1/2 KB\n"
        "\x1b[2K  d    [##########..........] 1/2 KB\n"
        "\x1b[5A\x1b[2KPatch  [####................] 1/5 files\n"
        "\x1b[2K\n\x1b[2K\n\x1b[2K\n\x1b[2K\n\x1b[4A");
    Mem_Free(text);
}

TEST(ansi_file_under_1_kb_shows_bytes)
{
    APP_CONTEXT run = { .out = tmpfile(), .terminal = true, .ansi = true };
    CONSOLE_PATCH patch;
    Console_StartPatch(&patch, &run, "Patch", 6, 2);
    Console_ReceiveFile(&patch, "a", 0, 512);
    Console_ReceiveFile(&patch, "b", 512, 0);
    char *const text = M_Read(run.out);
    CHECK(strstr(text, "  a    [....................] 0/512 B\n") != nullptr);
    CHECK(strstr(text, "  b    [....................] 512 B\n") != nullptr);
    CHECK(strstr(text, "KB") == nullptr);
    Mem_Free(text);
}

TEST(ansi_patch_draws_its_last_line_once)
{
    APP_CONTEXT run = { .out = tmpfile(), .terminal = true, .ansi = true };
    CONSOLE_PATCH patch;
    Console_StartPatch(&patch, &run, "Patch", 6, 1);
    Console_ReceiveFile(&patch, "a", 10, 10);
    Console_EndFile(&patch, "a");
    Console_EndPatch(&patch);
    char *const text = M_Read(run.out);
    CHECK_EQ_STR(
        text,
        "\x1b[2KPatch  [....................] 0/1 file\n"
        "\x1b[1A\x1b[2KPatch  [....................] 0/1 file\n"
        "\x1b[2K  a    [####################] 10/10 B\n"
        "\x1b[2A\x1b[2KPatch  [####################] 1/1 file\n"
        "\x1b[2K\n\x1b[1A");
    Mem_Free(text);
}

TEST(terminal_without_ansi_redraws_one_line)
{
    APP_CONTEXT run = { .out = tmpfile(), .terminal = true };
    CONSOLE_PATCH patch;
    Console_StartPatch(&patch, &run, "Patch", 6, 2);
    Console_ReceiveFile(&patch, "a", 10, 10);
    Console_EndFile(&patch, "a");
    Console_EndPatch(&patch);
    char *const text = M_Read(run.out);
    CHECK_EQ_STR(
        text,
        "\rPatch  [....................] 0/2 files"
        "\rPatch  [##########..........] 1/2 files\n");
    Mem_Free(text);
}

TEST(file_prints_the_last_state_only)
{
    APP_CONTEXT run = { .out = tmpfile() };
    CONSOLE_PATCH patch;
    Console_StartPatch(&patch, &run, "Patch", 6, 2);
    Console_ReceiveFile(&patch, "a", 10, 10);
    Console_EndFile(&patch, "a");
    Console_EndPatch(&patch);
    char *const text = M_Read(run.out);
    CHECK_EQ_STR(text, "Patch: 1/2 files\n");
    Mem_Free(text);
}
