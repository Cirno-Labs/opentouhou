#pragma once

#include "th02/music_room/render.h"

typedef enum {
    MUSIC_ROOM_LOAD,
    MUSIC_ROOM_PLAY,
} MUSIC_ROOM_AUDIO_ACTION;

typedef struct {
    MUSIC_ROOM_AUDIO_ACTION action;
    uint8_t track;
    bool midi;
} MUSIC_ROOM_AUDIO_REQUEST;

// Requests playback in order, without waiting; null requests nothing.
typedef void (*MUSIC_ROOM_AUDIO)(
    void *context, const MUSIC_ROOM_AUDIO_REQUEST *request);

typedef struct {
    bool wait_frame;
    bool done;
} MUSIC_ROOM_EVENTS;

// Resets the track, renderer and animation, once at startup.
void MusicRoom_Init(void);

// Resets the room's input state between visits, keeping the playing
// track.
void MusicRoom_Reset(void);

// Enters the room, clearing the input state and keeping the playing
// track and the renderer's animation.
void MusicRoom_Begin(
    const MUSIC_ROOM_ASSETS *assets, const TITLE_ASSETS *menu,
    bool midi_possible, bool midi_active, MUSIC_ROOM_AUDIO audio,
    void *audio_context);

// Runs one frame for keys, which hold through the comments and the exit
// release, as in retail.
MUSIC_ROOM_EVENTS MusicRoom_Update(uint16_t keys);
