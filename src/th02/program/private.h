#pragma once

#include "th02/audio.h"
#include "th02/audio_clock.h"
#include "th02/bomb.h"
#include "th02/bomb/tail.h"
#include "th02/clip.h"
#include "th02/demo.h"
#include "th02/ending.h"
#include "th02/game_over.h"
#include "th02/gameplay.h"
#include "th02/grcg.h"
#include "th02/high_score_entry.h"
#include "th02/high_scores.h"
#include "th02/hud.h"
#include "th02/input.h"
#include "th02/items.h"
#include "th02/main.h"
#include "th02/miss.h"
#include "th02/music/assets.h"
#include "th02/music/wait.h"
#include "th02/opening.h"
#include "th02/options_menu.h"
#include "th02/palette.h"
#include "th02/performance.h"
#include "th02/player.h"
#include "th02/presenter.h"
#include "th02/score.h"
#include "th02/session.h"
#include "th02/sound_effects.h"
#include "th02/sparks.h"
#include "th02/stage.h"
#include "th02/stage_bonus.h"
#include "th02/stage_effects.h"
#include "th02/text_font.h"
#include "th02/tram.h"
#include "th02/video.h"
#include "th02/vsync_irq.h"

#include "core/audio.h"
#include "core/base/arena.h"
#include "pc98/pmd.h"
#include "pc98/pmd_data.h"

// Everything the program loads, in one arena.
typedef struct {
    ARENA arena;
    OPENING_ASSETS opening;
    GAMEPLAY_ASSETS gameplay;
    MUSIC_ASSETS music;
    ENDING_ASSETS ending;
    PATTERN_BANK stage_patterns[6];
    STAGE_ASSETS stages[6];
    BOMB_ASSETS bombs[3];
    uint8_t bomb_tail[BOMB_TAIL_SIZE];
    DEMO_RECORD demos[3];
} PROGRAM_ASSETS;

// The music driver, its clock and the sound options.
typedef struct {
    PMD *pmd;
    MUSIC_WAIT music_wait;
    bool music_loaded;
    GAME_AUDIO *game;
    OPTIONS_MENU_SOUND sound;
} PROGRAM_AUDIO;

// Text RAM, the gaiji in use and the font.
typedef struct {
    TRAM_DISPLAY text;
    TRAM_CONSOLE console;
    GAIJI gaiji;
    GAIJI saved_gaiji;
    bool font_saved;
    TEXT_FONT font;
} PROGRAM_DISPLAY;

// The current scene and the run it plays, with the pending saves.
typedef struct {
    TH02_SCENE scene;
    MAIN_MENU_ACTION request;
    TH02_SESSION session;
    HIGH_SCORES scores;
    bool save_scores;
    bool save_settings;
    HUD_WIPE stage_leave;
} PROGRAM_SCENES;

typedef struct {
    PROGRAM_ASSETS assets;
    PROGRAM_AUDIO audio;
    PROGRAM_DISPLAY display;
    PROGRAM_SCENES scenes;
    bool failed;
    VSYNC_CLOCK clock;
    bool initialized;
    bool live_clock_enabled;
    TH02_CLOCK_SAMPLE live_clocks[3];
    uint64_t host_origin;
} PROGRAM;

// Writable view of the program's state for its parts and tests.
extern PROGRAM *const p_Program;

// The calls scenes.c makes into the other screens and the shell, set by
// Program_InitHooks: the real functions by default, a fake observer in
// tests.
typedef struct {
    void (*ending_begin)(
        const ENDING_ASSETS *assets, const TH02_SESSION *session,
        STORY_AUDIO music, void *music_context, bool extra_unlocked);
    bool (*stage_bonus_update)(const TH02_INPUT *input);
    bool (*ending_update)(
        const TH02_INPUT *input, bool music_active, uint16_t measure);
    uint16_t (*pmd_measure)(const PMD *pmd);
    HIGH_SCORE_ENTRY_EVENTS (*high_score_entry_update)(const TH02_INPUT *input);
    void (*game_over_resume)(void);
    GAME_OVER_EVENT (*game_over_update)(uint16_t keys);
    OPENING_EVENTS (*opening_update)(
        bool reference_edge, uint16_t keys, bool skip_logo);
    void (*opening_begin_program)(void);
    RESULT (*high_scores_file_save)(VFS *user_files, const HIGH_SCORES *scores);
    VFS *(*shell_get_user_files)(void);
    void (*shell_request_save_settings)(void);
    void (*presenter_draw)(
        bool text_visible, const GAIJI *gaiji, const TRAM_FONT *font,
        uint16_t first_cell, bool blink_visible);
    void (*presenter_close)(void);
    TRAM_FONT (*text_font_tram)(const TEXT_FONT *font);
} PROGRAM_HOOKS;

extern const PROGRAM_HOOKS g_ProgramHooks;

// Swaps scenes.c's hooks; defaults to g_ProgramHooks, so only tests need
// to call this.
void Program_InitHooks(const PROGRAM_HOOKS *hooks);

// Begins or resumes score registration for the current run, entering
// its scene and running its first update.
void ProgramScenes_Registration(const TH02_INPUT *input, bool ending);

// Commits the run's score and enters the ending at the given stage.
void ProgramScenes_Ending(bool extra);

// Requests the pending settings save, as an OPTIONS_MENU_SOUND context
// callback.
void ProgramAudio_SaveSettings(void *context);

// Fills the options menu's sound state from the settings, with its
// sound callback.
void ProgramAudio_InitSound(OPTIONS_MENU_SOUND *sound);

// Returns the opening's title music, music room and settings save.
OPENING_AUDIO ProgramAudio_Opening(void);

// Returns the stage and boss music, with the entry wait unless in a demo.
GAMEPLAY_MUSIC ProgramAudio_Gameplay(bool demo);

// Requests a story song, as the ending's STORY_AUDIO.
void ProgramAudio_Story(void *context, STORY_MUSIC song);

// Fades the music out at speed.
void ProgramAudio_Fade(uint8_t speed);

// Mounts the game's archive into files.
RESULT ProgramAssets_Mount(VFS *files);

// Loads every asset the program keeps into its arena.
RESULT ProgramAssets_Load(VFS *files);

// Starts the program at the launcher logo.
RESULT Program_Init(void);

// Runs one tick; returns false once the program stops.
bool Program_Tick(const void *input);

// Saves what is pending and releases everything.
void Program_Shutdown(void);
