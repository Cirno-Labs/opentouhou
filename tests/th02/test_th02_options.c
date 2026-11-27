#include "th02_menu.c"

#include "test.h"

typedef struct {
    OPTIONS_MENU_SOUND_EVENT events[16];
    size_t count;
} TRACE;

static void M_RecordSound(
    void *const context, const OPTIONS_MENU_SOUND_EVENT *const event)
{
    TRACE *const trace = context;
    if (trace->count < 16) {
        trace->events[trace->count++] = *event;
    }
}

static void M_Begin(
    TH02_SESSION *const session, TH02_SETTINGS *const settings,
    OPTIONS_MENU_SOUND *const sound)
{
    MainMenu_Begin(
        session, settings, nullptr, nullptr, sound, nullptr, false, 1);
    m_Menu.nav.in_option = true;
}

TEST(option_gate_requires_release_and_does_not_reset_idle)
{
    TH02_SESSION session = {};
    TH02_SETTINGS settings = { 1, 1, 3, 2, false, true };
    OPTIONS_MENU_SOUND sound = {};
    M_Begin(&session, &settings, &sound);
    m_Menu.idle_frame = 65535;
    OPTIONS_MENU_EVENTS events = OptionsMenu_Update(GAME_KEY_RIGHT);
    CHECK(events.wait_frame && !events.return_main);
    CHECK_EQ_INT(settings.difficulty, 1);
    CHECK_EQ_INT(m_Menu.idle_frame, 0);
    OptionsMenu_Update(GAME_KEY_CANCEL);
    CHECK(m_Menu.nav.in_option);
    OptionsMenu_Update(0);
    OptionsMenu_Update(GAME_KEY_RIGHT | GAME_KEY_LEFT);
    CHECK_EQ_INT(settings.difficulty, 1);
    CHECK_EQ_INT(m_Menu.idle_frame, 3);
    CHECK_EQ_INT(session.frame, 4);
    CHECK(!OptionsMenu_Sample().input && !m_Menu.input_allowed);
}

TEST(music_changes_preserve_probe_latch_and_both_direction_requests)
{
    TH02_SESSION session = {};
    TH02_SETTINGS settings = { 1, 1, 3, 2, false, true };
    TRACE trace = {};
    OPTIONS_MENU_SOUND sound = {
        .midi_possible = true,
        .fm_possible = true,
        .active = true,
        .emit = M_RecordSound,
        .context = &trace,
    };
    M_Begin(&session, &settings, &sound);
    OptionsMenu_Update(0);
    m_Menu.nav.selection = 1;
    OptionsMenu_Update(GAME_KEY_RIGHT | GAME_KEY_LEFT);
    CHECK_EQ_INT(settings.music, TH02_MUSIC_FM);
    CHECK_EQ_INT(trace.count, 6);
    for (size_t i = 0; i < trace.count; i++) {
        CHECK_EQ_INT(trace.events[i].action, i % 3);
    }
    CHECK(!trace.events[0].midi_active && trace.events[0].active);
    CHECK(trace.events[1].midi_active && trace.events[2].midi_active);
    CHECK(trace.events[3].midi_active && trace.events[3].active);
    CHECK(!trace.events[5].midi_active && !trace.events[5].active);
    CHECK(sound.fm_possible && !sound.active && !sound.midi_active);
    settings.music = TH02_MUSIC_MIDI;
    sound.active = sound.midi_active = true;
    trace.count = 0;
    OptionsMenu_Update(0);
    OptionsMenu_Update(GAME_KEY_RIGHT);
    CHECK_EQ_INT(settings.music, TH02_MUSIC_OFF);
    CHECK_EQ_INT(trace.count, 1);
    CHECK_EQ_INT(trace.events[0].action, OPTIONS_MENU_SOUND_STOP);
    CHECK(trace.events[0].midi_active && trace.events[0].active);
    CHECK(!trace.events[0].fm_possible);
    CHECK(sound.midi_active && !sound.active && !sound.fm_possible);
}

TEST(reset_restores_defaults_and_the_resident_reset_marker)
{
    TH02_SESSION session = { .score = 1234,
                             .reduce_effects = true,
                             .unused_2 = -7 };
    TH02_SETTINGS settings = { 3, 0, 0, 4, true, false };
    TRACE trace = {};
    OPTIONS_MENU_SOUND sound = {
        .fm_available = true,
        .midi_possible = true,
        .midi_active = true,
        .emit = M_RecordSound,
        .context = &trace,
    };
    M_Begin(&session, &settings, &sound);
    OptionsMenu_Update(0);
    m_Menu.nav.selection = 5;
    OptionsMenu_Update(GAME_KEY_SHOT);
    CHECK_EQ_INT(settings.difficulty, 1);
    CHECK_EQ_INT(settings.music, 1);
    CHECK_EQ_INT(settings.bombs, 3);
    CHECK_EQ_INT(settings.lives, 2);
    CHECK(!settings.fix_bomb_transition_overread);
    CHECK(!settings.reduce_effects && !session.reduce_effects);
    CHECK_EQ_INT(session.unused_2, 1);
    CHECK_EQ_INT(session.score, 1234);
    CHECK_EQ_INT(trace.count, 3);
    CHECK(trace.events[0].midi_active && !trace.events[0].active);
    CHECK(!trace.events[2].midi_active && trace.events[2].active);
    CHECK(sound.fm_possible && sound.active && !sound.midi_active);
}

TEST(cancel_returns_to_main_after_the_frame_and_requires_release_again)
{
    TH02_SESSION session = {};
    TH02_SETTINGS settings = { 1, 1, 3, 2, false, true };
    OPTIONS_MENU_SOUND sound = {};
    M_Begin(&session, &settings, &sound);
    OptionsMenu_Update(0);
    const OPTIONS_MENU_EVENTS events = OptionsMenu_Update(GAME_KEY_CANCEL);
    CHECK(events.wait_frame && events.return_main);
    CHECK(!m_Menu.nav.in_option && !m_Menu.quit && !OptionsMenu_Sample().ready);
    CHECK_EQ_INT(m_Menu.nav.selection, 3);
    CHECK_EQ_INT(session.frame, 2);
    CHECK(!OptionsMenu_Update(GAME_KEY_CANCEL).wait_frame);
    const MAIN_MENU_EVENTS back = MainMenu_Update(GAME_KEY_CANCEL);
    CHECK(back.wait_frame && !back.quit);
    CHECK_EQ_INT(m_Menu.idle_frame, 1);
    CHECK_EQ_INT(session.frame, 3);
}
