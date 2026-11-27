#include "dosbox.h"

#include "core/base/utils.h"

int main(int argc, char **argv)
{
    static const DOSBOX_FILE files[] = {
        { "ZUN.COM", "dcef7e71f55b42c516de4915d704c81778d8e7d6" },
        { "OP.EXE", "7c3717c554b01e28339c700d0aa29b15646c742a" },
        { "MAIN.EXE", "511675251fc4dcc9539edcb3554a5f5277730d7f" },
        { "MAINE.EXE", "f1472765045963d350d3f99d29365484ebab6de7" },
        { "GAME.BAT", "98e988499d026d183dce7e223f7ef824719acc0c" },
        { "MMD.COM", "8b0da59644d14da1700e5841859677bbe1e5264e" },
        { "PMD.COM", "13692609d38e640d6368b24233a25e6b2d0b4c82" },
        { "PMD86.COM", "4a4d621eeca0e0fb6f42c024764d934e52778b02" },
        { "PMDB2.COM", "d047241b2cd66709e3259cd0c704563e35908448" },
    };
    static const char *const commands[] = { "GAME.BAT" };
    const DOSBOX_PROGRAM program = {
        .files = files,
        .file_count = ARRAY_SIZE(files),
        .commands = commands,
        .command_count = ARRAY_SIZE(commands),
        .library = "th02dosbox.so",
        .font = "th02-font.bmp",
    };
    return Dosbox_Launch(&program, argc, argv);
}
