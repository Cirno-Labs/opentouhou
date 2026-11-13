// SPDX-License-Identifier: GPL-3.0-only

#include "hotkeys.h"

static const INPUT_ACTION m_Actions[] = {
    INPUT_BUTTON(HOTKEYS, screenshot, "print_screen, f12", nullptr, nullptr),
    INPUT_BUTTON(HOTKEYS, fullscreen, "f11, alt+return", nullptr, nullptr),
};

const INPUT_SCHEMA g_HotkeysSchema = {
    .actions = m_Actions,
    .count = ARRAY_SIZE(m_Actions),
    .size = sizeof(HOTKEYS),
    .players = 1,
};
