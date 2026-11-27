#include "th02/tram/console.h"

#include "th02/tram/cells.h"

#include <string.h>

void TramConsole_Init(TRAM_CONSOLE *const console, const uint8_t attribute)
{
    memset(console, 0, sizeof(*console));
    console->attribute = attribute;
    console->cursor_visible = true;
    console->system_line_visible = true;
}

void TramConsole_Clear(TRAM_CONSOLE *const console)
{
    Tram_Fill(' ', console->attribute);
    console->cursor_column = console->cursor_row = 0;
}
