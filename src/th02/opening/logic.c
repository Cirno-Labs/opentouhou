#include "th02/opening/logic.h"

#include "th02/high_score_view.h"
#include "th02/shot_select.h"
#include "th02/wait.h"

#include "pc98/zun/bfnt.h"
#include "pc98/zun/logo1.h"

#include <string.h>

typedef enum {
    OPENING_LOGO,
    OPENING_TITLE,
    OPENING_MENU,
    OPENING_SUSPENDED,
    OPENING_DONE,
    OPENING_SCORES,
    OPENING_MUSIC,
    OPENING_START,
    OPENING_SHOT_FADE_IN,
    OPENING_SHOTS,
    OPENING_SHOT_FADE_OUT,
    OPENING_RETURN_SCORES,
} OPENING_PHASE;

typedef struct {
    ZUN_LOGO1 logo;
    WAIT wait;
    OPENING_PHASE phase;
    uint8_t logo_wait;
    bool quit_pending;
} OPENING_PROGRAM;

typedef struct {
    const OPENING_ASSETS *assets;
    TH02_SESSION *session;
    TH02_SETTINGS *settings;
    OPTIONS_MENU_SOUND *sound;
    TRAM_CONSOLE *console;
    OPENING_AUDIO audio;
    const BFNT *logo;
    const TITLE_ASSETS *title;
    const SHOT_SELECT_ASSETS *shots;
    const HIGH_SCORES *scores;
    uint8_t next_demo;
    uint8_t last_demo;
    uint8_t clear_attribute;
    HIGH_SCORE_CLEARS clears;
    OPENING_PROGRAM program;
} OPENING;

static OPENING m_Opening;

static bool M_WaitReady(void)
{
    if (!Wait_Ready(&m_Opening.program.wait, nullptr, 0)) {
        return false;
    }
    Wait_Cancel(&m_Opening.program.wait);
    return true;
}

static void M_Delay(const uint16_t frames)
{
    Wait_Delay(&m_Opening.program.wait, frames);
}

static bool M_Title(void)
{
    for (;;) {
        const TITLE_EVENTS events = Title_Step();
        m_Opening.last_demo = events.demo_num;
        if (m_Opening.audio.title != nullptr) {
            for (size_t i = 0; i < events.audio_count; i++) {
                m_Opening.audio.title(
                    m_Opening.audio.context, &events.audio[i]);
            }
        }
        if (events.wait == TITLE_WAIT_FRAMES) {
            M_Delay(events.count);
            return false;
        }
        if (events.wait == TITLE_WAIT_VSYNC) {
            Wait_Edge(&m_Opening.program.wait);
            return false;
        }
        if (events.wait == TITLE_COMPLETE) {
            return true;
        }
    }
}

static MAIN_MENU_EVENTS M_Scores(const uint16_t keys)
{
    const HIGH_SCORE_VIEW_EVENTS events = HighScoreView_Update(keys);
    if (events.done) {
        m_Opening.program.phase = OPENING_SUSPENDED;
        return Opening_Resume(0);
    }
    M_Delay(events.wait_frames);
    return (MAIN_MENU_EVENTS) {};
}

static MAIN_MENU_EVENTS M_ReturnScores(const uint16_t keys)
{
    const HIGH_SCORE_VIEW_EVENTS events = HighScoreView_Update(keys);
    if (events.done) {
        Video_ShowPage(0);
        Video_AccessPage(0);
        m_Opening.program.phase = OPENING_TITLE;
        M_Title();
    } else {
        M_Delay(events.wait_frames);
    }
    return (MAIN_MENU_EVENTS) {};
}

static MAIN_MENU_EVENTS M_Music(const uint16_t keys)
{
    const MUSIC_ROOM_EVENTS events = MusicRoom_Update(keys);
    if (events.done) {
        m_Opening.program.phase = OPENING_SUSPENDED;
        return Opening_Resume(0);
    }
    if (events.wait_frame) {
        M_Delay(1);
    }
    return (MAIN_MENU_EVENTS) {};
}

static void M_FadeWait(void)
{
    Wait_Edge(&m_Opening.program.wait);
}

static MAIN_MENU_EVENTS M_Shots(const uint16_t keys)
{
    for (;;) {
        const SHOT_SELECT_EVENTS events = ShotSelect_Update(keys);
        if (events.wait == SHOT_SELECT_FRAME) {
            M_Delay(1);
            return (MAIN_MENU_EVENTS) {};
        }
        if (events.wait == SHOT_SELECT_FADE_OUT) {
            ShotSelect_BeginFadeOut();
            m_Opening.program.phase = OPENING_SHOT_FADE_OUT;
            M_FadeWait();
            return (MAIN_MENU_EVENTS) {};
        }
        if (events.wait == SHOT_SELECT_COMPLETE) {
            m_Opening.program.phase = OPENING_SUSPENDED;
            return (MAIN_MENU_EVENTS) { .action = m_Opening.session->stage == 5
                                            ? MAIN_MENU_EXTRA
                                            : MAIN_MENU_START };
        }
    }
}

static MAIN_MENU_EVENTS M_ShotFade(const uint16_t keys)
{
    if (!ShotSelect_VSync()) {
        M_FadeWait();
        return (MAIN_MENU_EVENTS) {};
    }
    if (m_Opening.program.phase == OPENING_SHOT_FADE_OUT) {
        return M_Shots(keys);
    }
    ShotSelect_BeginInput();
    m_Opening.program.phase = OPENING_SHOTS;
    return M_Shots(keys);
}

static MAIN_MENU_EVENTS M_MenuEvents(
    MAIN_MENU_EVENTS events, const uint16_t keys)
{
    if ((events.action == MAIN_MENU_START || events.action == MAIN_MENU_EXTRA)
        && m_Opening.shots != nullptr) {
        const SESSION_EVENTS start =
            Session_BeginStart(events.action == MAIN_MENU_EXTRA);
        if (m_Opening.audio.title != nullptr) {
            const TITLE_AUDIO_REQUEST request = { TITLE_FORCE_EFFECT, false,
                                                  (uint8_t)start.force_effect };
            m_Opening.audio.title(m_Opening.audio.context, &request);
        }
        m_Opening.program.phase = OPENING_START;
        M_Delay(start.wait_frames);
        return (MAIN_MENU_EVENTS) {};
    }
    if (events.action == MAIN_MENU_SCORES && m_Opening.scores != nullptr) {
        m_Opening.program.phase = OPENING_SCORES;
        HighScoreView_Begin(
            m_Opening.scores, m_Opening.settings, &m_Opening.assets->scores,
            events.score_frames);
        return M_Scores(keys);
    } else if (
        events.action == MAIN_MENU_MUSIC
        && m_Opening.assets->music.background.view.pixels != nullptr) {
        m_Opening.program.phase = OPENING_MUSIC;
        MusicRoom_Begin(
            &m_Opening.assets->music, m_Opening.title,
            m_Opening.sound->midi_possible, m_Opening.sound->midi_active,
            m_Opening.audio.music, m_Opening.audio.context);
        return M_Music(keys);
    } else if (events.action == MAIN_MENU_OPTIONS) {
        // MainMenu_Update already drives the options menu itself; this
        // event is only a notice, not a request for main.c to dispatch.
        events.action = MAIN_MENU_NONE;
    } else if (events.action != MAIN_MENU_NONE) {
        m_Opening.program.phase = OPENING_SUSPENDED;
    }
    if (events.wait_frame) {
        M_Delay(1);
    }
    if (events.quit) {
        m_Opening.program.quit_pending = true;
        if (events.wait_frame) {
            events.quit = false;
        } else {
            m_Opening.program.phase = OPENING_DONE;
        }
    }
    return events;
}

static bool M_LogoTick(void)
{
    const bool running = ZunLogo1_Tick(
        &m_Opening.program.logo, g_Video.pages, m_Opening.logo->pixels);
    Video_AccessPage(m_Opening.program.logo.drawing_page);
    Video_ShowPage(m_Opening.program.logo.visible_page);
    return running;
}

static void M_LogoDraw(void)
{
    M_LogoTick();
    ZunLogo1_Palette(
        &m_Opening.program.logo, m_Opening.logo->palette, g_Video.palette);
    m_Opening.program.logo_wait = 2;
}

static void M_BeginProgram(void)
{
    if (m_Opening.last_demo == 0 && m_Opening.audio.title != nullptr) {
        const TITLE_AUDIO_REQUEST request = { .action = TITLE_STOP_SONG };
        m_Opening.audio.title(m_Opening.audio.context, &request);
    }
    memset(&m_Opening.clears, 0, sizeof(m_Opening.clears));
    if (m_Opening.scores != nullptr) {
        HighScores_GetClears(m_Opening.scores, &m_Opening.clears);
    }
    VSyncIrq_BeginProgram();
    Video_SetSplit(400, 0, 0);
    Video_ShowPage(0);
    Video_AccessPage(0);
    Title_Begin(
        m_Opening.title, m_Opening.last_demo, m_Opening.sound->midi_possible,
        m_Opening.sound->midi_active);
    m_Opening.program.phase = OPENING_TITLE;
}

static MAIN_MENU_EVENTS M_Update(
    const bool reference_edge, const uint16_t keys, const bool skip_logo)
{
    if (m_Opening.program.phase == OPENING_DONE) {
        return (MAIN_MENU_EVENTS) { .quit = true };
    }
    if (m_Opening.program.phase == OPENING_SUSPENDED) {
        return (MAIN_MENU_EVENTS) {};
    }
    if (m_Opening.program.phase == OPENING_LOGO) {
        if (!reference_edge || --m_Opening.program.logo_wait != 0) {
            return (MAIN_MENU_EVENTS) {};
        }
        const bool running = M_LogoTick();
        if (running && !skip_logo) {
            M_LogoDraw();
            return (MAIN_MENU_EVENTS) {};
        }
        M_BeginProgram();
    }
    if (!M_WaitReady()) {
        return (MAIN_MENU_EVENTS) {};
    }
    if (m_Opening.program.quit_pending) {
        m_Opening.program.phase = OPENING_DONE;
        return (MAIN_MENU_EVENTS) { .quit = true };
    }
    if (m_Opening.program.phase == OPENING_SCORES) {
        return M_Scores(keys);
    }
    if (m_Opening.program.phase == OPENING_RETURN_SCORES) {
        return M_ReturnScores(keys);
    }
    if (m_Opening.program.phase == OPENING_MUSIC) {
        return M_Music(keys);
    }
    if (m_Opening.program.phase == OPENING_START) {
        Session_FinishStart(m_Opening.session, m_Opening.settings);
        if (m_Opening.audio.save_settings != nullptr) {
            m_Opening.audio.save_settings(m_Opening.audio.context);
        }
        if (m_Opening.console != nullptr) {
            TramConsole_Clear(m_Opening.console);
        } else {
            Tram_Fill(' ', m_Opening.clear_attribute);
        }
        ShotSelect_Begin(
            m_Opening.session, m_Opening.shots,
            m_Opening.session->stage == 5 ? m_Opening.clears.extra
                                          : m_Opening.clears.game);
        m_Opening.program.phase = OPENING_SHOT_FADE_IN;
        M_FadeWait();
        return (MAIN_MENU_EVENTS) {};
    }
    if (m_Opening.program.phase == OPENING_SHOT_FADE_IN
        || m_Opening.program.phase == OPENING_SHOT_FADE_OUT) {
        return M_ShotFade(keys);
    }
    if (m_Opening.program.phase == OPENING_SHOTS) {
        return M_Shots(keys);
    }
    if (m_Opening.program.phase == OPENING_TITLE) {
        if (!M_Title()) {
            return (MAIN_MENU_EVENTS) {};
        }
        MainMenu_Begin(
            m_Opening.session, m_Opening.settings, m_Opening.title,
            m_Opening.assets->reduction, m_Opening.sound, m_Opening.console,
            m_Opening.clears.extra_unlocked, m_Opening.next_demo);
        m_Opening.last_demo = 0;
        m_Opening.program.phase = OPENING_MENU;
    }
    return M_MenuEvents(MainMenu_Update(keys), keys);
}

OPENING_SAMPLE Opening_Sample(void)
{
    return (OPENING_SAMPLE) {
        .menu = MainMenu_Sample(),
        .logo_frame = m_Opening.program.logo.frame,
        .logo = m_Opening.program.phase == OPENING_LOGO,
        .in_menu = m_Opening.program.phase == OPENING_MENU,
    };
}

void Opening_Begin(
    const OPENING_ASSETS *const assets, TH02_SESSION *const session,
    TH02_SETTINGS *const settings, const HIGH_SCORES *const scores,
    OPTIONS_MENU_SOUND *const sound, TRAM_CONSOLE *const console,
    const OPENING_AUDIO *const audio, const uint8_t next_demo)
{
    memset(&m_Opening, 0, sizeof(m_Opening));
    Title_Init();
    Session_CancelStart();
    MainMenu_Init();
    ShotSelect_Init();
    HighScoreView_Init();
    MusicRoom_Init();
    m_Opening.assets = assets;
    m_Opening.session = session;
    m_Opening.settings = settings;
    m_Opening.sound = sound;
    m_Opening.console = console;
    if (audio != nullptr) {
        m_Opening.audio = *audio;
    }
    m_Opening.logo = &assets->logo;
    m_Opening.title = &assets->title;
    m_Opening.shots = assets->shots.background.view.pixels != nullptr
        ? &assets->shots
        : nullptr;
    m_Opening.clear_attribute = console != nullptr ? console->attribute : 0xe1;
    m_Opening.next_demo = next_demo;
    m_Opening.scores = scores;
    ZunLogo1_Init(&m_Opening.program.logo, g_Video.pages);
    Video_AccessPage(m_Opening.program.logo.drawing_page);
    Video_ShowPage(m_Opening.program.logo.visible_page);
    M_LogoDraw();
}

void Opening_BeginProgram(void)
{
    memset(&m_Opening.program, 0, sizeof(m_Opening.program));
    Session_CancelStart();
    MainMenu_Init();
    ShotSelect_Init();
    HighScoreView_Init();
    MusicRoom_Reset();
    M_BeginProgram();
}

void Opening_BeginDemoReturn(const uint16_t keys)
{
    Opening_BeginProgram();
    if (m_Opening.scores != nullptr) {
        Palette_SetTone(100);
        HighScoreView_Begin(
            m_Opening.scores, m_Opening.settings, &m_Opening.assets->scores,
            350);
        m_Opening.program.phase = OPENING_RETURN_SCORES;
        M_ReturnScores(keys);
    }
}

void Opening_DemoStarted(const uint8_t demo)
{
    m_Opening.last_demo = demo;
    m_Opening.next_demo = (uint8_t)(demo % 3 + 1);
}

OPENING_EVENTS Opening_Update(
    const bool reference_edge, const uint16_t keys, const bool skip_logo)
{
    const MAIN_MENU_EVENTS menu = M_Update(reference_edge, keys, skip_logo);
    return (OPENING_EVENTS) {
        .menu = menu,
        .hide_text = m_Opening.program.phase == OPENING_LOGO,
    };
}

MAIN_MENU_EVENTS Opening_Resume(const uint16_t keys)
{
    if (m_Opening.program.phase != OPENING_SUSPENDED) {
        return (MAIN_MENU_EVENTS) {};
    }
    m_Opening.program.phase = OPENING_MENU;
    return M_MenuEvents(MainMenu_Resume(keys), keys);
}

void Opening_Shutdown(void)
{
    memset(&m_Opening, 0, sizeof(m_Opening));
    Title_Init();
    Session_CancelStart();
    MainMenu_Init();
    ShotSelect_Init();
    HighScoreView_Init();
    MusicRoom_Init();
}
