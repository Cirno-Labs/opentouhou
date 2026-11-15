#include "../core/launcher.h"

#include "core/base/assert.h"
#include "core/base/memory.h"
#include "core/base/utils.h"
#include "core/io/writer.h"

#include <windows.h>

// Writes the shipped key mapping, windowed, no frame skip, no sound.
static void M_WriteConfig(WRITER *const writer)
{
    // Shoot, bomb, focus, menu, up, down, left, right, skip.
    const int16_t buttons[] = { 0, 1, 2, 4, -1, -1, -1, -1, 3 };
    for (size_t i = 0; i < ARRAY_SIZE(buttons); i++) {
        Writer_Write(writer, &buttons[i]);
    }
    Writer_WriteU16(writer, 0);
    Writer_WriteU32(writer, 0x102); // version
    Writer_WriteU8(writer, 2); // lives
    Writer_WriteU8(writer, 3); // bombs
    Writer_WriteU8(writer, 0); // 16-bit colour
    Writer_WriteU8(writer, 0); // music mode
    Writer_WriteU8(writer, 0); // sound effects
    Writer_WriteU8(writer, 1); // difficulty
    Writer_WriteU8(writer, 1); // windowed
    Writer_WriteU8(writer, 0); // frame skip
    Writer_WriteI16(writer, 600); // pad threshold x
    Writer_WriteI16(writer, 600); // pad threshold y
    for (int i = 0; i < 4; i++) {
        Writer_WriteU32(writer, 0);
    }
    Writer_WriteU32(writer, 1); // flags: colour-add emulation
    ASSERT(Writer_Tell(writer) == 0x38);
}

static const LAUNCHER_GAME m_Game = {
    .game_id = L"th06",
    .exe_name = "東方紅魔郷.exe",
    .exe_sha1 = "e61b4f4fea3802e926ef307f45166599c3e86555",
    .version = "1.02h",
    .cfg_name = "東方紅魔郷.cfg",
    // Names the replay copy so that the game's replay menu lists it.
    .replay_copy = L"replay\\th6_ud0000.rpy",
    .write_cfg = M_WriteConfig,
};

int main(void)
{
    int argc;
    wchar_t **const wide_argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    char **const argv = Mem_AllocArr((size_t)argc, sizeof(char *));
    for (int i = 0; i < argc; i++) {
        const int size = WideCharToMultiByte(
            CP_UTF8, 0, wide_argv[i], -1, nullptr, 0, nullptr, nullptr);
        argv[i] = Mem_Alloc((size_t)size);
        WideCharToMultiByte(
            CP_UTF8, 0, wide_argv[i], -1, argv[i], size, nullptr, nullptr);
    }
    LocalFree(wide_argv);
    const int exit_code = Launcher_Main(&m_Game, argc, argv);
    for (int i = 0; i < argc; i++) {
        Mem_Free(argv[i]);
    }
    Mem_Free(argv);
    return exit_code;
}
