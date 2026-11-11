#include "othpatch/main.h"

#include "core/io/path.h"

int main(const int argc, char **const argv)
{
    return Main_Run(argc, argv, Path_GetExeDir(), stdout);
}
