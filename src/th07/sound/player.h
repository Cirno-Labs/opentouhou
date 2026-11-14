#pragma once

#include "core/base/result.h"

// Plays the events of th07/sound/events.h through core/audio.
//
// Sound effects come from the wave files in data/wav. Each entry of the
// sound table plays one of them at its own volume, in AUDIO_GROUP_EFFECTS.
// An entry queued again while it plays starts again from the beginning.
// While g_Settings.sound is off, the queued sound effects stay queued and
// play once it is on again, as in the original.
//
// Music streams in AUDIO_GROUP_MUSIC from the tracks of thbgm.dat that
// th07/sound/bgm.h reads. The original carries out its music commands
// over several ticks; the port applies a music change in the update after
// the tick that asks for it, as nothing in the simulation depends on when
// music starts. The original can also play its music as MIDI or from
// tracks read into memory first; neither is ported. g_Settings.music off
// starts, stops, fades, pauses and resumes no music.
//
// A fade lowers the volume a little each tick, linearly in decibels, by
// 50 dB over its length, and then stops the music. It counts ticks, so it
// goes on while the music is paused.

// Loads the sound effects and the track list. Without an initialized
// core/audio, reports failure, and every other function does nothing but
// clear the events. When a sound effect file is missing or is not a wave
// file, plays no sound effect at all; when the track list cannot be read,
// plays no music. Either reports failure.
RESULT SoundPlayer_Init(void);

// Plays the events of the tick that ran and clears them: steps the fade,
// applies the music change, the fade and the pause asked for, and plays
// the queued sound effects. Music starts once its first audio is ready.
void SoundPlayer_Update(void);

// Stops the music and releases the sound effects and the track list.
void SoundPlayer_Shutdown(void);
