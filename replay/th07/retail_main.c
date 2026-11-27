#include "replay/retail/retail.h"

// Runs retail th07 1.00b on the retail backend as th07retail.exe.

int main(int argc, char **argv)
{
    const RETAIL_PROGRAM program = {
        .exe = "th07.exe",
        .exe_sha1 = "80cbf99382c81ea0a9b7b85c7f9e059a75a52725",
        .dll = "th07retail.dll",
    };
    return Retail_Launch(&program, argc, argv);
}
