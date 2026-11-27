#include "replay/dosbox/test/port.h"

// Attaches dostestdosbox.so to the emulator.

[[gnu::visibility("default")]] const DOSBOX_GUEST *Dosbox_Attach(
    const DOSBOX_HOST *const host, const int argc, char **const argv)
{
    return Dosbox_Start(&g_DostestPort, host, argc, argv);
}
