#include "pc98/mdrv_synth.h"

#include <string.h>

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

struct MDRV_SYNTH {
    MDRV_SYNTH_CONFIG config;
    OPNA *chip;
    M_SONG *songs;
    size_t song_count, song;
    const MDRV_EFFECTS *effects;
    M_SSG_PROGRAM *effect_programs;
    MDRV_SEQUENCE music_sequence, effect_sequence;
    M_VOICE music[10], effect[3];
    M_SSG_PROGRAM patterns[6];
    uint8_t registers[512], rhythm[6], music_mixer, music_noise;
    uint8_t timer_b, fade_interval, fade_counter, fade_direction, fade_amount;
    uint64_t pulses;
    bool fading, failed, ignore_events;
    RESULT pending;
    MDRV_REGISTER_CALLBACK callback;
    void *context;
};

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

static void M_Write(
    MDRV_SYNTH *const synth, const uint16_t reg, const uint8_t value)
{
    synth->registers[reg] = value;
    Opna_Write(synth->chip, reg, value);
    if (synth->callback != nullptr) {
        synth->callback(synth->context, Opna_Time(synth->chip), reg, value);
    }
}

static void M_Output(
    MDRV_SYNTH *const synth, const MDRV_EVENT *const event, const uint8_t reg,
    const uint8_t value, const bool raw)
{
    if (!raw && event->suppressed) {
        return;
    }
    const uint16_t bank = (event->channel >= 0x83 && reg >= 0x30) ? 0x100 : 0;
    M_Write(
        synth, bank + reg, !raw && reg == 7 ? (value & 0x3f) | 0x80 : value);
}

static uint8_t M_Channel(const MDRV_EVENT *const event)
{
    if (event->channel == 0x10) {
        // Retain the IRQ index after the PCM slot.
        return 4;
    }
    return (event->channel & 0x80) != 0 ? (event->channel & 7) % 3
                                        : event->channel & 3;
}

static uint8_t M_EffectSlot(const MDRV_EVENT *const event)
{
    // Effect tracks always carry the SSG tags 0x40 to 0x42.
    return (event->channel & 3) % 3;
}

static M_VOICE *M_Voice(MDRV_SYNTH *const synth, const MDRV_EVENT *const event)
{
    if (event->effect_number != 0) {
        return &synth->effect[M_EffectSlot(event)];
    }
    return &synth->music
                [(event->channel & 0x80) != 0       ? event->channel & 7
                     : (event->channel & 0x40) != 0 ? 7 + M_Channel(event)
                                                    : 6];
}

static M_SSG_PROGRAM *M_Pattern(
    MDRV_SYNTH *const synth, const MDRV_EVENT *const event)
{
    return &synth->patterns
                [event->effect_number != 0 ? 3 + M_EffectSlot(event)
                                           : M_Channel(event)];
}

static void M_Silence(MDRV_SYNTH *const synth)
{
    for (int i = 6; i >= 0; i--) {
        M_Write(synth, 0x28, i);
    }
    M_Write(synth, 0x10, 0xff);
    M_Write(synth, 7, 0xb8);
    for (uint16_t reg = 8; reg <= 10; reg++) {
        M_Write(synth, reg, 0);
    }
    M_Write(synth, 0x10b, 0);
    M_Write(synth, 0x100, 1);
    M_Write(synth, 0x101, 0xc0);
    M_Write(synth, 0x10c, 0xff);
    M_Write(synth, 0x10d, 0xff);
}

static void M_Setup(MDRV_SYNTH *const synth)
{
    synth->music_mixer = 0xb8;
    synth->music_noise = synth->registers[6];
    M_Write(synth, 7, 0xb8);
    for (int bank = 0; bank < 2; bank++) {
        for (int i = 0; i < 3; i++) {
            M_Write(synth, bank * 256 + 0xb4 + i, 0xc0);
        }
        if (bank == 0) {
            for (int i = 0; i < 6; i++) {
                M_Write(synth, 0x18 + i, 0xc0);
                synth->rhythm[i] = 0xc0;
            }
        }
    }
    M_Write(synth, 0x22, 0);
    M_Write(synth, 0x12, 0);
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

static void M_ResetControls(MDRV_SYNTH *const synth)
{
    for (size_t i = 0; i < 10; i++) {
        M_ResetVoice(&synth->music[i]);
    }
    for (size_t i = 0; i < 3; i++) {
        synth->effect[i] = synth->music[7 + i];
        MDRV_CHANNEL_STATE *const state = &synth->effect_sequence.tracks[2 - i];
        const uint8_t source_channel = state->source_channel;
        *state = (MDRV_CHANNEL_STATE) {
            .channel = 0x40 + i,
            .source_channel = source_channel,
            .position = UINT32_MAX,
            .return_position = UINT32_MAX,
            .repeat_one_end = UINT32_MAX,
            .gate = 8,
            .pan = 3,
        };
        for (uint16_t track = 0; track < synth->music_sequence.track_count;
             track++) {
            const MDRV_CHANNEL_STATE *const music =
                &synth->music_sequence.tracks[track];
            if (music->channel == state->channel) {
                state->program = music->program;
                state->volume = music->volume;
            }
        }
    }
    synth->fading = false;
    synth->fade_amount = 0;
}

static uint8_t M_Tl(
    const MDRV_SYNTH *const synth, const M_VOICE *const voice,
    const uint8_t base)
{
    const uint8_t balance = synth->config.fm_balance;
    const uint8_t effective =
        voice->volume < balance ? 0 : voice->volume - balance;
    const uint8_t tl = base + (uint8_t)(127 - effective) + voice->attenuation;
    return tl >= 128 ? 127 : tl;
}

static void M_Volume(
    MDRV_SYNTH *const synth, const MDRV_EVENT *const event,
    const M_VOICE *const voice)
{
    const uint8_t channel = M_Channel(event);
    if ((event->channel & 0x80) != 0) {
        const uint8_t algorithm = voice->algorithm & 7;
        M_Output(
            synth, event, 0x4c + channel, M_Tl(synth, voice, voice->tl[3]),
            false);
        if (algorithm >= 4) {
            M_Output(
                synth, event, 0x48 + channel, M_Tl(synth, voice, voice->tl[2]),
                false);
        }
        if (algorithm >= 5) {
            M_Output(
                synth, event, 0x44 + channel, M_Tl(synth, voice, voice->tl[1]),
                false);
        }
        if (algorithm == 7) {
            M_Output(
                synth, event, 0x40 + channel, M_Tl(synth, voice, voice->tl[0]),
                false);
        }
    } else if (event->channel == 0x10) {
        const uint8_t attenuation = voice->attenuation >> 1;
        uint8_t value = 0;
        if (voice->volume >= attenuation) {
            const uint8_t level = (voice->volume - attenuation) & 0x3f;
            const uint8_t balance = synth->config.rhythm_balance;
            value = (level < balance ? 0 : level - balance) + 1;
        }
        M_Output(synth, event, 0x11, value, false);
    } else if ((voice->program & 0xc0) != 0) {
        const uint8_t attenuation = voice->attenuation >> 3;
        M_Output(
            synth, event, 8 + channel,
            voice->previous_volume < attenuation
                ? 0
                : voice->previous_volume - attenuation,
            false);
    }
}

static void M_Pitch(
    MDRV_SYNTH *const synth, const MDRV_EVENT *const event,
    const M_VOICE *const voice)
{
    const uint8_t channel = M_Channel(event);
    if ((event->channel & 0x80) != 0
        || (event->channel == 0x10 && event->kind == MDRV_EVENT_PORTAMENTO)) {
        M_Output(synth, event, 0xa4 + channel, voice->pitch >> 8, false);
        M_Output(synth, event, 0xa0 + channel, voice->pitch, false);
    } else if ((voice->program & 0xc0) != 0) {
        const uint16_t pitch =
            voice->octave >= 16 ? 0 : voice->pitch >> voice->octave;
        M_Output(synth, event, channel * 2 + 1, pitch >> 8, false);
        M_Output(synth, event, channel * 2, pitch, false);
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
    MDRV_SYNTH *const synth, const MDRV_EVENT *const event,
    M_VOICE *const voice)
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
            M_Volume(synth, event, voice);
        }
        if (reached) {
            if (level == 0 || voice->envelope_stage >= 2) {
                voice->envelope = false;
                voice->envelope_level = 0;
            } else {
                M_EnvelopeStage(
                    voice, M_Pattern(synth, event), voice->envelope_stage + 1);
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
    MDRV_SYNTH *const synth, const MDRV_EVENT *const event,
    M_VOICE *const voice)
{
    if ((event->channel & 0x80) != 0) {
        voice->pitch += voice->portamento;
        M_Pitch(synth, event, voice);
    } else if ((event->channel & 0x40) != 0) {
        if ((voice->program & 0xc0) != 0) {
            if (voice->portamento_enabled) {
                voice->pitch += voice->portamento;
            }
            M_Pitch(synth, event, voice);
        }
        M_Envelope(synth, event, voice);
    }
}

static void M_Clock(
    MDRV_SYNTH *const synth, const MDRV_EVENT *const event,
    M_VOICE *const voice)
{
    if (event->parameters[0] == 0) {
        M_Envelope(synth, event, voice);
        return;
    }
    if (synth->fading) {
        voice->attenuation = synth->fade_amount;
        M_Volume(synth, event, voice);
    } else {
        if (voice->pitch_lfo.enabled && event->channel != 0x10) {
            M_Lfo(voice, &voice->pitch_lfo, false);
        }
        if (voice->volume_lfo.enabled) {
            M_Lfo(voice, &voice->volume_lfo, true);
            M_Volume(synth, event, voice);
        }
    }
    M_Tail(synth, event, voice);
}

static uint8_t M_Pan(const uint8_t value)
{
    const uint8_t pan = value & 3;
    return (pan == 1 ? 2 : pan == 2 ? 1 : pan) << 6;
}

static void M_Mixer(
    MDRV_SYNTH *const synth, const MDRV_EVENT *const event,
    const M_VOICE *const voice)
{
    const unsigned channel = M_Channel(event);
    const uint8_t mask = (1 << channel) | (8 << channel);
    const uint8_t mode = voice->noise_mode;
    const uint8_t bits = ((mode & 0x40) != 0 ? 1 << channel : 0)
        | ((mode & 0x80) != 0 ? 8 << channel : 0);
    if (!event->suppressed) {
        M_Output(synth, event, 7, (synth->registers[7] & ~mask) | bits, false);
        if (event->effect_number != 0) {
            return;
        }
    }
    synth->music_mixer = (synth->music_mixer & ~mask) | bits;
}

static RESULT M_Program(
    MDRV_SYNTH *const synth, const MDRV_EVENT *const event,
    M_VOICE *const voice)
{
    const uint8_t encoded = event->parameters[0];
    const size_t number = encoded & 0x3f;
    voice->program = encoded;
    if ((event->channel & 0x80) != 0) {
        const M_SONG *const song = &synth->songs[synth->song];
        FAIL_IF(
            number >= song->asset->fm_count, "MDRV FM program outside bank");
        const M_FM_PROGRAM *const program = &song->fm[number];
        const uint8_t channel = M_Channel(event);
        for (uint8_t i = 0; i < 4; i++) {
            M_Output(synth, event, 0x80 + i * 4 + channel, 0xff, false);
        }
        voice->mask = program->mask;
        voice->algorithm = program->algorithm;
        memcpy(voice->tl, program->tl, 4);
        for (uint8_t i = 0; i < 4; i++) {
            M_Output(
                synth, event, 0x30 + i * 4 + channel, program->dt_mul[i],
                false);
        }
        M_Volume(synth, event, voice);
        const uint8_t algorithm = program->algorithm & 7;
        for (int i = 2; i >= 0; i--) {
            if ((i == 2 && algorithm < 4) || (i == 1 && algorithm < 5)
                || (i == 0 && algorithm < 7)) {
                M_Output(
                    synth, event, 0x40 + i * 4 + channel, program->tl[i],
                    false);
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
                    synth, event, 0x50 + row * 16 + i * 4 + channel,
                    rows[row][i], false);
            }
        }
        M_Output(synth, event, 0xb0 + channel, voice->algorithm, false);
        M_Output(
            synth, event, 0xb4 + channel, (voice->pan & 0x3f) | 0xc0, false);
    } else if ((event->channel & 0x40) != 0) {
        voice->program ^= 0xc0;
        FAIL_IF(
            (voice->program & 0xc0) == 0,
            "MDRV external SSG PCM is unsupported");
        const bool effect = event->effect_number != 0;
        const size_t count = effect
            ? synth->effects->ssg_count
            : synth->songs[synth->song].asset->ssg_count;
        FAIL_IF(number >= count, "MDRV SSG program outside bank");
        voice->envelope_stage = 0;
        *M_Pattern(synth, event) = effect
            ? synth->effect_programs[number]
            : synth->songs[synth->song].ssg[number];
        voice->noise_mode = (voice->noise_mode & 0x3f) | (encoded & 0xc0);
        M_Mixer(synth, event, voice);
    }
    return OK;
}

static RESULT M_NotePitch(
    MDRV_SYNTH *const synth, const MDRV_EVENT *const event,
    M_VOICE *const voice, const uint8_t note, const bool portamento)
{
    int key = note & 15;
    uint8_t octave = note >> 4;
    if (!portamento || (event->channel & 0x40) != 0) {
        key += voice->transpose;
        if (key < 0) {
            key += 12;
            octave--;
        } else if (key >= 12) {
            key -= 12;
            octave++;
        }
    }
    FAIL_IF(key < 0 || key >= 12, "MDRV transposed pitch outside table");
    if ((event->channel & 0x80) != 0
        || (portamento && event->channel == 0x10)) {
        voice->pitch =
            (uint16_t)(m_FmPitch[key] + ((octave & 15) << 11) + voice->detune);
    } else if ((event->channel & 0x40) != 0) {
        FAIL_IF(
            (voice->program & 0xc0) == 0,
            "MDRV external SSG PCM is unsupported");
        voice->pitch = m_SsgPitch[key] - voice->detune;
        voice->octave = octave;
    }
    M_Pitch(synth, event, voice);
    return OK;
}

static void M_KeyOff(
    MDRV_SYNTH *const synth, const MDRV_EVENT *const event,
    M_VOICE *const voice)
{
    if ((event->channel & 0x80) != 0) {
        const uint8_t channel =
            M_Channel(event) | (event->channel >= 0x83 ? 4 : 0);
        M_Output(synth, event, 0x28, channel, false);
    } else if ((event->channel & 0x40) != 0) {
        if ((voice->program & 0xc0) != 0) {
            M_EnvelopeStage(voice, M_Pattern(synth, event), 3);
        }
    } else {
        M_Output(synth, event, 0x10, (voice->program & 0x3f) | 0x80, false);
    }
}

static void M_EffectEnd(MDRV_SYNTH *const synth, const MDRV_EVENT *const event)
{
    const uint8_t channel = M_Channel(event);
    const uint8_t mask = (1 << channel) | (8 << channel);
    M_Output(
        synth, event, 7,
        (synth->registers[7] & ~mask) | (synth->music_mixer & mask), false);
    M_Output(synth, event, 6, synth->music_noise, false);
    M_Output(synth, event, 8 + channel, 0, false);
}

static void M_SetLfo(
    MDRV_SYNTH *const synth, const MDRV_EVENT *const event,
    M_VOICE *const voice, const bool volume)
{
    M_LFO *const lfo = volume ? &voice->volume_lfo : &voice->pitch_lfo;
    const uint8_t *const p = event->parameters;
    if (p[0] == 0) {
        lfo->enabled = false;
        if (volume) {
            voice->attenuation = 0;
        }
        return;
    }
    lfo->speed = p[0];
    lfo->depth = p[1];
    lfo->ramp = event->opcode == (volume ? 0xfc : 0xfb);
    if (lfo->ramp) {
        lfo->depth = (p[1] >> 1) | (p[1] << 7);
        if (volume) {
            lfo->depth_work = lfo->depth;
        }
    }
    lfo->step = p[2];
    if (!volume && (event->channel & 0x40) == 0
        && (synth->songs[synth->song].asset->hardware & 0xff) == 0) {
        lfo->step >>= 2;
    }
    lfo->delay = p[3] + 1;
    if (volume && lfo->ramp && lfo->delay == 0) {
        voice->attenuation = (lfo->depth & 0x80) != 0 ? 0 : 127;
        lfo->phase = 0;
    }
    lfo->enabled = true;
}

static RESULT M_Handle(MDRV_SYNTH *const synth, const MDRV_EVENT *const event)
{
    if (event->kind == MDRV_EVENT_DRIVER_RESET) {
        M_ResetControls(synth);
        if (event->parameters[0] == 0) {
            M_Silence(synth);
            M_Setup(synth);
            M_Silence(synth);
        }
        return OK;
    }
    if (event->track == UINT16_MAX || event->channel == 0) {
        return OK;
    }
    M_VOICE *const voice = M_Voice(synth, event);
    const uint8_t *const p = event->parameters;
    const uint8_t channel = M_Channel(event);
    switch (event->kind) {
    case MDRV_EVENT_CLOCK:
        M_Clock(synth, event, voice);
        break;
    case MDRV_EVENT_NOTE:
    case MDRV_EVENT_PORTAMENTO: {
        const bool portamento = event->kind == MDRV_EVENT_PORTAMENTO;
        const uint8_t note = portamento ? p[0] : event->opcode;
        if (event->channel != 0x10 || portamento) {
            MUST(M_NotePitch(synth, event, voice, note, portamento));
        }
        if (!event->legato) {
            if ((event->channel & 0x80) != 0
                || (portamento && event->channel == 0x10)) {
                M_Output(
                    synth, event, 0x28,
                    voice->mask | channel | (event->channel >= 0x83 ? 4 : 0),
                    false);
                if ((voice->hardware_lfo & 8) != 0) {
                    M_Output(
                        synth, event, 0x22, voice->hardware_lfo & ~8, false);
                    M_Output(
                        synth, event, 0x22, voice->hardware_lfo | 8, false);
                }
            } else if ((event->channel & 0x40) != 0) {
                const M_SSG_PROGRAM *const pattern = M_Pattern(synth, event);
                voice->envelope = true;
                voice->envelope_level = pattern->initial;
                M_EnvelopeStage(voice, pattern, 0);
            } else {
                M_Output(synth, event, 0x10, voice->program & 0x3f, false);
            }
        }
        voice->pitch_lfo.delay_work = voice->pitch_lfo.delay;
        voice->volume_lfo.delay_work = voice->volume_lfo.delay;
        voice->portamento = portamento ? p[2] | ((uint16_t)p[3] << 8) : 0;
        voice->portamento_enabled = portamento;
        if (portamento && (event->channel & 0x40) == 0
            && (synth->songs[synth->song].asset->hardware & 0xff) == 0) {
            const int32_t delta = voice->portamento >= 0x8000
                ? (int32_t)voice->portamento - 65536
                : voice->portamento;
            voice->portamento = delta >= 0 ? delta / 4 : -((-delta + 3) / 4);
        }
        M_Tail(synth, event, voice);
        break;
    }
    case MDRV_EVENT_REST:
        voice->portamento = 0;
        voice->portamento_enabled = false;
        M_Tail(synth, event, voice);
        break;
    case MDRV_EVENT_KEY_OFF:
        if (event->effect_number == 0 || event->opcode != 0xff) {
            M_KeyOff(synth, event, voice);
        }
        break;
    case MDRV_EVENT_PROGRAM:
        MUST(M_Program(synth, event, voice));
        break;
    case MDRV_EVENT_VOLUME:
        if (event->channel == 0x10 && event->opcode == 0xec) {
            if ((p[0] & 0x80) == 0) {
                voice->volume = p[0];
                M_Volume(synth, event, voice);
                for (uint8_t i = 0; i < 6; i++) {
                    synth->rhythm[i] = (synth->rhythm[i] & 0xc0) | p[i + 1];
                    M_Output(synth, event, 0x18 + i, synth->rhythm[i], false);
                }
            } else {
                const uint8_t index = p[0] & 0x7f;
                FAIL_IF(index >= 6, "MDRV rhythm level outside bank");
                synth->rhythm[index] = (synth->rhythm[index] & 0xc0) | p[1];
                M_Output(
                    synth, event, 0x18 + index, synth->rhythm[index], false);
            }
        } else {
            voice->volume = event->opcode == 0xf4 ? voice->volume + p[0]
                : event->opcode == 0xf5           ? voice->volume - p[0]
                                                  : p[0];
            if ((event->channel & 0x40) == 0) {
                M_Volume(synth, event, voice);
            }
        }
        break;
    case MDRV_EVENT_DETUNE:
        voice->detune = (int8_t)p[0];
        break;
    case MDRV_EVENT_TRANSPOSE:
        voice->transpose = (int8_t)p[0];
        break;
    case MDRV_EVENT_PITCH_LFO:
    case MDRV_EVENT_VOLUME_LFO:
        M_SetLfo(synth, event, voice, event->kind == MDRV_EVENT_VOLUME_LFO);
        break;
    case MDRV_EVENT_HARDWARE_LFO:
        voice->hardware_lfo = p[0];
        if ((event->channel & 0x40) != 0) {
            voice->previous_volume = p[0];
        }
        M_Output(synth, event, 0x22, p[0], true);
        if (p[0] != 0) {
            voice->hardware_lfo |= (uint8_t)(p[1] << 3);
            voice->pan = (voice->pan & 0xc0) | p[2] | (uint8_t)(p[3] << 4);
            M_Output(synth, event, 0xb4 + channel, voice->pan, true);
        }
        break;
    case MDRV_EVENT_REGISTER:
        M_Output(synth, event, p[0], p[1], true);
        break;
    case MDRV_EVENT_TEMPO:
        synth->timer_b = p[0] < 17 ? 1 : (uint8_t)-(4340 / p[0]);
        M_Write(synth, 0x26, synth->timer_b);
        break;
    case MDRV_EVENT_DELAY_NOISE:
        if ((event->channel & 0x40) != 0) {
            voice->noise_mode = (voice->noise_mode & 0xc0) | p[0];
            M_Output(synth, event, 6, p[0], false);
            if (event->effect_number == 0) {
                synth->music_noise = p[0];
            }
        } else {
            voice->pitch_lfo.delay = p[0] + 1;
        }
        break;
    case MDRV_EVENT_PAN:
        if (event->channel == 0x10) {
            FAIL_IF(p[0] >= 6, "MDRV rhythm pan outside bank");
            synth->rhythm[p[0]] = (synth->rhythm[p[0]] & 0x3f) | M_Pan(p[1]);
            M_Output(synth, event, 0x18 + p[0], synth->rhythm[p[0]], false);
        } else if ((event->channel & 0x80) != 0) {
            voice->pan = (voice->pan & 0x3f) | M_Pan(p[0]);
            M_Output(synth, event, 0xb4 + channel, voice->pan, false);
        }
        break;
    case MDRV_EVENT_FADE:
        MdrvSynth_Fade(synth, p[0]);
        break;
    case MDRV_EVENT_TRACK_END:
        voice->portamento_enabled = false;
        if (event->effect_number != 0) {
            M_EffectEnd(synth, event);
        }
        break;
    default:
        break;
    }
    return OK;
}

static void M_Event(void *const context, const MDRV_EVENT *const event)
{
    MDRV_SYNTH *const synth = context;
    if (!synth->ignore_events && IS_OK(synth->pending)) {
        synth->pending = M_Handle(synth, event);
    }
}

static void M_Timer(
    void *const context, OPNA *const chip, const unsigned timer,
    const uint64_t time)
{
    MDRV_SYNTH *const synth = context;
    const uint8_t status = Opna_ReadPort(chip, 0) & 3;
    if (status == 0) {
        return;
    }
    M_Write(synth, 0x27, 0x3f);
    if ((status & 2) == 0 || synth->failed || !IS_OK(synth->pending)) {
        return;
    }
    synth->pulses++;
    RESULT result =
        MdrvSeq_PulsePair(&synth->music_sequence, &synth->effect_sequence);
    if (!IS_OK(result)) {
        if (IS_OK(synth->pending)) {
            synth->pending = result;
        } else {
            Result_Ignore(result);
        }
    }
    if (!IS_OK(synth->pending)) {
        synth->failed = true;
        MdrvSynth_Stop(synth);
        return;
    }
    if (synth->fading && --synth->fade_counter == 0) {
        synth->fade_counter = synth->fade_interval;
        if (synth->fade_direction == 1) {
            if (++synth->fade_amount >= 127) {
                MdrvSynth_Stop(synth);
            }
        } else if (--synth->fade_amount == 0) {
            synth->fading = false;
        }
    }
}

static RESULT M_Play(
    MDRV_SYNTH *const synth, const size_t song, const bool clear_channels)
{
    FAIL_IF(song >= synth->song_count, "Invalid MDRV song index");
    Result_Ignore(synth->pending);
    synth->pending = OK;
    synth->failed = false;
    synth->song = song;
    const MDRV_MUSIC *const asset = synth->songs[song].asset;
    synth->ignore_events = true;
    MdrvSeq_Stop(&synth->effect_sequence);
    if (synth->effect_sequence.failed) {
        memcpy(
            synth->effect_sequence.work, synth->effects->bytes,
            synth->effects->size);
        synth->effect_sequence.failed = false;
    }
    MDRV_SEQUENCE *const sequence = &synth->music_sequence;
    sequence->music = asset;
    sequence->work_size = asset->size;
    sequence->commands_start = asset->commands_offset;
    sequence->commands_end = asset->fm_offset;
    RESULT result = MdrvSeq_Play(sequence);
    synth->ignore_events = false;
    MUST(result);
    if (clear_channels) {
        memset(synth->music, 0, sizeof(synth->music));
        memset(synth->effect, 0, sizeof(synth->effect));
    }
    M_ResetControls(synth);
    M_Silence(synth);
    M_Setup(synth);
    M_Write(synth, 0x26, synth->timer_b);
    return OK;
}

static void M_ReadSsg(READER *const reader, M_SSG_PROGRAM *const out)
{
    out->initial = Reader_ReadU8(reader);
    out->attack = Reader_ReadU8(reader);
    out->decay = Reader_ReadU8(reader);
    out->sustain = Reader_ReadU8(reader);
    out->sustain_rate = Reader_ReadU8(reader);
    out->release = Reader_ReadU8(reader);
}

RESULT MdrvSynth_Init(
    const MDRV_MUSIC *const songs, const size_t song_count,
    const MDRV_EFFECTS *const effects, const MDRV_SYNTH_CONFIG config,
    const ALLOCATOR *const allocator, const MDRV_REGISTER_CALLBACK callback,
    void *const context, MDRV_SYNTH **const out)
{
    *out = nullptr;
    FAIL_IF(
        config.fm_balance > 127 || config.rhythm_balance > 63,
        "Invalid MDRV balance configuration");
    FAIL_IF(song_count == 0 || song_count > 256, "Invalid MDRV song bank size");
    size_t max_size = 0;
    uint16_t max_tracks = 0;
    for (size_t i = 0; i < song_count; i++) {
        FAIL_IF(
            songs[i].size > 65536 || songs[i].track_count == 0,
            "Invalid MDRV song workspace");
        unsigned fm = 0, ssg = 0, rhythm = 0;
        for (uint16_t t = 0; t < songs[i].track_count; t++) {
            const uint8_t channel = songs[i].tracks[t].channel;
            fm += (channel & 0x80) != 0;
            ssg += (channel & 0x40) != 0;
            rhythm += channel == 0x10;
            FAIL_IF(
                fm > 6 || ssg > 3 || rhythm > 1,
                "MDRV song exceeds physical channel slots");
            FAIL_IF(
                (songs[i].tracks[t].channel & 0x20) != 0,
                "MDRV external PCM tracks are unsupported");
        }
        if (songs[i].size > max_size) {
            max_size = songs[i].size;
        }
        if (songs[i].track_count > max_tracks) {
            max_tracks = songs[i].track_count;
        }
    }
    MDRV_SYNTH *const synth = Allocator_AllocZ(allocator, sizeof(*synth));
    synth->songs =
        Allocator_AllocZ(allocator, song_count * sizeof(*synth->songs));
    synth->song_count = song_count;
    synth->config = config;
    synth->effects = effects;
    synth->callback = callback;
    synth->context = context;
    for (size_t i = 0; i < song_count; i++) {
        M_SONG *const song = &synth->songs[i];
        song->asset = &songs[i];
        song->fm =
            Allocator_AllocZ(allocator, songs[i].fm_count * sizeof(*song->fm));
        song->ssg = Allocator_AllocZ(
            allocator, songs[i].ssg_count * sizeof(*song->ssg));
        READER reader;
        Reader_OpenBuffer(&reader, songs[i].bytes, songs[i].size, nullptr);
        Reader_Seek(&reader, songs[i].fm_offset);
        for (uint16_t n = 0; n < songs[i].fm_count; n++) {
            M_FM_PROGRAM *const program = &song->fm[n];
            Reader_Skip(&reader, 3);
            program->mask = (Reader_ReadU8(&reader) & 0x78) << 1;
            program->algorithm = Reader_ReadU8(&reader);
            Reader_Skip(&reader, 1);
            uint8_t *const rows[] = { program->dt_mul, program->tl,
                                      program->ks_ar,  program->am_dr,
                                      program->sr,     program->sl_rr };
            for (size_t row = 0; row < 6; row++) {
                for (size_t op = 0; op < 4; op++) {
                    rows[row][op] = Reader_ReadU8(&reader);
                }
            }
            Reader_Skip(&reader, 2);
        }
        Reader_Seek(&reader, songs[i].ssg_offset);
        for (uint16_t n = 0; n < songs[i].ssg_count; n++) {
            M_ReadSsg(&reader, &song->ssg[n]);
        }
        MUST(Reader_Result(&reader));
    }
    synth->effect_programs = Allocator_AllocZ(
        allocator, effects->ssg_count * sizeof(*synth->effect_programs));
    READER reader;
    Reader_OpenBuffer(&reader, effects->bytes, effects->size, nullptr);
    Reader_Seek(&reader, effects->ssg_offset);
    for (uint16_t n = 0; n < effects->ssg_count; n++) {
        M_ReadSsg(&reader, &synth->effect_programs[n]);
    }
    MUST(Reader_Result(&reader));
    MDRV_MUSIC workspace = songs[0];
    workspace.size = max_size;
    workspace.track_count = max_tracks;
    MdrvSeq_InitMusic(
        &synth->music_sequence, &workspace, allocator, M_Event, synth);
    synth->music_sequence.music = &songs[0];
    MdrvSeq_InitEffects(
        &synth->effect_sequence, effects, allocator, M_Event, synth);
    synth->chip = Opna_Init(allocator, M_Timer, synth);
    MdrvSynth_Reset(synth);
    *out = synth;
    return OK;
}

void MdrvSynth_Reset(MDRV_SYNTH *const synth)
{
    Result_Ignore(synth->pending);
    synth->pending = OK;
    synth->failed = false;
    synth->ignore_events = true;
    MdrvSeq_Stop(&synth->music_sequence);
    MdrvSeq_Stop(&synth->effect_sequence);
    synth->ignore_events = false;
    synth->music_sequence.failed = false;
    synth->effect_sequence.failed = false;
    synth->music_sequence.sync = 0xff;
    synth->effect_sequence.sync = 0xff;
    synth->music_sequence.track_count = 0;
    synth->effect_sequence.track_count = 0;
    synth->music_sequence.pulse = 0;
    synth->effect_sequence.pulse = 0;
    synth->music_sequence.loops = 0;
    synth->effect_sequence.loops = 0;
    synth->effect_sequence.effect_number = 0;
    synth->effect_sequence.effect_counter = 0;
    memset(
        synth->music_sequence.tracks, 0,
        synth->music_sequence.capacity * sizeof(*synth->music_sequence.tracks));
    memset(
        synth->effect_sequence.tracks, 0,
        synth->effect_sequence.capacity
            * sizeof(*synth->effect_sequence.tracks));
    memcpy(
        synth->effect_sequence.work, synth->effects->bytes,
        synth->effects->size);
    memset(synth->music, 0, sizeof(synth->music));
    memset(synth->effect, 0, sizeof(synth->effect));
    memset(synth->registers, 0, sizeof(synth->registers));
    for (size_t i = 0; i < 6; i++) {
        synth->patterns[i] = m_DefaultEnvelope;
    }
    M_ResetControls(synth);
    synth->pulses = 0;
    synth->timer_b = 0xd0;
    Opna_Reset(synth->chip);
    M_Write(synth, 0x22, 0);
    M_Write(synth, 0x10, 0xbf);
    M_Write(synth, 0x29, 0x83);
    M_Write(synth, 0x24, 0);
    M_Write(synth, 0x25, 0);
    M_Write(synth, 0x26, 0xe0);
    M_Write(synth, 0x27, 0x3f);
    M_Silence(synth);
    M_Setup(synth);
}

RESULT MdrvSynth_Play(MDRV_SYNTH *const synth, const size_t song)
{
    return M_Play(synth, song, true);
}

RESULT MdrvSynth_RestartFadeIn(MDRV_SYNTH *const synth)
{
    synth->ignore_events = true;
    MdrvSeq_Stop(&synth->music_sequence);
    MdrvSeq_Stop(&synth->effect_sequence);
    synth->ignore_events = false;
    M_ResetControls(synth);
    M_Silence(synth);
    M_Setup(synth);
    MUST(M_Play(synth, synth->song, false));
    MdrvSynth_Fade(synth, 0x82);
    return OK;
}

RESULT MdrvSynth_EffectPlay(MDRV_SYNTH *const synth, const uint16_t effect)
{
    if (effect == 0) {
        return OK;
    }
    FAIL_IF(synth->failed, "MDRV producer is in failed state");
    synth->ignore_events = true;
    RESULT result = MdrvSeq_TriggerEffect(&synth->effect_sequence, effect);
    synth->ignore_events = false;
    return result;
}

void MdrvSynth_Stop(MDRV_SYNTH *const synth)
{
    synth->ignore_events = true;
    MdrvSeq_Stop(&synth->music_sequence);
    MdrvSeq_Stop(&synth->effect_sequence);
    synth->ignore_events = false;
    M_ResetControls(synth);
    M_Silence(synth);
    M_Setup(synth);
    M_Silence(synth);
}

void MdrvSynth_Fade(MDRV_SYNTH *const synth, const uint8_t interval)
{
    synth->fading = true;
    synth->fade_interval = interval & 0x7f;
    synth->fade_counter = synth->fade_interval;
    synth->fade_direction = interval < 128 ? 1 : 2;
    synth->fade_amount = interval < 128 ? 0 : 127;
}

RESULT MdrvSynth_Render(
    MDRV_SYNTH *const synth, MDRV_SYNTH_FRAME *const output,
    const size_t frames)
{
    FAIL_IF(synth->failed, "MDRV producer is in failed state");
    for (size_t i = 0; i < frames; i++) {
        OPNA_FRAME frame;
        Opna_Render(synth->chip, &frame, 1);
        if (output != nullptr) {
            output[i] = (MDRV_SYNTH_FRAME) {};
            if (!synth->failed) {
                for (size_t stereo = 0; stereo < 2; stereo++) {
                    output[i].music[stereo] = frame.fm_rhythm[stereo];
                    for (size_t channel = 0; channel < 3; channel++) {
                        bool effect = false;
                        for (size_t track = 0;
                             track < synth->effect_sequence.track_count;
                             track++) {
                            effect |=
                                synth->effect_sequence.tracks[track].active
                                && synth->effect_sequence.tracks[track].channel
                                    == 0x40 + channel;
                        }
                        if (effect) {
                            output[i].effects[stereo] += frame.ssg[channel];
                        } else {
                            output[i].music[stereo] += frame.ssg[channel];
                        }
                    }
                }
            }
        }
        if (synth->failed) {
            if (output != nullptr) {
                memset(output + i, 0, (frames - i) * sizeof(*output));
            }
            RESULT result = synth->pending;
            synth->pending = OK;
            return result;
        }
    }
    return OK;
}

MDRV_SYNTH_STATUS MdrvSynth_Status(const MDRV_SYNTH *const synth)
{
    uint8_t effect_mask = 0;
    for (size_t i = 0; i < synth->effect_sequence.track_count; i++) {
        if (synth->effect_sequence.tracks[i].active) {
            effect_mask |= 1 << (synth->effect_sequence.tracks[i].channel & 3);
        }
    }
    return (MDRV_SYNTH_STATUS) {
        .time = Opna_Time(synth->chip),
        .pulses = synth->pulses,
        .sync = synth->music_sequence.sync,
        .timer_b = synth->timer_b,
        .fade_attenuation = synth->fade_amount,
        .effect_mask = effect_mask,
        .music_playing = synth->music_sequence.playing,
        .effects_playing = synth->effect_sequence.playing,
        .fading = synth->fading,
        .failed = synth->failed,
    };
}
