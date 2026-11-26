#include "th02/menu.h"

#include "core/base/assert.h"

static const uint8_t m_Ranks[4][8] = {
    { 0xcf, 0xcf, 0xae, 0xaa, 0xbc, 0xc2, 0xcf, 0 },
    { 0xcf, 0xb6, 0xb8, 0xbb, 0xb7, 0xaa, 0xb5, 0 },
    { 0xcf, 0xcf, 0xb1, 0xaa, 0xbb, 0xad, 0xcf, 0 },
    { 0xb5, 0xbe, 0xb6, 0xaa, 0xbd, 0xb2, 0xac, 0 },
};

void Menu_Move(
    MENU_NAV *const nav, const int8_t maximum, const int8_t direction)
{
    ASSERT(maximum >= 0 && (direction == -1 || direction == 1));
    nav->selection = (int8_t)(nav->selection + direction);
    if (!nav->in_option && !nav->extra_unlocked && nav->selection == 1) {
        nav->selection = (int8_t)(nav->selection + direction);
    }
    if (nav->selection < 0) {
        nav->selection = maximum;
    }
    if (nav->selection > maximum) {
        nav->selection = 0;
    }
}

void Menu_Clear(TRAM_CONSOLE *const console, const uint8_t clear_attribute)
{
    if (console != nullptr) {
        TramConsole_Clear(console);
    } else {
        Tram_Fill(' ', clear_attribute);
    }
}

const uint8_t *Menu_DifficultyName(const int8_t difficulty)
{
    ASSERT(difficulty >= 0 && difficulty < 4);
    return m_Ranks[difficulty];
}
