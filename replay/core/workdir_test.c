// SPDX-License-Identifier: GPL-3.0-only

#include "workdir.h"

#include "core/base/log.h"

#include <windows.h>

#include <stdio.h>
#include <string.h>
#include <wchar.h>

// Checks the work directory helpers (workdir.c) without a game: the
// program copies a file from its working directory into a temporary work
// directory and from there to argv[1]. Both must be fresh, empty
// directories.

static int m_Failures;

static void M_Check(bool passed, const char *name)
{
    printf("%s: %s\n", passed ? "PASS" : "FAIL", name);
    if (!passed) {
        m_Failures++;
    }
}

static bool M_ReadMatches(const wchar_t *path, const char *expected)
{
    FILE *file = _wfopen(path, L"rb");
    if (file == nullptr) {
        return false;
    }
    char buffer[64];
    const size_t size = fread(buffer, 1, sizeof(buffer) - 1, file);
    fclose(file);
    buffer[size] = '\0';
    return strcmp(buffer, expected) == 0;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: workdir_test.exe <output directory>\n");
        return 1;
    }
    wchar_t out_dir[MAX_PATH];
    wchar_t out_arg[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, argv[1], -1, out_arg, MAX_PATH);
    GetFullPathNameW(out_arg, MAX_PATH, out_dir, nullptr);

    FILE *file = fopen("score.dat", "wb");
    fputs("seed", file);
    fclose(file);

    wchar_t work_dir[MAX_PATH];
    if (!Result_Log(
            Workdir_CreateTemp(L"test", work_dir, MAX_PATH), LOG_LEVEL_ERROR)) {
        return 1;
    }

    wchar_t source[MAX_PATH];
    GetFullPathNameW(L"score.dat", MAX_PATH, source, nullptr);
    M_Check(
        Result_Log(Workdir_CopyIn(work_dir, source), LOG_LEVEL_ERROR),
        "CopyIn succeeds");
    wchar_t path[MAX_PATH];
    swprintf(path, MAX_PATH, L"%ls\\score.dat", work_dir);
    M_Check(M_ReadMatches(path, "seed"), "the work directory has the file");

    file = _wfopen(path, L"wb");
    fputs("saved", file);
    fclose(file);
    M_Check(
        Result_Log(
            Workdir_CopyOut(work_dir, L"score.dat", out_dir), LOG_LEVEL_ERROR),
        "CopyOut succeeds");
    swprintf(path, MAX_PATH, L"%ls\\score.dat", out_dir);
    M_Check(
        M_ReadMatches(path, "saved"),
        "the output directory has the work directory's file");
    M_Check(M_ReadMatches(source, "seed"), "the source file is unchanged");

    M_Check(
        !Result_Ignore(Workdir_CopyOut(work_dir, L"missing.dat", out_dir)),
        "CopyOut of a missing file reports failure");

    Workdir_RemoveTemp(work_dir);
    M_Check(
        GetFileAttributesW(work_dir) == INVALID_FILE_ATTRIBUTES,
        "RemoveTemp removes the work directory");

    if (m_Failures == 0) {
        printf("all checks passed\n");
    } else {
        printf("%d check(s) failed\n", m_Failures);
    }
    return m_Failures == 0 ? 0 : 1;
}
