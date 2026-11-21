#include "addresses.h"
#include "dosbox.h"

#include "core/base/utils.h"

// Runs th01 on the dosbox backend as th01dosbox: GAME.BAT's commands with
// the original programs and the music driver checked.

int main(int argc, char **argv)
{
    static const DOSBOX_FILE files[] = {
#define M_FILE(program, name, sha1) { name, sha1 },
        RETAIL_PROGRAMS(M_FILE)
#undef M_FILE
            { "MDRV98.COM", RETAIL_MDRV98_SHA1 },
    };
    static const char *const commands[] = {
        "MDRV98 -M7",
        "ZUNSOFT",
        "OP",
        "MDRV98 -R",
    };
    const DOSBOX_PROGRAM program = {
        .files = files,
        .file_count = ARRAY_SIZE(files),
        .commands = commands,
        .command_count = ARRAY_SIZE(commands),
        .library = "th01dosbox.so",
    };
    return Dosbox_Launch(&program, argc, argv);
}
