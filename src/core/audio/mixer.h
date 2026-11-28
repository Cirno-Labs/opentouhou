#pragma once

#include "core/audio/types.h"
#include "core/base/result.h"

// Controls playback volume and mute per group and overall, read by the
// mixer callback on the audio thread.

RESULT Audio_SetMasterVolume(float volume);
void Audio_SetMuted(bool muted);
RESULT Audio_SetGroupVolume(AUDIO_GROUP group, float volume);

// Silences output while playback positions continue advancing.
void Audio_SetGroupMuted(AUDIO_GROUP group, bool muted);

// Suspends sample and stream positions for the group. Silences live PCM
// buses while their shared timeline continues advancing.
void Audio_SetGroupPaused(AUDIO_GROUP group, bool paused);
