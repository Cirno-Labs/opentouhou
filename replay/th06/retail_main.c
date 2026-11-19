#include "retail.h"

// Runs retail th06 1.02h on the retail backend as th06retail.exe.

int main(int argc, char **argv)
{
    const RETAIL_PROGRAM program = {
        .exe = "東方紅魔郷.exe",
        .exe_sha1 = "e61b4f4fea3802e926ef307f45166599c3e86555",
        .dll = "th06retail.dll",
    };
    return Retail_Launch(&program, argc, argv);
}
