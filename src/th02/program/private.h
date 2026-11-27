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
