// Derives the driver from Regulus's MDRV2 3.4f documentation, compiled author
// examples and an analysis of TH01's driver, with command sizes from
// HertzDevil's MDT decoder. Contains no driver source or machine code.
// References: https://www.vector.co.jp/soft/dos/art/se018677.html and
// https://gist.github.com/HertzDevil/036304c692b0f26b7a9d7cfe1126a0ac

#include "pc98/mdrv.h"

#include "core/base/log.h"
#include "pc98/opna.h"

#include <string.h>

#define M_COMMAND_BUDGET 4096
#define M_NO_POSITION UINT32_MAX

#define M_FATAL_IF(cond_, ...)                                                 \
    do {                                                                       \
        if (cond_) {                                                           \
            LOG_FATAL(__VA_ARGS__);                                            \
        }                                                                      \
    } while (0)

typedef struct {
    uint8_t mask, algorithm;
    uint8_t dt_mul[4], tl[4], ks_ar[4], am_dr[4], sr[4], sl_rr[4];
} M_FM_PROGRAM;

typedef struct {
    uint8_t initial, attack, decay, sustain, sustain_rate, release;
} M_SSG_PROGRAM;

typedef struct {
    const MDRV_MUSIC *asset;
    M_FM_PROGRAM *fm;
    M_SSG_PROGRAM *ssg;
} M_SONG;

typedef struct {
    uint8_t speed, depth, step, delay, phase, depth_work, delay_work;
    bool enabled, ramp;
} M_LFO;

typedef struct {
    uint16_t pitch, portamento;
    uint8_t volume, attenuation, program, pan, hardware_lfo;
    uint8_t algorithm, mask, tl[4], octave, noise_mode, previous_volume;
    uint8_t envelope_step, envelope_wait, envelope_target, envelope_level;
    uint8_t envelope_stage;
    int8_t detune, transpose;
    bool envelope, portamento_enabled;
    M_LFO pitch_lfo, volume_lfo;
} M_VOICE;

// Holds one track's parser state. channel is the physical slot: FM 80..85,
// PCM 20, rhythm 10 and SSG 40..42. suppressed masks the ordinary output of
// a music SSG track while an effect holds its channel.
typedef struct {
    uint32_t position, remaining, gate_remaining;
    uint32_t return_position, repeat_one_start, repeat_one_end,
        repeat_two_start;
    uint8_t channel, gate, repeat_one_count, repeat_two_count;
    bool active, key_on, legato, repeat_one, repeat_two, suppressed, effect;
} M_TRACK;

// Plays music when music is set and effects otherwise, on a private copy of
// the command bytes that F6/F7 counters modify.
typedef struct {
    const MDRV_MUSIC *music;
    M_TRACK *tracks;
    uint8_t *work;
    size_t work_size;
    uint32_t commands_start, commands_end;
    uint16_t track_count, capacity;
    uint8_t effect_counter;
    bool playing;
} M_SEQUENCE;

struct MDRV {
    OPNA *chip;
    M_SONG *songs;
    size_t song_count, song;
    const MDRV_EFFECTS *effects;
    M_SSG_PROGRAM *effect_programs;
    M_SEQUENCE music_sequence, effect_sequence;
    M_VOICE music[10], effect[3];
    M_SSG_PROGRAM patterns[6];
    uint8_t registers[512], rhythm[6], music_mixer, music_noise;
    uint8_t fm_balance, rhythm_balance;
    uint8_t timer_b, fade_interval, fade_counter, fade_direction, fade_amount;
    bool fading;
};

static const uint8_t m_ChannelOrder[] = { 0x80, 0x20, 0x10, 0x40 };
static const uint16_t m_FmPitch[12] = {
    0x26a, 0x28f, 0x2b6, 0x2df, 0x30b, 0x339,
    0x36a, 0x39e, 0x3d5, 0x410, 0x44e, 0x48f,
};
static const uint16_t m_SsgPitch[12] = {
    0xee8, 0xe12, 0xd48, 0xc89, 0xbd5, 0xb2b,
    0xa8a, 0x9f3, 0x964, 0x8dd, 0x85e, 0x7e6,
};
static const M_SSG_PROGRAM m_DefaultEnvelope = {
    .initial = 255,
    .attack = 255,
    .decay = 255,
    .sustain = 210,
    .sustain_rate = 3,
    .release = 1,
};

// Chip output

static void M_Write(MDRV *const mdrv, const uint16_t reg, const uint8_t value)
{
    mdrv->registers[reg] = value;
    Opna_Write(mdrv->chip, reg, value);
}

static void M_Output(
    MDRV *const mdrv, const M_TRACK *const track, const uint8_t reg,
    const uint8_t value, const bool raw)
{
    if (!raw && track->suppressed) {
        return;
    }
    const uint16_t bank = (track->channel >= 0x83 && reg >= 0x30) ? 0x100 : 0;
    M_Write(mdrv, bank + reg, !raw && reg == 7 ? (value & 0x3f) | 0x80 : value);
}

static void M_Silence(MDRV *const mdrv)
{
    for (int i = 6; i >= 0; i--) {
        M_Write(mdrv, 0x28, i);
    }
    M_Write(mdrv, 0x10, 0xff);
    M_Write(mdrv, 7, 0xb8);
    for (uint16_t reg = 8; reg <= 10; reg++) {
        M_Write(mdrv, reg, 0);
    }
    M_Write(mdrv, 0x10b, 0);
    M_Write(mdrv, 0x100, 1);
    M_Write(mdrv, 0x101, 0xc0);
    M_Write(mdrv, 0x10c, 0xff);
    M_Write(mdrv, 0x10d, 0xff);
}

static void M_Setup(MDRV *const mdrv)
{
    mdrv->music_mixer = 0xb8;
    mdrv->music_noise = mdrv->registers[6];
    M_Write(mdrv, 7, 0xb8);
    for (int bank = 0; bank < 2; bank++) {
        for (int i = 0; i < 3; i++) {
            M_Write(mdrv, bank * 256 + 0xb4 + i, 0xc0);
        }
        if (bank == 0) {
            for (int i = 0; i < 6; i++) {
                M_Write(mdrv, 0x18 + i, 0xc0);
                mdrv->rhythm[i] = 0xc0;
            }
        }
    }
    M_Write(mdrv, 0x22, 0);
    M_Write(mdrv, 0x12, 0);
}

// Voices

static uint8_t M_Channel(const M_TRACK *const track)
{
    if (track->channel == 0x10) {
        // Retain the IRQ index after the PCM slot.
        return 4;
    }
    return (track->channel & 0x80) != 0 ? (track->channel & 7) % 3
                                        : track->channel & 3;
}

static uint8_t M_EffectSlot(const M_TRACK *const track)
{
    return (track->channel & 3) % 3;
}

static M_VOICE *M_Voice(MDRV *const mdrv, const M_TRACK *const track)
{
    if (track->effect) {
        return &mdrv->effect[M_EffectSlot(track)];
    }
    return &mdrv->music
                [(track->channel & 0x80) != 0       ? track->channel & 7
                     : (track->channel & 0x40) != 0 ? 7 + M_Channel(track)
                                                    : 6];
}

static M_SSG_PROGRAM *M_Pattern(MDRV *const mdrv, const M_TRACK *const track)
{
    return &mdrv->patterns
                [track->effect ? 3 + M_EffectSlot(track) : M_Channel(track)];
}

static void M_ResetVoice(M_VOICE *const voice)
{
    voice->attenuation = 0;
    voice->previous_volume = 0;
    voice->hardware_lfo = 0;
    voice->detune = 0;
    voice->transpose = 0;
    voice->pan = 0xc0;
    memset(voice->tl, 0, sizeof(voice->tl));
    voice->envelope_step = 0;
    voice->envelope_wait = 0;
    voice->envelope_target = 0;
    voice->envelope_level = 0;
    voice->envelope = false;
    voice->envelope_stage = 0;
    voice->portamento_enabled = false;
    voice->pitch_lfo.enabled = false;
    voice->pitch_lfo.ramp = false;
    voice->volume_lfo.enabled = false;
    voice->volume_lfo.ramp = false;
}

static void M_ResetEffectTrack(M_TRACK *const track, const uint8_t channel)
{
    *track = (M_TRACK) {
        .channel = channel,
        .position = M_NO_POSITION,
        .return_position = M_NO_POSITION,
        .repeat_one_end = M_NO_POSITION,
        .gate = 8,
    };
}

// Resets the controls of every voice and the effect tracks, and ends a fade.
// Effect voices inherit the music SSG voices.
static void M_ResetControls(MDRV *const mdrv)
{
    for (size_t i = 0; i < 10; i++) {
        M_ResetVoice(&mdrv->music[i]);
    }
    for (size_t i = 0; i < 3; i++) {
        mdrv->effect[i] = mdrv->music[7 + i];
        M_ResetEffectTrack(&mdrv->effect_sequence.tracks[2 - i], 0x40 + i);
    }
    mdrv->fading = false;
    mdrv->fade_amount = 0;
}

static uint8_t M_Tl(
    const MDRV *const mdrv, const M_VOICE *const voice, const uint8_t base)
{
    const uint8_t balance = mdrv->fm_balance;
    const uint8_t effective =
        voice->volume < balance ? 0 : voice->volume - balance;
    const uint8_t tl = base + (uint8_t)(127 - effective) + voice->attenuation;
    return tl >= 128 ? 127 : tl;
}

static void M_Volume(
    MDRV *const mdrv, const M_TRACK *const track, const M_VOICE *const voice)
{
    const uint8_t channel = M_Channel(track);
    if ((track->channel & 0x80) != 0) {
        const uint8_t algorithm = voice->algorithm & 7;
        M_Output(
            mdrv, track, 0x4c + channel, M_Tl(mdrv, voice, voice->tl[3]),
            false);
        if (algorithm >= 4) {
            M_Output(
                mdrv, track, 0x48 + channel, M_Tl(mdrv, voice, voice->tl[2]),
                false);
        }
        if (algorithm >= 5) {
            M_Output(
                mdrv, track, 0x44 + channel, M_Tl(mdrv, voice, voice->tl[1]),
                false);
        }
        if (algorithm == 7) {
            M_Output(
                mdrv, track, 0x40 + channel, M_Tl(mdrv, voice, voice->tl[0]),
                false);
        }
    } else if (track->channel == 0x10) {
        const uint8_t attenuation = voice->attenuation >> 1;
        uint8_t value = 0;
        if (voice->volume >= attenuation) {
            const uint8_t level = (voice->volume - attenuation) & 0x3f;
            const uint8_t balance = mdrv->rhythm_balance;
            value = (level < balance ? 0 : level - balance) + 1;
        }
        M_Output(mdrv, track, 0x11, value, false);
    } else if ((voice->program & 0xc0) != 0) {
        const uint8_t attenuation = voice->attenuation >> 3;
        M_Output(
            mdrv, track, 8 + channel,
            voice->previous_volume < attenuation
                ? 0
                : voice->previous_volume - attenuation,
            false);
    }
}

// Writes the pitch of an FM voice, a rhythm portamento or an SSG voice.
static void M_Pitch(
    MDRV *const mdrv, const M_TRACK *const track, const M_VOICE *const voice)
{
    const uint8_t channel = M_Channel(track);
    if ((track->channel & 0x80) != 0 || track->channel == 0x10) {
        M_Output(mdrv, track, 0xa4 + channel, voice->pitch >> 8, false);
        M_Output(mdrv, track, 0xa0 + channel, voice->pitch, false);
    } else if ((voice->program & 0xc0) != 0) {
        const uint16_t pitch =
            voice->octave >= 16 ? 0 : voice->pitch >> voice->octave;
        M_Output(mdrv, track, channel * 2 + 1, pitch >> 8, false);
        M_Output(mdrv, track, channel * 2, pitch, false);
    }
}

static void M_EnvelopeStage(
    M_VOICE *const voice, const M_SSG_PROGRAM *const pattern,
    const uint8_t stage)
{
    voice->envelope_stage = stage;
    voice->envelope_wait = 1;
    voice->envelope_step = stage == 0 ? pattern->attack
        : stage == 1                  ? pattern->decay
        : stage == 2                  ? pattern->sustain_rate
                                      : pattern->release;
    voice->envelope_target = stage == 0 ? 255
        : stage == 1                    ? pattern->sustain
                                        : 0;
}

static void M_Envelope(
    MDRV *const mdrv, const M_TRACK *const track, M_VOICE *const voice)
{
    if (!voice->envelope || (voice->program & 0xc0) == 0) {
        return;
    }
    if (--voice->envelope_wait == 0) {
        voice->envelope_wait = 1;
        int level = voice->envelope_level
            + (voice->envelope_stage == 0 ? voice->envelope_step
                                          : -(int)voice->envelope_step);
        const bool reached = voice->envelope_stage == 0
            ? level >= voice->envelope_target
            : level <= voice->envelope_target;
        if (reached) {
            level = voice->envelope_target;
        }
        voice->envelope_level = level;
        const uint8_t volume =
            (uint16_t)(level * ((uint16_t)voice->volume + 1)) >> 8;
        if (volume != voice->previous_volume) {
            voice->previous_volume = volume;
            M_Volume(mdrv, track, voice);
        }
        if (reached) {
            if (level == 0 || voice->envelope_stage >= 2) {
                voice->envelope = false;
                voice->envelope_level = 0;
            } else {
                M_EnvelopeStage(
                    voice, M_Pattern(mdrv, track), voice->envelope_stage + 1);
            }
        }
    }
}

static uint8_t M_HalfDepth(const uint8_t depth)
{
    return ((depth >> 1) | (depth & 0x80)) & 0xbf;
}

static void M_Lfo(M_VOICE *const voice, M_LFO *const lfo, const bool volume)
{
    if (lfo->delay_work != 0) {
        if (--lfo->delay_work != 0) {
            return;
        }
        lfo->depth_work = M_HalfDepth(lfo->depth);
        lfo->phase = 0;
        if (volume) {
            voice->attenuation =
                lfo->ramp && (lfo->depth_work & 0x80) == 0 ? 127 : 0;
        }
    }
    const unsigned phase = lfo->phase + lfo->speed;
    lfo->phase = phase;
    if (phase < 256) {
        return;
    }
    const bool down = (lfo->depth_work & 0x80) != 0;
    if (volume) {
        voice->attenuation += down ? lfo->step : -(int)lfo->step;
    } else {
        voice->pitch += down ? -(int)lfo->step : lfo->step;
    }
    if (!lfo->ramp) {
        lfo->depth_work--;
        if ((lfo->depth_work & 0x7f) == 0) {
            lfo->depth_work =
                (uint8_t)((lfo->depth & 0x7f) + lfo->depth_work) ^ 0x80;
        }
    }
}

static void M_Tail(
    MDRV *const mdrv, const M_TRACK *const track, M_VOICE *const voice)
{
    if ((track->channel & 0x80) != 0) {
        voice->pitch += voice->portamento;
        M_Pitch(mdrv, track, voice);
    } else if ((track->channel & 0x40) != 0) {
        if ((voice->program & 0xc0) != 0) {
            if (voice->portamento_enabled) {
                voice->pitch += voice->portamento;
            }
            M_Pitch(mdrv, track, voice);
        }
        M_Envelope(mdrv, track, voice);
    }
}

// Runs a voice's periodic work, or only its envelope for an ended music SSG
// track.
static void M_Clock(MDRV *const mdrv, const M_TRACK *const track)
{
    M_VOICE *const voice = M_Voice(mdrv, track);
    if (!track->active) {
        M_Envelope(mdrv, track, voice);
        return;
    }
    if (mdrv->fading) {
        voice->attenuation = mdrv->fade_amount;
        M_Volume(mdrv, track, voice);
    } else {
        if (voice->pitch_lfo.enabled && track->channel != 0x10) {
            M_Lfo(voice, &voice->pitch_lfo, false);
        }
        if (voice->volume_lfo.enabled) {
            M_Lfo(voice, &voice->volume_lfo, true);
            M_Volume(mdrv, track, voice);
        }
    }
    M_Tail(mdrv, track, voice);
}

static uint8_t M_Pan(const uint8_t value)
{
    const uint8_t pan = value & 3;
    return (pan == 1 ? 2 : pan == 2 ? 1 : pan) << 6;
}

static void M_Mixer(
    MDRV *const mdrv, const M_TRACK *const track, const M_VOICE *const voice)
{
    const unsigned channel = M_Channel(track);
    const uint8_t mask = (1 << channel) | (8 << channel);
    const uint8_t mode = voice->noise_mode;
    const uint8_t bits = ((mode & 0x40) != 0 ? 1 << channel : 0)
        | ((mode & 0x80) != 0 ? 8 << channel : 0);
    if (!track->suppressed) {
        M_Output(mdrv, track, 7, (mdrv->registers[7] & ~mask) | bits, false);
        if (track->effect) {
            return;
        }
    }
    mdrv->music_mixer = (mdrv->music_mixer & ~mask) | bits;
}

static void M_Program(
    MDRV *const mdrv, const M_TRACK *const track, const uint8_t encoded)
{
    M_VOICE *const voice = M_Voice(mdrv, track);
    const size_t number = encoded & 0x3f;
    voice->program = encoded;
    if ((track->channel & 0x80) != 0) {
        const M_SONG *const song = &mdrv->songs[mdrv->song];
        M_FATAL_IF(
            number >= song->asset->fm_count, "MDRV FM program outside bank");
        const M_FM_PROGRAM *const program = &song->fm[number];
        const uint8_t channel = M_Channel(track);
        for (uint8_t i = 0; i < 4; i++) {
            M_Output(mdrv, track, 0x80 + i * 4 + channel, 0xff, false);
        }
        voice->mask = program->mask;
        voice->algorithm = program->algorithm;
        memcpy(voice->tl, program->tl, 4);
        for (uint8_t i = 0; i < 4; i++) {
            M_Output(
                mdrv, track, 0x30 + i * 4 + channel, program->dt_mul[i], false);
        }
        M_Volume(mdrv, track, voice);
        const uint8_t algorithm = program->algorithm & 7;
        for (int i = 2; i >= 0; i--) {
            if ((i == 2 && algorithm < 4) || (i == 1 && algorithm < 5)
                || (i == 0 && algorithm < 7)) {
                M_Output(
                    mdrv, track, 0x40 + i * 4 + channel, program->tl[i], false);
            }
        }
        const uint8_t *const rows[] = {
            program->ks_ar,
            program->am_dr,
            program->sr,
            program->sl_rr,
        };
        for (uint8_t row = 0; row < 4; row++) {
            for (uint8_t i = 0; i < 4; i++) {
                M_Output(
                    mdrv, track, 0x50 + row * 16 + i * 4 + channel,
                    rows[row][i], false);
            }
        }
        M_Output(mdrv, track, 0xb0 + channel, voice->algorithm, false);
        M_Output(
            mdrv, track, 0xb4 + channel, (voice->pan & 0x3f) | 0xc0, false);
    } else if ((track->channel & 0x40) != 0) {
        voice->program ^= 0xc0;
        M_FATAL_IF(
            (voice->program & 0xc0) == 0,
            "MDRV external SSG PCM is unsupported");
        const size_t count = track->effect
            ? mdrv->effects->ssg_count
            : mdrv->songs[mdrv->song].asset->ssg_count;
        M_FATAL_IF(number >= count, "MDRV SSG program outside bank");
        voice->envelope_stage = 0;
        *M_Pattern(mdrv, track) = track->effect
            ? mdrv->effect_programs[number]
            : mdrv->songs[mdrv->song].ssg[number];
        voice->noise_mode = (voice->noise_mode & 0x3f) | (encoded & 0xc0);
        M_Mixer(mdrv, track, voice);
    }
}

static void M_NotePitch(
    MDRV *const mdrv, const M_TRACK *const track, M_VOICE *const voice,
    const uint8_t note, const bool portamento)
{
    int key = note & 15;
    uint8_t octave = note >> 4;
    if (!portamento || (track->channel & 0x40) != 0) {
        key += voice->transpose;
        if (key < 0) {
            key += 12;
            octave--;
        } else if (key >= 12) {
            key -= 12;
            octave++;
        }
    }
    M_FATAL_IF(key < 0 || key >= 12, "MDRV transposed pitch outside table");
    if ((track->channel & 0x80) != 0
        || (portamento && track->channel == 0x10)) {
        voice->pitch =
            (uint16_t)(m_FmPitch[key] + ((octave & 15) << 11) + voice->detune);
    } else if ((track->channel & 0x40) != 0) {
        M_FATAL_IF(
            (voice->program & 0xc0) == 0,
            "MDRV external SSG PCM is unsupported");
        voice->pitch = m_SsgPitch[key] - voice->detune;
        voice->octave = octave;
    }
    M_Pitch(mdrv, track, voice);
}

// Starts a note, or a portamento (F2) with its pitch and signed delta in
// params.
static void M_StartNote(
    MDRV *const mdrv, const M_TRACK *const track, const uint8_t opcode,
    const uint8_t *const params, const bool legato)
{
    M_VOICE *const voice = M_Voice(mdrv, track);
    const uint8_t channel = M_Channel(track);
    const bool portamento = opcode == 0xf2;
    const uint8_t note = portamento ? params[0] : opcode;
    if (track->channel != 0x10 || portamento) {
        M_NotePitch(mdrv, track, voice, note, portamento);
    }
    if (!legato) {
        if ((track->channel & 0x80) != 0
            || (portamento && track->channel == 0x10)) {
            M_Output(
                mdrv, track, 0x28,
                voice->mask | channel | (track->channel >= 0x83 ? 4 : 0),
                false);
            if ((voice->hardware_lfo & 8) != 0) {
                M_Output(mdrv, track, 0x22, voice->hardware_lfo & ~8, false);
                M_Output(mdrv, track, 0x22, voice->hardware_lfo | 8, false);
            }
        } else if ((track->channel & 0x40) != 0) {
            const M_SSG_PROGRAM *const pattern = M_Pattern(mdrv, track);
            voice->envelope = true;
            voice->envelope_level = pattern->initial;
            M_EnvelopeStage(voice, pattern, 0);
        } else {
            M_Output(mdrv, track, 0x10, voice->program & 0x3f, false);
        }
    }
    voice->pitch_lfo.delay_work = voice->pitch_lfo.delay;
    voice->volume_lfo.delay_work = voice->volume_lfo.delay;
    voice->portamento = portamento ? params[2] | ((uint16_t)params[3] << 8) : 0;
    voice->portamento_enabled = portamento;
    if (portamento && (track->channel & 0x40) == 0
        && (mdrv->songs[mdrv->song].asset->hardware & 0xff) == 0) {
        const int32_t delta = voice->portamento >= 0x8000
            ? (int32_t)voice->portamento - 65536
            : voice->portamento;
        voice->portamento = delta >= 0 ? delta / 4 : -((-delta + 3) / 4);
    }
    M_Tail(mdrv, track, voice);
}

static void M_Rest(MDRV *const mdrv, const M_TRACK *const track)
{
    M_VOICE *const voice = M_Voice(mdrv, track);
    voice->portamento = 0;
    voice->portamento_enabled = false;
    M_Tail(mdrv, track, voice);
}

static void M_VoiceOff(MDRV *const mdrv, const M_TRACK *const track)
{
    M_VOICE *const voice = M_Voice(mdrv, track);
    if ((track->channel & 0x80) != 0) {
        const uint8_t channel =
            M_Channel(track) | (track->channel >= 0x83 ? 4 : 0);
        M_Output(mdrv, track, 0x28, channel, false);
    } else if ((track->channel & 0x40) != 0) {
        if ((voice->program & 0xc0) != 0) {
            M_EnvelopeStage(voice, M_Pattern(mdrv, track), 3);
        }
    } else {
        M_Output(mdrv, track, 0x10, (voice->program & 0x3f) | 0x80, false);
    }
}

static void M_SetVolume(
    MDRV *const mdrv, const M_TRACK *const track, const uint8_t opcode,
    const uint8_t *const p)
{
    M_VOICE *const voice = M_Voice(mdrv, track);
    if (track->channel == 0x10 && opcode == 0xec) {
        if ((p[0] & 0x80) == 0) {
            voice->volume = p[0];
            M_Volume(mdrv, track, voice);
            for (uint8_t i = 0; i < 6; i++) {
                mdrv->rhythm[i] = (mdrv->rhythm[i] & 0xc0) | p[i + 1];
                M_Output(mdrv, track, 0x18 + i, mdrv->rhythm[i], false);
            }
        } else {
            const uint8_t index = p[0] & 0x7f;
            M_FATAL_IF(index >= 6, "MDRV rhythm level outside bank");
            mdrv->rhythm[index] = (mdrv->rhythm[index] & 0xc0) | p[1];
            M_Output(mdrv, track, 0x18 + index, mdrv->rhythm[index], false);
        }
    } else {
        voice->volume = opcode == 0xf4 ? voice->volume + p[0]
            : opcode == 0xf5           ? voice->volume - p[0]
                                       : p[0];
        if ((track->channel & 0x40) == 0) {
            M_Volume(mdrv, track, voice);
        }
    }
}

// Sets a pitch LFO (ED, FB ramp) or volume LFO (E8, FC ramp).
static void M_SetLfo(
    MDRV *const mdrv, const M_TRACK *const track, const uint8_t opcode,
    const uint8_t *const p)
{
    M_VOICE *const voice = M_Voice(mdrv, track);
    const bool volume = opcode == 0xe8 || opcode == 0xfc;
    M_LFO *const lfo = volume ? &voice->volume_lfo : &voice->pitch_lfo;
    if (p[0] == 0) {
        lfo->enabled = false;
        if (volume) {
            voice->attenuation = 0;
        }
        return;
    }
    lfo->speed = p[0];
    lfo->depth = p[1];
    lfo->ramp = opcode == 0xfb || opcode == 0xfc;
    if (lfo->ramp) {
        lfo->depth = (p[1] >> 1) | (p[1] << 7);
        if (volume) {
            lfo->depth_work = lfo->depth;
        }
    }
    lfo->step = p[2];
    if (!volume && (track->channel & 0x40) == 0
        && (mdrv->songs[mdrv->song].asset->hardware & 0xff) == 0) {
        lfo->step >>= 2;
    }
    lfo->delay = p[3] + 1;
    if (volume && lfo->ramp && lfo->delay == 0) {
        voice->attenuation = (lfo->depth & 0x80) != 0 ? 0 : 127;
        lfo->phase = 0;
    }
    lfo->enabled = true;
}

static void M_SetHardwareLfo(
    MDRV *const mdrv, const M_TRACK *const track, const uint8_t *const p)
{
    M_VOICE *const voice = M_Voice(mdrv, track);
    voice->hardware_lfo = p[0];
    if ((track->channel & 0x40) != 0) {
        voice->previous_volume = p[0];
    }
    M_Output(mdrv, track, 0x22, p[0], true);
    if (p[0] != 0) {
        voice->hardware_lfo |= (uint8_t)(p[1] << 3);
        voice->pan = (voice->pan & 0xc0) | p[2] | (uint8_t)(p[3] << 4);
        M_Output(mdrv, track, 0xb4 + M_Channel(track), voice->pan, true);
    }
}

static void M_SetDelayNoise(
    MDRV *const mdrv, const M_TRACK *const track, const uint8_t value)
{
    M_VOICE *const voice = M_Voice(mdrv, track);
    if ((track->channel & 0x40) != 0) {
        voice->noise_mode = (voice->noise_mode & 0xc0) | value;
        M_Output(mdrv, track, 6, value, false);
        if (!track->effect) {
            mdrv->music_noise = value;
        }
    } else {
        voice->pitch_lfo.delay = value + 1;
    }
}

static void M_SetPan(
    MDRV *const mdrv, const M_TRACK *const track, const uint8_t *const p)
{
    M_VOICE *const voice = M_Voice(mdrv, track);
    if (track->channel == 0x10) {
        M_FATAL_IF(p[0] >= 6, "MDRV rhythm pan outside bank");
        mdrv->rhythm[p[0]] = (mdrv->rhythm[p[0]] & 0x3f) | M_Pan(p[1]);
        M_Output(mdrv, track, 0x18 + p[0], mdrv->rhythm[p[0]], false);
    } else if ((track->channel & 0x80) != 0) {
        voice->pan = (voice->pan & 0x3f) | M_Pan(p[0]);
        M_Output(mdrv, track, 0xb4 + M_Channel(track), voice->pan, false);
    }
}

static void M_SetTempo(MDRV *const mdrv, const uint8_t tempo)
{
    mdrv->timer_b = tempo < 17 ? 1 : (uint8_t)-(4340 / tempo);
    M_Write(mdrv, 0x26, mdrv->timer_b);
}

// Ends a track. An ended effect returns its SSG channel's mixer and noise
// settings to the music and mutes the channel.
static void M_EndTrack(MDRV *const mdrv, const M_TRACK *const track)
{
    M_Voice(mdrv, track)->portamento_enabled = false;
    if (!track->effect) {
        return;
    }
    const uint8_t channel = M_Channel(track);
    const uint8_t mask = (1 << channel) | (8 << channel);
    M_Output(
        mdrv, track, 7,
        (mdrv->registers[7] & ~mask) | (mdrv->music_mixer & mask), false);
    M_Output(mdrv, track, 6, mdrv->music_noise, false);
    M_Output(mdrv, track, 8 + channel, 0, false);
}

// Sequencer

static void M_KeyOff(MDRV *const mdrv, M_TRACK *const track, const bool voice)
{
    track->gate_remaining = 0;
    if (track->key_on) {
        track->key_on = false;
        if (voice) {
            M_VoiceOff(mdrv, track);
        }
    }
}

static void M_ResetTrack(
    M_TRACK *const track, const uint8_t channel, const uint32_t position)
{
    *track = (M_TRACK) {
        .position = position,
        .channel = channel,
        .active = channel != 0,
        .gate = 8,
        .return_position = M_NO_POSITION,
        .repeat_one_end = M_NO_POSITION,
    };
}

// Assigns physical channel slots by family ordinal in directory order.
static uint8_t M_PhysicalChannel(const uint8_t source, uint8_t counts[4])
{
    for (size_t group = 0; group < sizeof(m_ChannelOrder); group++) {
        if ((source & 0xf0) == m_ChannelOrder[group]) {
            return m_ChannelOrder[group] + counts[group]++;
        }
    }
    return 0;
}

static void M_Range(
    const M_SEQUENCE *const sequence, const uint32_t position,
    const uint32_t length)
{
    M_FATAL_IF(
        position < sequence->commands_start
            || position >= sequence->commands_end
            || length > sequence->commands_end - position,
        "MDRV command range outside stream: %u + %u", position, length);
}

static uint16_t M_Word(const uint8_t *const bytes)
{
    return bytes[0] | ((uint16_t)bytes[1] << 8);
}

static uint32_t M_Branch(
    const M_SEQUENCE *const sequence, const uint32_t base,
    const uint8_t *const displacement)
{
    const uint16_t word = M_Word(displacement);
    const int32_t relative = word >= 0x8000 ? (int32_t)word - 65536 : word;
    const int64_t target = (int64_t)base + relative;
    M_FATAL_IF(
        target < sequence->commands_start || target >= sequence->commands_end,
        "MDRV branch outside stream: %lld", (long long)target);
    return (uint32_t)target;
}

static uint8_t M_ParameterCount(const uint8_t opcode, const uint8_t channel)
{
    switch (opcode) {
    case 0xe1:
    case 0xe2:
    case 0xe3:
    case 0xe5:
    case 0xfe:
    case 0xff:
        return 0;
    case 0xe8:
    case 0xed:
    case 0xf2:
    case 0xfb:
    case 0xfc:
    case 0xfd:
        return 4;
    case 0xee:
    case 0xf3:
    case 0xf9:
    case 0xfa:
        return 2;
    case 0xf6:
    case 0xf7:
        return 3;
    case 0xf1:
        return channel == 0x10 ? 2 : 1;
    default:
        return 1;
    }
}

static bool M_IsNote(const uint8_t opcode)
{
    return opcode <= 0x8b && (opcode & 15) <= 11;
}

// Plays a note (00..8B), rest (90) or portamento (F2), merging same-pitch
// ties (91). A tie to a different pitch makes the next note legato and keeps
// this one from keying off at its gate.
static void M_Duration(
    MDRV *const mdrv, M_SEQUENCE *const sequence, M_TRACK *const track,
    const uint8_t opcode, const uint8_t *const params, uint32_t *const budget)
{
    uint32_t duration = params[opcode == 0xf2 ? 1 : 0];
    bool slur = false;
    if (opcode != 0xf2 && opcode != 0x90) {
        while (track->position < sequence->commands_end
               && sequence->work[track->position] == 0x91) {
            M_FATAL_IF(*budget == 0, "MDRV tie chain exceeds pulse budget");
            (*budget)--;
            M_Range(sequence, track->position, 2);
            track->position++;
            if (sequence->work[track->position] != opcode) {
                slur = true;
                break;
            }
            M_Range(sequence, track->position, 2);
            duration += sequence->work[track->position + 1];
            M_FATAL_IF(
                duration > UINT16_MAX, "MDRV tied duration exceeds work range");
            track->position += 2;
        }
    } else if (
        track->position < sequence->commands_end
        && sequence->work[track->position] == 0x91) {
        track->position++;
        slur = true;
    }
    const bool legato = track->legato;
    track->legato = slur;
    track->remaining = duration;
    track->gate_remaining = 0;
    if (opcode == 0x90) {
        M_Rest(mdrv, track);
        return;
    }
    track->key_on = true;
    const uint32_t gate =
        opcode == 0xf2 ? duration : duration * track->gate / 8;
    M_StartNote(mdrv, track, opcode, params, legato);
    if (!slur && gate != 0) {
        track->gate_remaining = gate - 1;
        if (track->gate_remaining == 0) {
            M_KeyOff(mdrv, track, true);
        }
    }
}

static void M_Command(
    MDRV *const mdrv, M_SEQUENCE *const sequence, M_TRACK *const track,
    const uint32_t offset, const uint8_t opcode, const uint8_t *const params)
{
    uint32_t target = 0;
    switch (opcode) {
    case 0xe0:
        M_FATAL_IF(track->repeat_one, "Nested MDRV E0 repeat");
        track->repeat_one = true;
        track->repeat_one_start = track->position;
        track->repeat_one_end = M_NO_POSITION;
        track->repeat_one_count = params[0];
        break;
    case 0xe1:
        M_FATAL_IF(!track->repeat_one, "MDRV E1 without E0");
        if (track->repeat_one_count == 1) {
            M_FATAL_IF(
                track->repeat_one_end == M_NO_POSITION,
                "MDRV E1 has no repeat end");
            track->position = track->repeat_one_end;
            track->repeat_one = false;
        }
        break;
    case 0xe2:
        M_FATAL_IF(!track->repeat_one, "MDRV E2 without E0");
        track->repeat_one_count--;
        if (track->repeat_one_count != 0) {
            track->repeat_one_end = track->position;
            track->position = track->repeat_one_start;
        } else {
            track->repeat_one = false;
        }
        break;
    case 0xe3:
        track->key_on = false;
        track->gate_remaining = 0;
        M_VoiceOff(mdrv, track);
        break;
    case 0xe4:
        M_FATAL_IF(track->repeat_two, "Nested MDRV E4 repeat");
        track->repeat_two = true;
        track->repeat_two_start = track->position;
        track->repeat_two_count = params[0];
        break;
    case 0xe5:
        M_FATAL_IF(!track->repeat_two, "MDRV E5 without E4");
        if (track->repeat_two_count == 0 || --track->repeat_two_count != 0) {
            track->position = track->repeat_two_start;
        } else {
            track->repeat_two = false;
        }
        break;
    case 0xe6:
        M_Voice(mdrv, track)->detune = (int8_t)params[0];
        break;
    case 0xe7:
        M_Voice(mdrv, track)->transpose = (int8_t)params[0];
        break;
    case 0xe8:
    case 0xed:
    case 0xfb:
    case 0xfc:
        M_SetLfo(mdrv, track, opcode, params);
        break;
    case 0xe9:
        M_SetTempo(mdrv, params[0]);
        break;
    case 0xea:
        M_FATAL_IF(params[0] > 8, "Invalid MDRV gate");
        track->gate = params[0];
        break;
    case 0xeb:
        M_Program(mdrv, track, params[0]);
        break;
    case 0xec:
    case 0xf4:
    case 0xf5:
        M_SetVolume(mdrv, track, opcode, params);
        break;
    case 0xee:
        M_Output(mdrv, track, params[0], params[1], true);
        break;
    case 0xef:
        M_SetDelayNoise(mdrv, track, params[0]);
        break;
    case 0xf0:
        Mdrv_Fade(mdrv, params[0]);
        break;
    case 0xf1:
        M_SetPan(mdrv, track, params);
        break;
    case 0xf3:
        track->position = M_Branch(sequence, offset + 2, params);
        break;
    case 0xf6:
        target = M_Branch(sequence, track->position, params + 1);
        sequence->work[target] = params[0];
        break;
    case 0xf7:
        target = offset + 1;
        if (sequence->work[target] != 0) {
            sequence->work[target]--;
            if (sequence->work[target] == 0) {
                break;
            }
        }
        track->position = M_Branch(sequence, offset + 3, params + 1);
        break;
    case 0xf8:
        break;
    case 0xf9:
        target = M_Branch(sequence, track->position, params);
        if (sequence->work[target] == 1) {
            M_Range(sequence, target, 3);
            track->position = target + 3;
        }
        break;
    case 0xfa:
        M_FATAL_IF(
            sequence->music == nullptr, "MDRV effect calls music pattern");
        M_FATAL_IF(
            track->return_position != M_NO_POSITION, "Nested MDRV user track");
        target = M_Word(params);
        M_Range(sequence, target, 1);
        track->return_position = track->position;
        track->position = target;
        break;
    case 0xfd:
        M_SetHardwareLfo(mdrv, track, params);
        break;
    case 0xfe:
        break;
    case 0xff:
        if (track->return_position != M_NO_POSITION) {
            track->position = track->return_position;
            track->return_position = M_NO_POSITION;
            break;
        }
        if (sequence->music == nullptr) {
            track->key_on = false;
            track->gate_remaining = 0;
            sequence->effect_counter--;
        }
        track->active = false;
        M_EndTrack(mdrv, track);
        break;
    default:
        LOG_FATAL("Unsupported MDRV opcode %02x at %u", opcode, offset);
    }
}

// Runs one clock of a track: its voice's periodic work, the gate countdown,
// and the commands up to the next note or rest.
static void M_Advance(
    MDRV *const mdrv, M_SEQUENCE *const sequence, M_TRACK *const track)
{
    M_Clock(mdrv, track);
    if (!track->active) {
        return;
    }
    if (track->gate_remaining != 0 && --track->gate_remaining == 0) {
        M_KeyOff(mdrv, track, true);
    }
    if (track->remaining != 0 && --track->remaining != 0) {
        return;
    }
    uint32_t budget = M_COMMAND_BUDGET;
    while (track->active && track->remaining == 0) {
        M_FATAL_IF(budget == 0, "MDRV track exceeds pulse budget");
        budget--;
        const uint32_t offset = track->position;
        M_Range(sequence, offset, 1);
        const uint8_t opcode = sequence->work[offset];
        M_FATAL_IF(
            !M_IsNote(opcode) && opcode != 0x90 && opcode < 0xe0,
            "Invalid MDRV opcode %02x at %u", opcode, offset);
        uint8_t count =
            opcode < 0xe0 ? 1 : M_ParameterCount(opcode, track->channel);
        M_Range(sequence, offset, (uint32_t)count + 1);
        const uint8_t *const params = sequence->work + offset + 1;
        if (opcode == 0xec && track->channel == 0x10) {
            count = (params[0] & 0x80) != 0 ? 2 : 7;
            M_Range(sequence, offset, (uint32_t)count + 1);
        }
        // Copy parameters before a shared repeat counter can change them.
        uint8_t copy[7];
        memcpy(copy, params, count);
        track->position += (uint32_t)count + 1;
        if (opcode < 0xe0 || opcode == 0xf2) {
            M_FATAL_IF(
                opcode == 0xf2 && !M_IsNote(copy[0]),
                "Invalid MDRV portamento pitch");
            M_Duration(mdrv, sequence, track, opcode, copy, &budget);
        } else {
            M_Command(mdrv, sequence, track, offset, opcode, copy);
        }
    }
}

static void M_Mask(
    M_SEQUENCE *const music, const uint8_t channel, const bool suppressed,
    const bool active_only)
{
    for (uint16_t i = 0; i < music->track_count; i++) {
        M_TRACK *const track = &music->tracks[i];
        if (track->channel == channel && (!active_only || track->active)) {
            track->suppressed = suppressed;
        }
    }
}

// Visits music tracks in directory order and effect tracks from the last
// slot down.
static M_TRACK *M_Track(M_SEQUENCE *const sequence, const uint16_t ordinal)
{
    return &sequence->tracks
                [sequence->music != nullptr
                     ? ordinal
                     : sequence->track_count - 1 - ordinal];
}

// Clocks the tracks whose channel masked by mask equals group. Music SSG
// tracks keep their envelope clock after they end.
static void M_AdvanceGroup(
    MDRV *const mdrv, M_SEQUENCE *const sequence, const uint8_t group,
    const uint8_t mask, const bool idle_ssg)
{
    if (!sequence->playing && !idle_ssg) {
        return;
    }
    for (uint16_t ordinal = 0; ordinal < sequence->track_count; ordinal++) {
        M_TRACK *const track = M_Track(sequence, ordinal);
        const bool clock = track->active
            || (sequence->music != nullptr && (track->channel & 0xf0) == 0x40);
        if (clock && (track->channel & mask) == group) {
            M_Advance(mdrv, sequence, track);
        }
    }
}

static bool M_AnyActive(const M_SEQUENCE *const sequence)
{
    for (uint16_t i = 0; i < sequence->track_count; i++) {
        if (sequence->tracks[i].active) {
            return true;
        }
    }
    return false;
}

static bool M_MusicFinished(const M_SEQUENCE *const music)
{
    return music->playing && !M_AnyActive(music);
}

// Stops a sequence's tracks, keying off their voices when voice is set.
static void M_StopSequence(
    MDRV *const mdrv, M_SEQUENCE *const sequence, const bool voice)
{
    if (!sequence->playing) {
        return;
    }
    for (size_t group = 0; group < sizeof(m_ChannelOrder); group++) {
        for (uint16_t ordinal = 0; ordinal < sequence->track_count; ordinal++) {
            M_TRACK *const track = M_Track(sequence, ordinal);
            if ((track->channel & 0xf0) == m_ChannelOrder[group]) {
                M_KeyOff(mdrv, track, voice);
                track->active = false;
                track->remaining = 0;
                track->legato = false;
            }
        }
    }
    sequence->playing = false;
}

static void M_ResetMusicTracks(M_SEQUENCE *const music, const bool active)
{
    uint8_t counts[4] = {};
    for (uint16_t i = 0; i < music->track_count; i++) {
        const MDRV_TRACK *const source = &music->music->tracks[i];
        M_ResetTrack(
            &music->tracks[i], M_PhysicalChannel(source->channel, counts),
            source->offset);
        music->tracks[i].active &= active;
    }
}

// Ends the song after its last track, or restarts it when header flag 80
// requests repeat. Both cancel the effects and reset the controls, keeping
// volumes and programs; only the end silences the chip.
static void M_CompleteMusic(MDRV *const mdrv)
{
    M_SEQUENCE *const music = &mdrv->music_sequence;
    const bool restart = (music->music->flags & 0x80) != 0;
    M_ResetMusicTracks(music, restart);
    music->playing = restart;
    mdrv->effect_sequence.playing = false;
    M_ResetControls(mdrv);
    if (!restart) {
        M_Silence(mdrv);
        M_Setup(mdrv);
        M_Silence(mdrv);
    }
}

static void M_Finish(MDRV *const mdrv, M_SEQUENCE *const sequence)
{
    if (sequence->playing && !M_AnyActive(sequence)) {
        M_StopSequence(mdrv, sequence, true);
    }
}

// Advances music and effects on one timer B IRQ. Each SSG channel runs its
// effect before its music, which keeps running but writes no ordinary output
// while the effect holds the channel.
static void M_Pulse(MDRV *const mdrv)
{
    M_SEQUENCE *const music = &mdrv->music_sequence;
    M_SEQUENCE *const effects = &mdrv->effect_sequence;
    for (size_t group = 0; group < 3; group++) {
        M_AdvanceGroup(mdrv, music, m_ChannelOrder[group], 0xf0, false);
        if (M_MusicFinished(music)) {
            M_CompleteMusic(mdrv);
            return;
        }
    }
    for (uint8_t channel = 0x40; channel <= 0x42; channel++) {
        bool owned = false;
        for (uint16_t i = 0; i < effects->track_count; i++) {
            owned |= effects->tracks[i].active
                && effects->tracks[i].channel == channel;
        }
        M_Mask(music, channel, owned, false);
        M_AdvanceGroup(mdrv, effects, channel, 0xff, false);
        // The driver's wrapping effect count, not the live tracks, releases
        // the music.
        if (effects->effect_counter == 0) {
            M_Mask(music, channel, false, true);
        }
        M_AdvanceGroup(mdrv, music, channel, 0xff, effects->playing);
        if (M_MusicFinished(music)) {
            M_CompleteMusic(mdrv);
            return;
        }
    }
    M_Finish(mdrv, music);
    M_Finish(mdrv, effects);
}

static void M_Timer(
    void *const context, OPNA *const chip, const unsigned timer,
    const uint64_t time)
{
    MDRV *const mdrv = context;
    const uint8_t status = Opna_ReadPort(chip, 0) & 3;
    if (status == 0) {
        return;
    }
    M_Write(mdrv, 0x27, 0x3f);
    if ((status & 2) == 0) {
        return;
    }
    M_Pulse(mdrv);
    if (mdrv->fading && --mdrv->fade_counter == 0) {
        mdrv->fade_counter = mdrv->fade_interval;
        if (mdrv->fade_direction == 1) {
            if (++mdrv->fade_amount >= 127) {
                Mdrv_Stop(mdrv);
            }
        } else if (--mdrv->fade_amount == 0) {
            mdrv->fading = false;
        }
    }
}

// Initialization

static void M_ReadSsg(READER *const reader, M_SSG_PROGRAM *const out)
{
    out->initial = Reader_ReadU8(reader);
    out->attack = Reader_ReadU8(reader);
    out->decay = Reader_ReadU8(reader);
    out->sustain = Reader_ReadU8(reader);
    out->sustain_rate = Reader_ReadU8(reader);
    out->release = Reader_ReadU8(reader);
}

static RESULT M_ValidateSong(const MDRV_MUSIC *const song)
{
    FAIL_IF(
        song->size > 65536 || song->track_count == 0,
        "Invalid MDRV song workspace");
    uint8_t counts[4] = {};
    for (uint16_t i = 0; i < song->track_count; i++) {
        FAIL_IF(
            (song->tracks[i].channel & 0x20) != 0,
            "MDRV external PCM tracks are unsupported");
        M_PhysicalChannel(song->tracks[i].channel, counts);
        FAIL_IF(
            counts[0] > 6 || counts[2] > 1 || counts[3] > 3,
            "MDRV song exceeds physical channel slots");
    }
    return OK;
}

static RESULT M_LoadSong(
    const MDRV_MUSIC *const asset, const ALLOCATOR *const allocator,
    M_SONG *const song)
{
    song->asset = asset;
    song->fm = Allocator_AllocZ(allocator, asset->fm_count * sizeof(*song->fm));
    song->ssg =
        Allocator_AllocZ(allocator, asset->ssg_count * sizeof(*song->ssg));
    READER reader;
    Reader_OpenBuffer(&reader, asset->bytes, asset->size, nullptr);
    Reader_Seek(&reader, asset->fm_offset);
    for (uint16_t n = 0; n < asset->fm_count; n++) {
        M_FM_PROGRAM *const program = &song->fm[n];
        Reader_Skip(&reader, 3);
        program->mask = (Reader_ReadU8(&reader) & 0x78) << 1;
        program->algorithm = Reader_ReadU8(&reader);
        Reader_Skip(&reader, 1);
        uint8_t *const rows[] = {
            program->dt_mul, program->tl, program->ks_ar,
            program->am_dr,  program->sr, program->sl_rr
        };
        for (size_t row = 0; row < 6; row++) {
            for (size_t op = 0; op < 4; op++) {
                rows[row][op] = Reader_ReadU8(&reader);
            }
        }
        Reader_Skip(&reader, 2);
    }
    Reader_Seek(&reader, asset->ssg_offset);
    for (uint16_t n = 0; n < asset->ssg_count; n++) {
        M_ReadSsg(&reader, &song->ssg[n]);
    }
    return Reader_Result(&reader);
}

static void M_InitSequence(
    M_SEQUENCE *const sequence, const size_t size, const uint16_t capacity,
    const ALLOCATOR *const allocator)
{
    *sequence = (M_SEQUENCE) {
        .work_size = size,
        .capacity = capacity,
    };
    const size_t tracks_size = capacity * sizeof(*sequence->tracks);
    uint8_t *const block = Allocator_AllocZ(allocator, tracks_size + size);
    sequence->tracks = (M_TRACK *)block;
    sequence->work = block + tracks_size;
}

RESULT Mdrv_Init(
    const MDRV_MUSIC *const songs, const size_t song_count,
    const MDRV_EFFECTS *const effects, const uint8_t fm_balance,
    const uint8_t rhythm_balance, const ALLOCATOR *const allocator,
    MDRV **const out)
{
    *out = nullptr;
    FAIL_IF(
        fm_balance > 127 || rhythm_balance > 63,
        "Invalid MDRV balance configuration");
    FAIL_IF(song_count == 0 || song_count > 256, "Invalid MDRV song bank size");
    size_t max_size = 0;
    uint16_t max_tracks = 0;
    for (size_t i = 0; i < song_count; i++) {
        MUST(M_ValidateSong(&songs[i]));
        if (songs[i].size > max_size) {
            max_size = songs[i].size;
        }
        if (songs[i].track_count > max_tracks) {
            max_tracks = songs[i].track_count;
        }
    }
    MDRV *const mdrv = Allocator_AllocZ(allocator, sizeof(*mdrv));
    mdrv->songs =
        Allocator_AllocZ(allocator, song_count * sizeof(*mdrv->songs));
    mdrv->song_count = song_count;
    mdrv->fm_balance = fm_balance;
    mdrv->rhythm_balance = rhythm_balance;
    mdrv->effects = effects;
    for (size_t i = 0; i < song_count; i++) {
        MUST(M_LoadSong(&songs[i], allocator, &mdrv->songs[i]));
    }
    mdrv->effect_programs = Allocator_AllocZ(
        allocator, effects->ssg_count * sizeof(*mdrv->effect_programs));
    READER reader;
    Reader_OpenBuffer(&reader, effects->bytes, effects->size, nullptr);
    Reader_Seek(&reader, effects->ssg_offset);
    for (uint16_t n = 0; n < effects->ssg_count; n++) {
        M_ReadSsg(&reader, &mdrv->effect_programs[n]);
    }
    MUST(Reader_Result(&reader));
    M_InitSequence(&mdrv->music_sequence, max_size, max_tracks, allocator);
    mdrv->music_sequence.music = &songs[0];
    M_InitSequence(
        &mdrv->effect_sequence, effects->size, MDRV_EFFECT_MAX_CHANNELS,
        allocator);
    M_SEQUENCE *const sequence = &mdrv->effect_sequence;
    sequence->commands_start = 16 + (uint32_t)effects->effect_count * 4;
    sequence->commands_end = effects->ssg_offset;
    memcpy(sequence->work, effects->bytes, effects->size);
    mdrv->chip = Opna_Init(allocator, M_Timer, mdrv);
    for (size_t i = 0; i < 6; i++) {
        mdrv->patterns[i] = m_DefaultEnvelope;
    }
    M_ResetControls(mdrv);
    mdrv->timer_b = 0xd0;
    Opna_Reset(mdrv->chip);
    M_Write(mdrv, 0x22, 0);
    M_Write(mdrv, 0x10, 0xbf);
    M_Write(mdrv, 0x29, 0x83);
    M_Write(mdrv, 0x24, 0);
    M_Write(mdrv, 0x25, 0);
    M_Write(mdrv, 0x26, 0xe0);
    M_Write(mdrv, 0x27, 0x3f);
    M_Silence(mdrv);
    M_Setup(mdrv);
    *out = mdrv;
    return OK;
}

// Controls

void Mdrv_Play(MDRV *const mdrv, const size_t song)
{
    M_FATAL_IF(song >= mdrv->song_count, "Invalid MDRV song index %zu", song);
    mdrv->song = song;
    M_StopSequence(mdrv, &mdrv->effect_sequence, false);
    M_SEQUENCE *const music = &mdrv->music_sequence;
    const MDRV_MUSIC *const asset = mdrv->songs[song].asset;
    music->music = asset;
    music->work_size = asset->size;
    music->commands_start = asset->commands_offset;
    music->commands_end = asset->fm_offset;
    memcpy(music->work, asset->bytes, asset->size);
    memset(music->tracks, 0, music->capacity * sizeof(*music->tracks));
    music->playing = true;
    music->track_count = asset->track_count;
    M_ResetMusicTracks(music, true);
    memset(mdrv->music, 0, sizeof(mdrv->music));
    memset(mdrv->effect, 0, sizeof(mdrv->effect));
    M_ResetControls(mdrv);
    M_Silence(mdrv);
    M_Setup(mdrv);
    M_Write(mdrv, 0x26, mdrv->timer_b);
}

void Mdrv_Effect(MDRV *const mdrv, const uint16_t effect)
{
    if (effect == 0) {
        return;
    }
    M_SEQUENCE *const sequence = &mdrv->effect_sequence;
    M_FATAL_IF(
        effect > mdrv->effects->effect_count, "Invalid MDRV effect %u", effect);
    sequence->playing = true;
    const MDRV_EFFECT *const source = &mdrv->effects->effects[effect - 1];
    sequence->effect_counter = source->channel_count;
    if (sequence->track_count < source->channel_count) {
        sequence->track_count = source->channel_count;
    }
    for (uint16_t i = 0; i < source->channel_count; i++) {
        M_TRACK *const track = &sequence->tracks[i];
        track->position = source->stream_offsets[i];
        track->remaining = 0;
        track->gate_remaining = 0;
        track->active = true;
        track->effect = true;
        track->repeat_one = false;
        track->repeat_two = false;
        track->repeat_one_end = M_NO_POSITION;
        track->return_position = M_NO_POSITION;
    }
}

void Mdrv_Stop(MDRV *const mdrv)
{
    M_StopSequence(mdrv, &mdrv->music_sequence, false);
    M_StopSequence(mdrv, &mdrv->effect_sequence, false);
    M_ResetControls(mdrv);
    M_Silence(mdrv);
    M_Setup(mdrv);
    M_Silence(mdrv);
}

void Mdrv_Fade(MDRV *const mdrv, const uint8_t interval)
{
    mdrv->fading = true;
    mdrv->fade_interval = interval & 0x7f;
    mdrv->fade_counter = mdrv->fade_interval;
    mdrv->fade_direction = interval < 128 ? 1 : 2;
    mdrv->fade_amount = interval < 128 ? 0 : 127;
}

void Mdrv_Render(
    MDRV *const mdrv, MDRV_FRAME *const output, const size_t frames)
{
    const M_SEQUENCE *const effects = &mdrv->effect_sequence;
    for (size_t i = 0; i < frames; i++) {
        OPNA_FRAME frame;
        Opna_Render(mdrv->chip, &frame, 1);
        if (output == nullptr) {
            continue;
        }
        bool effect[3] = {};
        for (size_t track = 0; track < effects->track_count; track++) {
            if (effects->tracks[track].active) {
                effect[effects->tracks[track].channel & 3] = true;
            }
        }
        for (size_t stereo = 0; stereo < 2; stereo++) {
            output[i].music[stereo] = frame.fm_rhythm[stereo];
            output[i].effects[stereo] = 0;
            for (size_t channel = 0; channel < 3; channel++) {
                if (effect[channel]) {
                    output[i].effects[stereo] += frame.ssg[channel];
                } else {
                    output[i].music[stereo] += frame.ssg[channel];
                }
            }
        }
    }
}
