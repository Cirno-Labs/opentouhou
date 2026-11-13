#pragma once

#include "core/base/result.h"

// Plays the events of th06/sound/events.h through core/audio.
//
// Sound effects come from the wave files in data/wav. Each entry of the
// sound table plays one of them at its own volume, in AUDIO_GROUP_EFFECTS.
// An entry queued again while it plays starts again from the beginning.
//
// Music streams in AUDIO_GROUP_MUSIC from the wave file that has the
// path of the play event with the extension "wav": "bgm/th06_01.mid"
// plays "bgm/th06_01.wav". The .pos file with the same name holds the loop
// as two 32-bit sample frame positions, start and end. The music plays
// up to the end, then repeats from the start. Without a .pos file, the
// whole file repeats. The original can also play its music as MIDI,
// which is not ported: a configuration that selects MIDI in the original
// plays these wave files instead.
//
// g_Settings.sound off plays no sound effects and starts no music.
// g_Settings.music off starts, stops and fades no music, as the original
// ignores its music calls in that mode.
//
// A fade lowers the volume a little each tick, linearly in decibels, by
// 50 dB over its length, and then stops the music.

// Loads the sound effects. Without an initialized core/audio, reports
// failure, and every other function does nothing but clear the events.
// When a sound effect file is missing or is not a wave file, reports
// failure and plays no sound effect at all, but the music still plays.
RESULT SoundPlayer_Init(void);

// Plays the events of the tick that ran, then clears them: steps the fade,
// applies the music change and the fade asked for, and plays the queued
// sound effects. Music starts once its first audio is ready.
void SoundPlayer_Update(void);

// Stops the music and releases the sound effects.
void SoundPlayer_Shutdown(void);
