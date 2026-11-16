#include "th01/mdrv_seq.h"

#include <string.h>

#define M_COMMAND_BUDGET 4096
#define M_NO_POSITION UINT32_MAX

static const uint8_t m_ChannelOrder[] = { 0x80, 0x20, 0x10, 0x40 };

static bool M_OutputEvent(const MDRV_EVENT_KIND kind)
{
    switch (kind) {
    case MDRV_EVENT_CLOCK:
    case MDRV_EVENT_NOTE:
    case MDRV_EVENT_REST:
    case MDRV_EVENT_KEY_OFF:
    case MDRV_EVENT_DETUNE:
    case MDRV_EVENT_TRANSPOSE:
    case MDRV_EVENT_GATE:
    case MDRV_EVENT_PROGRAM:
    case MDRV_EVENT_VOLUME:
    case MDRV_EVENT_PAN:
    case MDRV_EVENT_PITCH_LFO:
    case MDRV_EVENT_VOLUME_LFO:
    case MDRV_EVENT_HARDWARE_LFO:
    case MDRV_EVENT_DELAY_NOISE:
    case MDRV_EVENT_PORTAMENTO:
        return true;
    default:
        return false;
    }
}

static void M_Emit(
    MDRV_SEQUENCE *const sequence, const uint16_t track,
    const MDRV_EVENT_KIND kind, const uint32_t offset, const uint8_t opcode,
    const uint8_t *const parameters, const uint8_t count,
    const uint32_t duration, const bool legato)
{
    if (sequence->callback == nullptr) {
        return;
    }
    MDRV_EVENT event = {
        .kind = kind,
        .pulse = sequence->pulse,
        .offset = offset,
        .duration = duration,
        .track = track,
        .effect_number = sequence->effect_number,
        .source_channel = track < sequence->track_count
            ? sequence->tracks[track].source_channel
            : 0,
        .channel =
            track < sequence->track_count ? sequence->tracks[track].channel : 0,
        .opcode = opcode,
        .parameter_count = count,
        .legato = legato,
        .suppressed = track < sequence->track_count
            && sequence->tracks[track].suppressed && M_OutputEvent(kind),
    };
    if (count != 0) {
        memcpy(event.parameters, parameters, count);
    }
    sequence->callback(sequence->context, &event);
}

static void M_KeyOff(MDRV_SEQUENCE *const sequence, const uint16_t track)
{
    MDRV_CHANNEL_STATE *const state = &sequence->tracks[track];
    state->gate_remaining = 0;
    if (state->key_on) {
        state->key_on = false;
        M_Emit(
            sequence, track, MDRV_EVENT_KEY_OFF, state->position, 0, nullptr, 0,
            0, false);
    }
}

static void M_Init(
    MDRV_SEQUENCE *const sequence, const size_t size, const uint16_t capacity,
    const ALLOCATOR *const allocator, const MDRV_EVENT_CALLBACK callback,
    void *const context)
{
    *sequence = (MDRV_SEQUENCE) {
        .work_size = size,
        .capacity = capacity,
        .callback = callback,
        .context = context,
    };
    const size_t tracks_size = capacity * sizeof(*sequence->tracks);
    uint8_t *const block = Allocator_AllocZ(allocator, tracks_size + size);
    sequence->tracks = (MDRV_CHANNEL_STATE *)block;
    sequence->work = block + tracks_size;
}

static void M_ResetTrack(
    MDRV_CHANNEL_STATE *const state, const uint8_t channel,
    const uint32_t position)
{
    *state = (MDRV_CHANNEL_STATE) {
        .position = position,
        .channel = channel,
        .source_channel = channel,
        .active = channel != 0,
        .gate = 8,
        .pan = 3,
        .return_position = M_NO_POSITION,
        .repeat_one_end = M_NO_POSITION,
    };
}

static uint8_t M_PhysicalChannel(const uint8_t source, uint8_t counts[4])
{
    for (size_t group = 0; group < sizeof(m_ChannelOrder); group++) {
        if ((source & 0xf0) == m_ChannelOrder[group]) {
            return m_ChannelOrder[group] + counts[group]++;
        }
    }
    return 0;
}

static RESULT M_ValidateMusic(const MDRV_MUSIC *const music)
{
    uint8_t counts[4] = {};
    for (uint16_t i = 0; i < music->track_count; i++) {
        M_PhysicalChannel(music->tracks[i].channel, counts);
        FAIL_IF(
            counts[0] > 6 || counts[1] > 1 || counts[2] > 1 || counts[3] > 3,
            "MDRV music exceeds physical channel slots");
    }
    return OK;
}

static void M_Reset(MDRV_SEQUENCE *const sequence, const uint8_t *const bytes)
{
    memcpy(sequence->work, bytes, sequence->work_size);
    memset(sequence->tracks, 0, sequence->capacity * sizeof(*sequence->tracks));
    sequence->failed = false;
    sequence->playing = true;
    sequence->pulse = 0;
    sequence->loops = 0;
    sequence->sync = 0;
    sequence->tempo = 0;
}

static RESULT M_Range(
    const MDRV_SEQUENCE *const sequence, const uint32_t position,
    const uint32_t length)
{
    FAIL_IF(
        position < sequence->commands_start
            || position >= sequence->commands_end
            || length > sequence->commands_end - position,
        "MDRV command range outside stream: %u + %u", position, length);
    return OK;
}

static uint16_t M_Word(const uint8_t *const bytes)
{
    return bytes[0] | ((uint16_t)bytes[1] << 8);
}

static RESULT M_Branch(
    MDRV_SEQUENCE *const sequence, const uint32_t base,
    const uint8_t *const displacement, uint32_t *const out)
{
    const uint16_t word = M_Word(displacement);
    const int32_t relative = word >= 0x8000 ? (int32_t)word - 65536 : word;
    const int64_t target = (int64_t)base + relative;
    FAIL_IF(
        target < sequence->commands_start || target >= sequence->commands_end,
        "MDRV branch outside stream: %lld", (long long)target);
    *out = (uint32_t)target;
    return OK;
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

static bool M_Note(const uint8_t opcode)
{
    return opcode <= 0x8b && (opcode & 15) <= 11;
}

static RESULT M_Duration(
    MDRV_SEQUENCE *const sequence, const uint16_t track, const uint32_t offset,
    const uint8_t opcode, const uint8_t *const params, uint32_t *const budget)
{
    MDRV_CHANNEL_STATE *const state = &sequence->tracks[track];
    uint32_t duration = params[opcode == 0xf2 ? 1 : 0];
    bool slur = false;
    if (opcode != 0xf2 && opcode != 0x90) {
        while (state->position < sequence->commands_end
               && sequence->work[state->position] == 0x91) {
            FAIL_IF(*budget == 0, "MDRV tie chain exceeds pulse budget");
            (*budget)--;
            MUST(M_Range(sequence, state->position, 2));
            M_Emit(
                sequence, track, MDRV_EVENT_TIE, state->position, 0x91, nullptr,
                0, 0, false);
            state->position++;
            if (sequence->work[state->position] != opcode) {
                slur = true;
                break;
            }
            MUST(M_Range(sequence, state->position, 2));
            duration += sequence->work[state->position + 1];
            FAIL_IF(
                duration > UINT16_MAX, "MDRV tied duration exceeds work range");
            state->position += 2;
        }
    } else if (
        state->position < sequence->commands_end
        && sequence->work[state->position] == 0x91) {
        state->position++;
        slur = true;
        M_Emit(
            sequence, track, MDRV_EVENT_TIE, state->position - 1, 0x91, nullptr,
            0, 0, false);
    }
    const bool legato = state->legato;
    state->legato = slur;
    state->remaining = duration;
    state->gate_remaining = 0;
    if (opcode == 0x90) {
        M_Emit(
            sequence, track, MDRV_EVENT_REST, offset, opcode, params, 1,
            duration, legato);
    } else {
        state->note = opcode == 0xf2 ? params[0] : opcode;
        state->key_on = true;
        const uint32_t gate =
            opcode == 0xf2 ? duration : duration * state->gate / 8;
        M_Emit(
            sequence, track,
            opcode == 0xf2 ? MDRV_EVENT_PORTAMENTO : MDRV_EVENT_NOTE, offset,
            opcode, params, opcode == 0xf2 ? 4 : 1, duration, legato);
        if (!slur && gate != 0) {
            state->gate_remaining = gate - 1;
            if (state->gate_remaining == 0) {
                M_KeyOff(sequence, track);
            }
        }
    }
    return OK;
}

static RESULT M_Command(
    MDRV_SEQUENCE *const sequence, const uint16_t track, const uint32_t offset,
    const uint8_t opcode, const uint8_t *const params)
{
    MDRV_CHANNEL_STATE *const state = &sequence->tracks[track];
    MDRV_EVENT_KIND kind;
    uint8_t count = M_ParameterCount(opcode, state->channel);
    uint32_t target;
    switch (opcode) {
    case 0xe0:
        FAIL_IF(state->repeat_one, "Nested MDRV E0 repeat");
        state->repeat_one = true;
        state->repeat_one_start = state->position;
        state->repeat_one_end = M_NO_POSITION;
        state->repeat_one_count = params[0];
        kind = MDRV_EVENT_REPEAT;
        break;
    case 0xe1:
        FAIL_IF(!state->repeat_one, "MDRV E1 without E0");
        if (state->repeat_one_count == 1) {
            FAIL_IF(
                state->repeat_one_end == M_NO_POSITION,
                "MDRV E1 has no repeat end");
            state->position = state->repeat_one_end;
            state->repeat_one = false;
        }
        kind = MDRV_EVENT_REPEAT;
        break;
    case 0xe2:
        FAIL_IF(!state->repeat_one, "MDRV E2 without E0");
        state->repeat_one_count--;
        if (state->repeat_one_count != 0) {
            state->repeat_one_end = state->position;
            state->position = state->repeat_one_start;
        } else {
            state->repeat_one = false;
        }
        kind = MDRV_EVENT_REPEAT;
        break;
    case 0xe3:
        state->key_on = false;
        state->gate_remaining = 0;
        M_Emit(
            sequence, track, MDRV_EVENT_KEY_OFF, offset, opcode, nullptr, 0, 0,
            false);
        return OK;
    case 0xe4:
        FAIL_IF(state->repeat_two, "Nested MDRV E4 repeat");
        state->repeat_two = true;
        state->repeat_two_start = state->position;
        state->repeat_two_count = params[0];
        kind = MDRV_EVENT_REPEAT;
        break;
    case 0xe5:
        FAIL_IF(!state->repeat_two, "MDRV E5 without E4");
        if (state->repeat_two_count == 0 || --state->repeat_two_count != 0) {
            state->position = state->repeat_two_start;
        } else {
            state->repeat_two = false;
        }
        kind = MDRV_EVENT_REPEAT;
        break;
    case 0xe6:
        state->detune = (int8_t)params[0];
        kind = MDRV_EVENT_DETUNE;
        break;
    case 0xe7:
        state->transpose = (int8_t)params[0];
        kind = MDRV_EVENT_TRANSPOSE;
        break;
    case 0xe8:
    case 0xfc:
        kind = MDRV_EVENT_VOLUME_LFO;
        break;
    case 0xe9:
        sequence->tempo = params[0];
        kind = MDRV_EVENT_TEMPO;
        break;
    case 0xea:
        FAIL_IF(params[0] > 8, "Invalid MDRV gate");
        state->gate = params[0];
        kind = MDRV_EVENT_GATE;
        break;
    case 0xeb:
        state->program = params[0];
        if ((state->channel & 0x80) != 0) {
            state->pan = 3;
        }
        kind = MDRV_EVENT_PROGRAM;
        break;
    case 0xec:
        if (state->channel == 0x10) {
            count = (params[0] & 0x80) != 0 ? 2 : 7;
            if (count == 7) {
                state->volume = params[0];
            }
        } else {
            state->volume = params[0];
        }
        kind = MDRV_EVENT_VOLUME;
        break;
    case 0xed:
    case 0xfb:
        kind = MDRV_EVENT_PITCH_LFO;
        break;
    case 0xee:
        kind = MDRV_EVENT_REGISTER;
        break;
    case 0xef:
        kind = MDRV_EVENT_DELAY_NOISE;
        break;
    case 0xf0:
        kind = MDRV_EVENT_FADE;
        break;
    case 0xf1:
        if (state->channel != 0x10) {
            state->pan = params[0];
        }
        kind = MDRV_EVENT_PAN;
        break;
    case 0xf3:
        MUST(M_Branch(sequence, offset + 2, params, &target));
        state->position = target;
        state->loops++;
        sequence->loops++;
        sequence->sync = 0xfe;
        kind = MDRV_EVENT_LOOP;
        break;
    case 0xf4:
    case 0xf5:
        state->volume = opcode == 0xf4 ? state->volume + params[0]
                                       : state->volume - params[0];
        kind = MDRV_EVENT_VOLUME;
        break;
    case 0xf6:
        MUST(M_Branch(sequence, state->position, params + 1, &target));
        sequence->work[target] = params[0];
        kind = MDRV_EVENT_REPEAT;
        break;
    case 0xf7:
        target = offset + 1;
        if (sequence->work[target] != 0) {
            sequence->work[target]--;
            if (sequence->work[target] == 0) {
                kind = MDRV_EVENT_REPEAT;
                break;
            }
        }
        MUST(M_Branch(sequence, offset + 3, params + 1, &target));
        state->position = target;
        kind = MDRV_EVENT_REPEAT;
        break;
    case 0xf8:
        sequence->sync = params[0];
        kind = MDRV_EVENT_SYNC;
        break;
    case 0xf9:
        MUST(M_Branch(sequence, state->position, params, &target));
        if (sequence->work[target] == 1) {
            MUST(M_Range(sequence, target, 3));
            state->position = target + 3;
        }
        kind = MDRV_EVENT_REPEAT;
        break;
    case 0xfa:
        FAIL_IF(sequence->music == nullptr, "MDRV effect calls music pattern");
        FAIL_IF(
            state->return_position != M_NO_POSITION, "Nested MDRV user track");
        target = M_Word(params);
        MUST(M_Range(sequence, target, 1));
        state->return_position = state->position;
        state->position = target;
        kind = MDRV_EVENT_PATTERN;
        break;
    case 0xfd:
        kind = MDRV_EVENT_HARDWARE_LFO;
        break;
    case 0xfe:
        kind = MDRV_EVENT_EXTENDED;
        break;
    case 0xff:
        if (state->return_position != M_NO_POSITION) {
            state->position = state->return_position;
            state->return_position = M_NO_POSITION;
            kind = MDRV_EVENT_PATTERN;
        } else {
            if (sequence->effects != nullptr) {
                M_KeyOff(sequence, track);
            }
            state->active = false;
            kind = MDRV_EVENT_TRACK_END;
        }
        break;
    default:
        return FAIL("Unsupported MDRV opcode %02x at %u", opcode, offset);
    }
    M_Emit(sequence, track, kind, offset, opcode, params, count, 0, false);
    return OK;
}

static RESULT M_Advance(MDRV_SEQUENCE *const sequence, const uint16_t track)
{
    MDRV_CHANNEL_STATE *const state = &sequence->tracks[track];
    const uint8_t full = state->active;
    M_Emit(
        sequence, track, MDRV_EVENT_CLOCK, state->position, 0, &full, 1, 0,
        false);
    if (!state->active) {
        return OK;
    }
    if (state->gate_remaining != 0 && --state->gate_remaining == 0) {
        M_KeyOff(sequence, track);
    }
    if (state->remaining != 0 && --state->remaining != 0) {
        return OK;
    }
    uint32_t budget = M_COMMAND_BUDGET;
    while (state->active && state->remaining == 0) {
        FAIL_IF(budget == 0, "MDRV track %u exceeds pulse budget", track);
        budget--;
        const uint32_t offset = state->position;
        MUST(M_Range(sequence, offset, 1));
        const uint8_t opcode = sequence->work[offset];
        FAIL_IF(
            !M_Note(opcode) && opcode != 0x90 && opcode < 0xe0,
            "Invalid MDRV opcode %02x at %u", opcode, offset);
        uint8_t count =
            opcode < 0xe0 ? 1 : M_ParameterCount(opcode, state->channel);
        MUST(M_Range(sequence, offset, (uint32_t)count + 1));
        const uint8_t *const params = sequence->work + offset + 1;
        if (opcode == 0xec && state->channel == 0x10) {
            count = (params[0] & 0x80) != 0 ? 2 : 7;
            MUST(M_Range(sequence, offset, (uint32_t)count + 1));
        }
        // Copy parameters before a shared repeat counter can change them.
        uint8_t copy[7];
        memcpy(copy, params, count);
        state->position += (uint32_t)count + 1;
        if (opcode < 0xe0 || opcode == 0xf2) {
            FAIL_IF(
                opcode == 0xf2 && !M_Note(copy[0]),
                "Invalid MDRV portamento pitch");
            MUST(M_Duration(sequence, track, offset, opcode, copy, &budget));
        } else {
            MUST(M_Command(sequence, track, offset, opcode, copy));
        }
    }
    return OK;
}

static void M_Mask(
    MDRV_SEQUENCE *const music, const uint8_t channel, const bool suppressed,
    const bool active_only)
{
    for (uint16_t i = 0; i < music->track_count; i++) {
        MDRV_CHANNEL_STATE *const state = &music->tracks[i];
        if (state->channel != channel || (active_only && !state->active)) {
            continue;
        }
        if (!music->playing) {
            state->suppressed = suppressed;
        } else if (state->suppressed != suppressed) {
            state->suppressed = suppressed;
            const uint8_t parameter = suppressed;
            M_Emit(
                music, i, MDRV_EVENT_OUTPUT_MASK, state->position, 0,
                &parameter, 1, 0, false);
        }
    }
}

static uint16_t M_TrackIndex(
    const MDRV_SEQUENCE *const sequence, const uint16_t ordinal)
{
    return sequence->effects == nullptr ? ordinal
                                        : sequence->track_count - 1 - ordinal;
}

static RESULT M_AdvanceGroup(
    MDRV_SEQUENCE *const sequence, const uint8_t group, const uint8_t mask,
    const bool idle_ssg)
{
    if (!sequence->playing && !idle_ssg) {
        return OK;
    }
    for (uint16_t ordinal = 0; ordinal < sequence->track_count; ordinal++) {
        const uint16_t i = M_TrackIndex(sequence, ordinal);
        const MDRV_CHANNEL_STATE *const state = &sequence->tracks[i];
        const bool clock = state->active
            || (sequence->music != nullptr && (state->channel & 0xf0) == 0x40);
        if (clock && (state->channel & mask) == group) {
            MUST(M_Advance(sequence, i));
        }
    }
    return OK;
}

static bool M_AnyActive(const MDRV_SEQUENCE *const sequence)
{
    for (uint16_t i = 0; i < sequence->track_count; i++) {
        if (sequence->tracks[i].active) {
            return true;
        }
    }
    return false;
}

static bool M_MusicFinished(const MDRV_SEQUENCE *const sequence)
{
    return sequence->music != nullptr && sequence->playing
        && !M_AnyActive(sequence);
}

static void M_CompleteMusic(
    MDRV_SEQUENCE *const music, MDRV_SEQUENCE *const effects)
{
    const bool restart = (music->music->flags & 0x80) != 0;
    uint8_t counts[4] = {};
    for (uint16_t i = 0; i < music->track_count; i++) {
        const MDRV_TRACK *const track = &music->music->tracks[i];
        const uint8_t volume = music->tracks[i].volume;
        const uint8_t program = music->tracks[i].program;
        M_ResetTrack(
            &music->tracks[i], M_PhysicalChannel(track->channel, counts),
            track->offset);
        music->tracks[i].source_channel = track->channel;
        music->tracks[i].active &= restart;
        music->tracks[i].volume = volume;
        music->tracks[i].program = program;
    }
    music->sync = 0xff;
    music->playing = restart;
    bool stopped_effects = false;
    if (effects != nullptr) {
        stopped_effects = effects->playing;
        effects->playing = false;
        effects->tempo = music->tempo;
        effects->sync = music->sync;
        for (uint16_t i = 0; i < effects->capacity; i++) {
            const uint8_t source_channel = effects->tracks[i].source_channel;
            M_ResetTrack(&effects->tracks[i], 0x42 - i, M_NO_POSITION);
            effects->tracks[i].active = false;
            effects->tracks[i].source_channel = source_channel;
            for (uint16_t track = 0; track < music->track_count; track++) {
                if (music->tracks[track].channel
                    == effects->tracks[i].channel) {
                    effects->tracks[i].volume = music->tracks[track].volume;
                    effects->tracks[i].program = music->tracks[track].program;
                }
            }
        }
    }
    const uint8_t parameter = restart;
    M_Emit(
        music, UINT16_MAX, MDRV_EVENT_DRIVER_RESET, 0, 0xff, &parameter, 1, 0,
        false);
    if (stopped_effects) {
        M_Emit(
            effects, UINT16_MAX, MDRV_EVENT_STOP, 0, 0, nullptr, 0, 0, false);
        effects->pulse++;
    }
    if (restart) {
        music->loops++;
    }
    M_Emit(
        music, UINT16_MAX, restart ? MDRV_EVENT_LOOP : MDRV_EVENT_STOP, 0, 0xff,
        nullptr, 0, 0, false);
    music->pulse++;
}

static void M_Finish(MDRV_SEQUENCE *const sequence)
{
    if (!sequence->playing) {
        return;
    }
    if (!M_AnyActive(sequence)) {
        MdrvSeq_Stop(sequence);
    }
    sequence->pulse++;
}

static void M_Failed(MDRV_SEQUENCE *const sequence)
{
    MdrvSeq_Stop(sequence);
    sequence->failed = true;
}

void MdrvSeq_InitMusic(
    MDRV_SEQUENCE *const sequence, const MDRV_MUSIC *const music,
    const ALLOCATOR *const allocator, const MDRV_EVENT_CALLBACK callback,
    void *const context)
{
    M_Init(
        sequence, music->size, music->track_count, allocator, callback,
        context);
    sequence->music = music;
    sequence->commands_start = music->commands_offset;
    sequence->commands_end = music->fm_offset;
}

void MdrvSeq_InitEffects(
    MDRV_SEQUENCE *const sequence, const MDRV_EFFECTS *const effects,
    const ALLOCATOR *const allocator, const MDRV_EVENT_CALLBACK callback,
    void *const context)
{
    M_Init(
        sequence, effects->size, MDRV_EFFECT_MAX_CHANNELS, allocator, callback,
        context);
    sequence->effects = effects;
    sequence->commands_start = 16 + (uint32_t)effects->effect_count * 4;
    sequence->commands_end = effects->ssg_offset;
}

RESULT MdrvSeq_Play(MDRV_SEQUENCE *const sequence)
{
    FAIL_IF(sequence->music == nullptr, "MDRV sequence has no music");
    MUST(M_ValidateMusic(sequence->music));
    MdrvSeq_Stop(sequence);
    M_Reset(sequence, sequence->music->bytes);
    sequence->track_count = sequence->music->track_count;
    uint8_t counts[4] = {};
    for (uint16_t i = 0; i < sequence->track_count; i++) {
        const MDRV_TRACK *const track = &sequence->music->tracks[i];
        M_ResetTrack(
            &sequence->tracks[i], M_PhysicalChannel(track->channel, counts),
            track->offset);
        sequence->tracks[i].source_channel = track->channel;
    }
    M_Emit(sequence, UINT16_MAX, MDRV_EVENT_START, 0, 0, nullptr, 0, 0, false);
    return OK;
}

RESULT MdrvSeq_TriggerEffect(
    MDRV_SEQUENCE *const sequence, const uint16_t number)
{
    FAIL_IF(sequence->effects == nullptr, "MDRV sequence has no effects");
    if (number == 0) {
        return OK;
    }
    FAIL_IF(number > sequence->effects->effect_count, "Invalid MDRV effect");
    MdrvSeq_Stop(sequence);
    M_Reset(sequence, sequence->effects->bytes);
    sequence->effect_number = number;
    const MDRV_EFFECT *const effect = &sequence->effects->effects[number - 1];
    sequence->track_count = effect->channel_count;
    for (uint16_t i = 0; i < sequence->track_count; i++) {
        M_ResetTrack(&sequence->tracks[i], 0x42 - i, effect->stream_offsets[i]);
        sequence->tracks[i].source_channel = effect->base_channel + i;
    }
    M_Emit(sequence, UINT16_MAX, MDRV_EVENT_START, 0, 0, nullptr, 0, 0, false);
    return OK;
}

RESULT MdrvSeq_Pulse(MDRV_SEQUENCE *const sequence)
{
    FAIL_IF(sequence->failed, "MDRV sequence is in failed state");
    for (uint8_t channel = 0x40; channel <= 0x42; channel++) {
        M_Mask(sequence, channel, false, false);
    }
    for (size_t group = 0; group < sizeof(m_ChannelOrder); group++) {
        RESULT result =
            M_AdvanceGroup(sequence, m_ChannelOrder[group], 0xf0, false);
        if (!IS_OK(result)) {
            M_Failed(sequence);
            return result;
        }
        if (M_MusicFinished(sequence)) {
            M_CompleteMusic(sequence, nullptr);
            return OK;
        }
    }
    M_Finish(sequence);
    return OK;
}

RESULT MdrvSeq_PulsePair(
    MDRV_SEQUENCE *const music, MDRV_SEQUENCE *const effects)
{
    FAIL_IF(
        music == effects || music->music == nullptr
            || effects->effects == nullptr,
        "Invalid MDRV paired instances");
    FAIL_IF(
        music->failed || effects->failed,
        "MDRV paired sequence is in failed state");
    effects->tempo = music->tempo;
    effects->sync = music->sync;
    RESULT result = OK;
    for (size_t group = 0; group < 3; group++) {
        result = M_AdvanceGroup(music, m_ChannelOrder[group], 0xf0, false);
        if (!IS_OK(result)) {
            break;
        }
        if (M_MusicFinished(music)) {
            M_CompleteMusic(music, effects);
            return OK;
        }
    }
    effects->tempo = music->tempo;
    effects->sync = music->sync;
    for (uint8_t channel = 0x40; channel <= 0x42 && IS_OK(result); channel++) {
        bool owned = false;
        for (uint16_t i = 0; i < effects->track_count; i++) {
            owned |= effects->tracks[i].active
                && effects->tracks[i].channel == channel;
        }
        M_Mask(music, channel, owned, false);
        result = M_AdvanceGroup(effects, channel, 0xff, false);
        bool any_effect = false;
        for (uint16_t i = 0; i < effects->track_count; i++) {
            any_effect |= effects->tracks[i].active;
        }
        if (!any_effect) {
            M_Mask(music, channel, false, true);
        }
        music->tempo = effects->tempo;
        music->sync = effects->sync;
        if (IS_OK(result)) {
            result = M_AdvanceGroup(music, channel, 0xff, effects->playing);
            effects->tempo = music->tempo;
            effects->sync = music->sync;
            if (IS_OK(result) && M_MusicFinished(music)) {
                M_CompleteMusic(music, effects);
                return OK;
            }
        }
    }
    if (!IS_OK(result)) {
        M_Failed(music);
        M_Failed(effects);
        return result;
    }
    M_Finish(music);
    M_Finish(effects);
    effects->tempo = music->tempo;
    effects->sync = music->sync;
    return OK;
}

void MdrvSeq_Stop(MDRV_SEQUENCE *const sequence)
{
    if (!sequence->playing) {
        return;
    }
    for (size_t group = 0; group < sizeof(m_ChannelOrder); group++) {
        for (uint16_t ordinal = 0; ordinal < sequence->track_count; ordinal++) {
            const uint16_t i = M_TrackIndex(sequence, ordinal);
            MDRV_CHANNEL_STATE *const state = &sequence->tracks[i];
            if ((state->channel & 0xf0) == m_ChannelOrder[group]) {
                M_KeyOff(sequence, i);
                state->active = false;
                state->remaining = 0;
                state->legato = false;
            }
        }
    }
    sequence->playing = false;
    sequence->sync = 0xff;
    M_Emit(sequence, UINT16_MAX, MDRV_EVENT_STOP, 0, 0, nullptr, 0, 0, false);
}
