#include "overlay.h"

#include "core/base/log.h"

#include <windows.h>

#include <stdio.h>
#include <string.h>

// Checks the file overlay (overlay.c) without a game: the program installs
// the overlay over its own import table, with its working directory as the
// game directory (G) and argv[1] as the output directory (O). Both must be
// fresh, empty directories.

static int m_Failures;

static void M_Check(bool passed, const char *name)
{
    printf("%s: %s\n", passed ? "PASS" : "FAIL", name);
    if (!passed) {
        m_Failures++;
    }
}

static void M_WriteFileRaw(const char *path, const char *content)
{
    FILE *file = fopen(path, "wb");
    fwrite(content, 1, strlen(content), file);
    fclose(file);
}

static bool M_ExistsRaw(const char *path)
{
    return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
}

static bool M_ReadMatches(const char *path, const char *expected)
{
    HANDLE file = CreateFileA(
        path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    char buffer[256];
    DWORD read_size = 0;
    BOOL ok = ReadFile(file, buffer, sizeof(buffer) - 1, &read_size, nullptr);
    CloseHandle(file);
    if (!ok) {
        return false;
    }
    buffer[read_size] = '\0';
    return strcmp(buffer, expected) == 0;
}

static void M_CheckReadFromGame(const char *out_full)
{
    M_WriteFileRaw("readonly.txt", "from-game");

    bool read_ok = M_ReadMatches("readonly.txt", "from-game");
    M_Check(read_ok, "read-only file is read from G");

    char out_path[MAX_PATH];
    snprintf(out_path, sizeof(out_path), "%s\\readonly.txt", out_full);
    M_Check(!M_ExistsRaw(out_path), "read-only open does not create in O");
}

static void M_CheckWriteGoesToOut(const char *game_full, const char *out_full)
{
    HANDLE file = CreateFileA(
        "score.dat", GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    bool created = file != INVALID_HANDLE_VALUE;
    if (created) {
        DWORD written;
        WriteFile(file, "score-data", 10, &written, nullptr);
        CloseHandle(file);
    }
    M_Check(created, "CreateFileA(CREATE_ALWAYS) succeeds");

    char game_path[MAX_PATH];
    snprintf(game_path, sizeof(game_path), "%s\\score.dat", game_full);
    M_Check(!M_ExistsRaw(game_path), "score.dat is not created in G");

    char out_path[MAX_PATH];
    snprintf(out_path, sizeof(out_path), "%s\\score.dat", out_full);
    M_Check(
        M_ReadMatches(out_path, "score-data"),
        "score.dat exists in O with its content");
    M_Check(
        M_ReadMatches("score.dat", "score-data"),
        "reading score.dat afterwards gives O's content");
}

static void M_CheckCreateDirectory(const char *game_full, const char *out_full)
{
    BOOL created = CreateDirectoryA("replay", nullptr);
    M_Check(created, "CreateDirectoryA(\"replay\") succeeds");

    char game_path[MAX_PATH];
    snprintf(game_path, sizeof(game_path), "%s\\replay", game_full);
    M_Check(!M_ExistsRaw(game_path), "replay directory is not created in G");

    char out_path[MAX_PATH];
    snprintf(out_path, sizeof(out_path), "%s\\replay", out_full);
    M_Check(
        GetFileAttributesA(out_path) & FILE_ATTRIBUTE_DIRECTORY,
        "replay directory exists in O");

    HANDLE file = CreateFileA(
        "replay\\th6_ud0001.rpy", GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    bool created_file = file != INVALID_HANDLE_VALUE;
    if (created_file) {
        DWORD written;
        WriteFile(file, "replay-data", 11, &written, nullptr);
        CloseHandle(file);
    }

    char replay_path[MAX_PATH];
    snprintf(
        replay_path, sizeof(replay_path), "%s\\replay\\th6_ud0001.rpy",
        out_full);
    M_Check(
        created_file && M_ReadMatches(replay_path, "replay-data"),
        "replay/th6_ud0001.rpy lands in O/replay");
}

static void M_CheckRefusedWrite(const char *game_full)
{
    M_WriteFileRaw("gameonly.dat", "orig");

    HANDLE file = CreateFileA(
        "gameonly.dat", GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    DWORD error = GetLastError();
    M_Check(
        file == INVALID_HANDLE_VALUE && error == ERROR_ACCESS_DENIED,
        "write-open of a G-only file is refused");

    char game_path[MAX_PATH];
    snprintf(game_path, sizeof(game_path), "%s\\gameonly.dat", game_full);
    WIN32_FILE_ATTRIBUTE_DATA data;
    bool unchanged =
        GetFileAttributesExA(game_path, GetFileExInfoStandard, &data)
        && data.nFileSizeLow == 4;
    M_Check(unchanged, "G's file is unchanged by the refused open");
}

static void M_CheckDeleteFile(const char *game_full, const char *out_full)
{
    BOOL deleted_game_only = DeleteFileA("gameonly.dat");
    DWORD error = GetLastError();
    M_Check(
        !deleted_game_only && error == ERROR_FILE_NOT_FOUND,
        "deleting a G-only file fails with ERROR_FILE_NOT_FOUND");

    char game_path[MAX_PATH];
    snprintf(game_path, sizeof(game_path), "%s\\gameonly.dat", game_full);
    M_Check(M_ExistsRaw(game_path), "the G-only file is still in G");

    BOOL deleted_out = DeleteFileA("score.dat");
    M_Check(deleted_out, "deleting score.dat (in O) succeeds");

    char out_path[MAX_PATH];
    snprintf(out_path, sizeof(out_path), "%s\\score.dat", out_full);
    M_Check(!M_ExistsRaw(out_path), "score.dat is gone from O");
}

static void M_CheckFindFirstFile(void)
{
    M_WriteFileRaw("th6_ud0001.rpy", "g1");
    M_WriteFileRaw("th6_ud0002.rpy", "g2");
    HANDLE file = CreateFileA(
        "th6_ud0002.rpy", GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    CloseHandle(file);
    file = CreateFileA(
        "th6_ud0003.rpy", GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    CloseHandle(file);

    char names[8][MAX_PATH];
    int count = 0;
    WIN32_FIND_DATAA data;
    HANDLE find = FindFirstFileA("th6_ud????.rpy", &data);
    bool ud0002_before_ud0001 = false;
    if (find != INVALID_HANDLE_VALUE) {
        do {
            if (count < 8) {
                strcpy(names[count], data.cFileName);
            }
            if (strcmp(data.cFileName, "th6_ud0001.rpy") == 0) {
                ud0002_before_ud0001 = count > 0;
            }
            count++;
        } while (FindNextFileA(find, &data));
        FindClose(find);
    }

    M_Check(count == 3, "FindFirstFileA merges O and G with no duplicates");

    int seen_0001 = 0;
    int seen_0002 = 0;
    int seen_0003 = 0;
    for (int i = 0; i < count && i < 8; i++) {
        if (strcmp(names[i], "th6_ud0001.rpy") == 0) {
            seen_0001++;
        } else if (strcmp(names[i], "th6_ud0002.rpy") == 0) {
            seen_0002++;
        } else if (strcmp(names[i], "th6_ud0003.rpy") == 0) {
            seen_0003++;
        }
    }
    M_Check(
        seen_0001 == 1 && seen_0002 == 1 && seen_0003 == 1,
        "each replay name appears exactly once");
    M_Check(ud0002_before_ud0001, "O's matches are listed before G's");
}

static void M_CheckOutsidePathUntouched(void)
{
    char temp_dir[MAX_PATH];
    GetTempPathA(MAX_PATH, temp_dir);
    char outside_path[MAX_PATH];
    snprintf(
        outside_path, sizeof(outside_path), "%soverlay_test_outside.tmp",
        temp_dir);

    HANDLE file = CreateFileA(
        outside_path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    bool created = file != INVALID_HANDLE_VALUE;
    if (created) {
        DWORD written;
        WriteFile(file, "outside-data", 12, &written, nullptr);
        CloseHandle(file);
    }
    M_Check(
        created && M_ExistsRaw(outside_path),
        "a path outside G is created at its real location");

    BOOL deleted = DeleteFileA(outside_path);
    M_Check(deleted, "deleting a path outside G uses the real function");
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: overlay_test.exe <output directory>\n");
        return 1;
    }

    char game_full[MAX_PATH];
    GetFullPathNameA(".", MAX_PATH, game_full, nullptr);
    char out_full[MAX_PATH];
    GetFullPathNameA(argv[1], MAX_PATH, out_full, nullptr);

    wchar_t game_wide[MAX_PATH];
    wchar_t out_wide[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, game_full, -1, game_wide, MAX_PATH);
    MultiByteToWideChar(CP_ACP, 0, out_full, -1, out_wide, MAX_PATH);
    if (!Result_Log(Overlay_Install(game_wide, out_wide), LOG_LEVEL_ERROR)) {
        return 1;
    }

    M_CheckReadFromGame(out_full);
    M_CheckWriteGoesToOut(game_full, out_full);
    M_CheckCreateDirectory(game_full, out_full);
    M_CheckRefusedWrite(game_full);
    M_CheckDeleteFile(game_full, out_full);
    M_CheckFindFirstFile();
    M_CheckOutsidePathUntouched();

    if (m_Failures == 0) {
        printf("all checks passed\n");
    } else {
        printf("%d check(s) failed\n", m_Failures);
    }
    return m_Failures == 0 ? 0 : 1;
}
