#include "pc98/pmd/part.h"

static const uint16_t m_Periods[12] = {
    0xee8, 0xe12, 0xd48, 0xc89, 0xbd5, 0xb2b,
    0xa8a, 0x9f3, 0x964, 0x8dd, 0x85e, 0x7e6,
};

static uint16_t M_Word(const uint8_t *const bytes)
{
    return (uint16_t)(bytes[0] | (uint16_t)bytes[1] << 8);
}

static RESULT M_Pitch(
    PMD_PITCH *const pitch, const uint8_t note, const bool ssg,
    const bool keep_rest)
{
    if (!ssg) {
        return PmdPitch_Note(pitch, note, keep_rest);
    }
    if ((note & 15) == 15) {
        pitch->note = 255;
        if (!keep_rest) {
            pitch->frequency = 0;
        }
        return OK;
    }
    FAIL_IF((note & 15) >= 12, "Invalid SSG semitone");
    const uint8_t octave = note >> 4;
    const uint16_t base = m_Periods[note & 15];
    pitch->frequency =
        (uint16_t)((base >> octave)
                   + (octave != 0 ? (base >> (octave - 1)) & 1 : 0));
    pitch->note = note;
    return OK;
}

static uint8_t M_Begin(
    PMD_PART *const part, PMD_CONTROL *const control, uint8_t raw_note,
    const uint8_t timer_delta)
{
    if ((raw_note & 0x0f) == 0x0c) {
        raw_note = part->note.previous;
    }
    part->note.previous = raw_note;
    const bool rest = (raw_note & 0x0f) == 0x0f;
    if (!rest) {
        part->pitch.glide = 0;
    }
    for (unsigned i = 0; i < 2; i++) {
        const unsigned shift = i * 4;
        const uint8_t flags = (uint8_t)(part->lfo_flags >> shift);
        if ((flags & 3) == 0) {
            continue;
        }
        if (!rest && !control->tied && (flags & 4) == 0) {
            PmdLfo_Reset(&part->lfos[i]);
        }
        PmdLfo_Step(
            &part->lfos[i], (part->lfo_modes & (2 << shift)) != 0, timer_delta,
            &control->random_seed);
    }
    return raw_note;
}

static uint8_t M_Gate(
    const PMD_GATE *const rules, const uint8_t duration, const bool slur)
{
    if (slur) {
        return 0;
    }
    uint8_t gate = (uint8_t)(rules->fixed
                             + ((uint16_t)duration * rules->proportional >> 8));
    if (rules->minimum != 0) {
        if (duration < rules->minimum) {
            return 0;
        }
        const uint8_t limit = (uint8_t)(duration - rules->minimum);
        if (gate >= limit) {
            gate = limit;
        }
    }
    return gate;
}

RESULT PmdPart_Note(
    PMD_PART *const part, PMD_CONTROL *const control, const bool ssg,
    const uint8_t raw_note, const uint8_t duration, const bool slur,
    const bool glide, const uint8_t target_note, const uint8_t timer_delta)
{
    const uint8_t note = M_Begin(part, control, raw_note, timer_delta);
    const bool keep_rest = (part->lfo_flags & 0x11) != 0;
    MUST(M_Pitch(
        &part->pitch, PmdPitch_Transpose(note, part->transpose, part->master),
        ssg, keep_rest));
    part->note.gate = M_Gate(&part->gate, duration, slur);
    if (glide) {
        FAIL_IF(duration == 0, "Zero PMD portamento duration");
        PMD_PITCH target = part->pitch;
        MUST(M_Pitch(
            &target,
            PmdPitch_Transpose(target_note, part->transpose, part->master), ssg,
            keep_rest));
        if (ssg) {
            const int16_t difference =
                (int16_t)(uint16_t)(target.frequency - part->pitch.frequency);
            part->pitch.glide_step = (int16_t)(difference / duration);
            part->pitch.glide_remainder = (int16_t)(difference % duration);
        } else {
            MUST(PmdPitch_StartGlide(&part->pitch, target.frequency, duration));
        }
        part->lfo_flags |= 8;
    }
    if (part->one_volume != 0 && part->pitch.note != 255
        && --control->volume_push != 0) {
        part->one_volume = 0;
    }
    return OK;
}

uint8_t PmdPart_Advance(
    PMD_PART *const part, uint16_t *const random_seed,
    const uint8_t timer_delta)
{
    uint8_t changed = part->lfo_flags & 8;
    for (unsigned i = 0; i < 2; i++) {
        const unsigned shift = i * 4;
        const uint8_t flags = part->lfo_flags & (3 << shift);
        if (flags != 0
            && PmdLfo_Step(
                &part->lfos[i], (part->lfo_modes & (2 << shift)) != 0,
                timer_delta, random_seed)) {
            changed |= flags;
        }
    }
    if ((changed & 8) != 0) {
        PmdPitch_GlideStep(&part->pitch);
    }
    return changed;
}

bool PmdPart_Command(
    PMD_PART *const part, PMD_CONTROL *const control, const bool ssg,
    const uint8_t opcode, const uint8_t *const p)
{
    const unsigned index = opcode >= 0xb9 && opcode <= 0xbf ? 1 : 0;
    PMD_LFO *const lfo = &part->lfos[index];
    const uint8_t step = ssg ? 1 : 4;
    const uint8_t limit = ssg ? 16 : 128;
    switch (opcode) {
    case 0xfe:
        part->gate.fixed = p[0];
        break;
    case 0xfd:
        part->volume = p[0];
        break;
    case 0xfb:
        control->tied = true;
        break;
    case 0xfa:
        part->detune = (int16_t)M_Word(p);
        break;
    case 0xf5:
        part->transpose = (int8_t)p[0];
        break;
    case 0xf4:
    case 0xe3: {
        const uint8_t volume =
            (uint8_t)(part->volume + (opcode == 0xf4 ? step : p[0]));
        part->volume = volume < limit ? volume : limit - 1;
        break;
    }
    case 0xf3:
    case 0xe2: {
        const uint8_t amount = opcode == 0xf3 ? step : p[0];
        part->volume = part->volume >= amount ? part->volume - amount : 0;
        break;
    }
    case 0xf2:
    case 0xbf:
        PmdLfo_Configure(lfo, p);
        break;
    case 0xf1:
    case 0xbe: {
        const unsigned shift = index * 4;
        const uint8_t flags =
            PmdLfo_Switch(lfo, (uint8_t)(part->lfo_flags >> shift), p[0]);
        part->lfo_flags =
            (uint8_t)((part->lfo_flags & (index == 0 ? 0xf0 : 0x0f))
                      | (flags << shift));
        break;
    }
    case 0xe7:
        part->transpose = (int8_t)(uint8_t)(part->transpose + p[0]);
        break;
    case 0xde:
    case 0xdd: {
        uint8_t volume;
        if (opcode == 0xde) {
            volume = (uint8_t)(part->volume + p[0]);
            volume = volume < limit ? volume : limit - 1;
        } else {
            volume = part->volume >= p[0] ? part->volume - p[0] : 0;
        }
        part->one_volume = (uint8_t)(volume + 1);
        control->volume_push = 1;
        break;
    }
    case 0xd6:
    case 0xbd:
        lfo->depth_interval = lfo->depth_counter = p[0];
        lfo->depth = p[1];
        break;
    case 0xd5:
        part->detune = (int16_t)(uint16_t)(part->detune + M_Word(p));
        break;
    case 0xcb:
    case 0xbc:
        lfo->wave = p[0];
        break;
    case 0xca:
    case 0xbb: {
        const unsigned bit = 2 << (index * 4);
        part->lfo_modes =
            (uint8_t)((part->lfo_modes & ~bit) | ((p[0] & 1) != 0 ? bit : 0));
        break;
    }
    case 0xc4:
        part->gate.proportional = p[0];
        break;
    case 0xc2:
    case 0xb9:
        lfo->initial_delay = p[0];
        PmdLfo_Reset(lfo);
        break;
    case 0xb7: {
        PMD_LFO *const target = &part->lfos[(p[0] & 128) != 0 ? 1 : 0];
        const uint8_t count = p[0] & 127;
        target->depth_count = target->initial_depth_count =
            count != 0 ? count : 255;
        break;
    }
    case 0xb3:
        part->gate.minimum = p[0];
        break;
    case 0xb2:
        part->master = (int8_t)p[0];
        break;
    default:
        return false;
    }
    return true;
}

bool PmdPart_KeyOffDue(PMD_PART *const part, const uint8_t remaining)
{
    if ((part->note.key_flags & 3) != 0 || remaining > part->note.gate) {
        return false;
    }
    part->note.key_flags = 0xff;
    return true;
}

void PmdPart_Arm(PMD_PART *const part, const bool next_tie)
{
    part->note.key_flags = next_tie ? 2 : 0;
}
