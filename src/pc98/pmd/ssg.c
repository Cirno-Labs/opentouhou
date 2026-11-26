#include "pc98/pmd/ssg.h"

#include <string.h>

static uint8_t M_Rate(
    const PMD_PART_ENVELOPE *const envelope, const unsigned index)
{
    uint8_t rate = (uint8_t)((index == 3 ? envelope->rates[index] * 2
                                         : envelope->rates[index])
                             - 16);
    if ((index == 1 || index == 2) && (rate & 128) != 0) {
        rate = (uint8_t)(rate * 2);
    }
    return rate;
}

static void M_ExtendedEnvelope(PMD_PART_ENVELOPE *const envelope)
{
    if (envelope->stage == 0) {
        return;
    }
    const unsigned index = envelope->stage < 4 ? envelope->stage - 1 : 3;
    const uint8_t count = envelope->counters[index];
    if ((int8_t)(uint8_t)(count - 1) < 0) {
        if (envelope->rates[index] != 0) {
            envelope->counters[index]++;
        }
        return;
    }
    if (index == 0) {
        envelope->level = (uint8_t)(envelope->level + count);
        if (envelope->level >= 15) {
            envelope->level = 15;
            envelope->stage = envelope->sustain == 15 ? 3 : 2;
            return;
        }
    } else {
        const bool borrow = envelope->level < count;
        envelope->level = (uint8_t)(envelope->level - count);
        if (index == 1 && (borrow || envelope->level < envelope->sustain)) {
            envelope->level = envelope->sustain;
            envelope->stage++;
            return;
        }
        if (borrow) {
            envelope->level = 0;
        }
    }
    envelope->counters[index] = M_Rate(envelope, index);
}

static bool M_EnvelopeTick(PMD_PART_ENVELOPE *const envelope)
{
    const uint8_t before = envelope->level;
    if (envelope->mode == 255) {
        M_ExtendedEnvelope(envelope);
    } else if (envelope->mode == 0) {
        envelope->rates[0]--;
        if (envelope->rates[0] == 0) {
            envelope->mode = 1;
            envelope->level = envelope->rates[1];
        }
    } else {
        const unsigned index = envelope->mode == 2 ? 3 : 2;
        if (envelope->rates[index] == 0) {
            if (index == 3) {
                envelope->level = (uint8_t)-15;
            }
        } else if (--envelope->rates[index] == 0) {
            envelope->rates[index] = envelope->counters[index];
            envelope->level--;
            if (envelope->level < 241 && envelope->level >= 15) {
                envelope->level = (uint8_t)-15;
            }
        }
    }
    return before != envelope->level;
}

static void M_EnvelopeBegin(PMD_PART_ENVELOPE *const envelope)
{
    if (envelope->mode == 255) {
        for (unsigned i = 0; i < 4; i++) {
            envelope->counters[i] = M_Rate(envelope, i);
        }
        envelope->level = envelope->start_level;
        envelope->stage = 1;
        M_ExtendedEnvelope(envelope);
    } else {
        envelope->mode = 0;
        envelope->level = 0;
        envelope->rates[0] = envelope->counters[0];
        if (envelope->rates[0] == 0) {
            envelope->mode = 1;
            envelope->level = envelope->rates[1];
        }
        envelope->rates[2] = envelope->counters[2];
        envelope->rates[3] = envelope->counters[3];
    }
}

static void M_Release(PMD_PART *const channel)
{
    if (channel->pitch.note != 255) {
        if (channel->envelope.mode == 255) {
            channel->envelope.stage = 4;
        } else {
            channel->envelope.mode = 2;
        }
    }
}

static void M_Volume(
    const MUSIC_SSG *const ssg, const PMD_PART *const channel,
    const uint8_t index)
{
    const PMD_PART_ENVELOPE *const envelope = &channel->envelope;
    if (envelope->mode == 3
        || (envelope->mode == 255 && envelope->stage == 0)) {
        return;
    }
    uint8_t volume =
        channel->one_volume != 0 ? channel->one_volume - 1 : channel->volume;
    if (ssg->volume_down != 0) {
        volume = (uint8_t)((uint16_t)(256 - ssg->volume_down) * volume >> 8);
    }
    if (ssg->control->fade.volume != 0) {
        volume = (uint8_t)((uint16_t)(256 - ssg->control->fade.volume) * volume
                           >> 8);
    }
    if (volume != 0) {
        if (envelope->mode == 255) {
            if (envelope->level == 0) {
                PmdChip_Write(ssg->chip, (uint8_t)(8 + index), 0);
                return;
            }
            const uint8_t product = (uint8_t)(volume * (envelope->level + 1));
            volume = (uint8_t)((product >> 4) + ((product & 8) != 0));
        } else {
            volume = (uint8_t)(volume + envelope->level);
            volume = (volume & 128) != 0 ? 0 : volume >= 16 ? 15 : volume;
            if (volume == 0) {
                PmdChip_Write(ssg->chip, (uint8_t)(8 + index), 0);
                return;
            }
        }
        uint16_t modulated = volume;
        if ((channel->lfo_flags & 2) != 0) {
            modulated = (uint16_t)(modulated + channel->lfos[0].value);
        }
        if ((channel->lfo_flags & 32) != 0) {
            modulated = (uint16_t)(modulated + channel->lfos[1].value);
        }
        if ((channel->lfo_flags & 0x22) != 0) {
            volume = (modulated & 0x8000) != 0 ? 0
                : modulated >= 16              ? 15
                                               : (uint8_t)modulated;
        }
    }
    PmdChip_Write(ssg->chip, (uint8_t)(8 + index), volume);
}

static uint16_t M_ExtendedDelta(const uint16_t period, const int16_t amount)
{
    if (amount == 0) {
        return period;
    }
    const uint32_t product = (uint32_t)((int32_t)(int16_t)period * amount) << 4;
    const int16_t high = (int16_t)(product >> 16);
    return (uint16_t)(period - (high + (high < 0 ? -1 : 1)));
}

static void M_Pitch(
    const MUSIC_SSG *const ssg, const PMD_PART *const channel,
    const uint8_t index)
{
    if (channel->pitch.frequency == 0) {
        return;
    }
    uint16_t period =
        (uint16_t)(channel->pitch.frequency + channel->pitch.glide);
    uint16_t lfo = 0;
    if ((channel->lfo_flags & 1) != 0) {
        lfo = channel->lfos[0].value;
    }
    if ((channel->lfo_flags & 16) != 0) {
        lfo = (uint16_t)(lfo + channel->lfos[1].value);
    }
    if ((channel->lfo_modes & 1) != 0) {
        period = M_ExtendedDelta(period, channel->detune);
        period = M_ExtendedDelta(period, (int16_t)lfo);
    } else {
        period = (uint16_t)(period - channel->detune - lfo);
    }
    if (period >= 4096) {
        period = (period & 0x8000) != 0 ? 0 : 4095;
    }
    PmdChip_Write(ssg->chip, (uint8_t)(index * 2), (uint8_t)period);
    PmdChip_Write(ssg->chip, (uint8_t)(index * 2 + 1), (uint8_t)(period >> 8));
}

static void M_KeyOn(
    MUSIC_SSG *const ssg, const PMD_PART *const channel, const uint8_t index)
{
    if (channel->pitch.note == 255) {
        return;
    }
    const uint8_t mask = (uint8_t)(9u << index);
    PmdChip_Write(
        ssg->chip, 7,
        (uint8_t)((ssg->chip->registers[7] | mask)
                  & ~(mask & channel->pattern)));
    if (ssg->noise != ssg->chip->last_noise && (ssg->effect_id & 128) != 0) {
        PmdChip_Write(ssg->chip, 6, ssg->noise);
        ssg->chip->last_noise = ssg->noise;
    }
}

static RESULT M_Note(
    MUSIC_SSG *const ssg, PMD_PART *const channel, const uint8_t index,
    const uint8_t note, const uint8_t duration, const bool slur,
    const bool glide, const uint8_t target, const bool next_tie,
    const uint8_t timer_delta)
{
    const uint8_t resolved = (note & 15) == 12 ? channel->note.previous : note;
    if ((resolved & 15) != 15 && !ssg->control->tied) {
        M_EnvelopeBegin(&channel->envelope);
    }
    MUST(PmdPart_Note(
        channel, ssg->control, true, note, duration, slur, glide, target,
        timer_delta));
    M_Volume(ssg, channel, index);
    M_Pitch(ssg, channel, index);
    M_KeyOn(ssg, channel, index);
    PmdPart_Arm(channel, next_tie);
    channel->new_note = true;
    ssg->control->tied = false;
    ssg->control->volume_push = 0;
    return OK;
}

static void M_Advance(
    MUSIC_SSG *const ssg, PMD_PART *const channel, const uint8_t index,
    const uint8_t timer_delta)
{
    const uint8_t changed =
        PmdPart_Advance(channel, &ssg->control->random_seed, timer_delta);
    if ((changed & 0x19) != 0) {
        M_Pitch(ssg, channel, index);
    }
    const unsigned count = (channel->lfo_modes & 4) != 0 ? timer_delta : 1;
    bool envelope_changed = false;
    for (unsigned i = 0; i < count; i++) {
        envelope_changed |= M_EnvelopeTick(&channel->envelope);
    }
    if (envelope_changed || (changed & 0x22) != 0
        || ssg->control->fade.speed != 0) {
        M_Volume(ssg, channel, index);
    }
}

static void M_Command(
    MUSIC_SSG *const ssg, PMD_PART *const channel, const uint8_t index,
    const uint8_t mask, const uint8_t opcode, const uint8_t *const p)
{
    if (PmdPart_Command(channel, ssg->control, true, opcode, p)) {
        return;
    }
    switch (opcode) {
    case 0xf0: {
        PMD_PART_ENVELOPE *const e = &channel->envelope;
        e->rates[0] = e->counters[0] = p[0];
        e->rates[1] = p[1];
        e->rates[2] = e->counters[2] = p[2];
        e->rates[3] = e->counters[3] = p[3];
        if (e->mode == 255) {
            e->mode = 2;
            e->level = (uint8_t)-15;
        }
        break;
    }
    case 0xef:
        PmdChip_Write(ssg->chip, p[0], p[1]);
        break;
    case 0xee:
        ssg->noise = p[0];
        break;
    case 0xed:
        channel->pattern = p[0];
        break;
    case 0xd4:
    case 0xd3:
    case 0xd2:
        break;
    case 0xd0: {
        const uint8_t noise = (uint8_t)(ssg->noise + p[0]);
        ssg->noise = (noise & 128) != 0 ? 0 : noise >= 32 ? 31 : noise;
        break;
    }
    case 0xcd: {
        PMD_PART_ENVELOPE *const e = &channel->envelope;
        for (unsigned i = 0; i < 3; i++) {
            e->rates[i] = p[i] & 31;
        }
        e->rates[3] = p[3] & 15;
        e->sustain = (uint8_t)(15 - (p[3] >> 4));
        e->start_level = p[4] & 15;
        if (e->mode != 255) {
            e->mode = 255;
            e->stage = 4;
            e->level = 0;
        }
        break;
    }
    case 0xcc:
        channel->lfo_modes =
            (uint8_t)((channel->lfo_modes & 0xfe) | (p[0] & 1));
        break;
    case 0xc9:
        channel->lfo_modes =
            (uint8_t)((channel->lfo_modes & 0xfb) | ((p[0] & 1) << 2));
        break;
    case 0xc0:
        if (p[0] == 1 && mask == 0) {
            PmdChip_Write(
                ssg->chip, 7,
                (uint8_t)(ssg->chip->registers[7] | (9u << index)));
        }
        break;
    default:
        break;
    }
    return;
}

void MusicSsg_Init(
    MUSIC_SSG *const ssg, PMD_CONTROL *const control, PMD_CHIP *const chip)
{
    memset(ssg, 0, sizeof(*ssg));
    ssg->effect_id = 255;
    ssg->control = control;
    ssg->chip = chip;
}

void MusicSsg_Stop(MUSIC_SSG *const ssg)
{
    PmdChip_Write(
        ssg->chip, 7,
        ssg->effect_active ? (uint8_t)((ssg->chip->registers[7] & 0x3f) | 0x9b)
                           : 0xbf);
}

void MusicSsg_Reset(MUSIC_SSG *const ssg)
{
    memset(ssg->channels, 0, sizeof(ssg->channels));
    ssg->noise = ssg->volume_down = 0;
    if (!ssg->effect_active) {
        ssg->chip->last_noise = 0;
    }
    ssg->control->tied = false;
    ssg->control->volume_push = 0;
    for (unsigned i = 0; i < 3; i++) {
        PMD_PART *const channel = &ssg->channels[i];
        channel->volume = 8;
        channel->pattern = 7;
        channel->envelope.mode = 3;
        channel->pitch.note = channel->note.previous = channel->note.key_flags =
            255;
        for (unsigned j = 0; j < 2; j++) {
            channel->lfos[j].depth_count =
                channel->lfos[j].initial_depth_count = 255;
        }
    }
}

void MusicSsg_Begin(
    MUSIC_SSG *const ssg, const uint8_t part, const uint8_t mask,
    const uint8_t remaining)
{
    if (part < 6 || part > 8) {
        return;
    }
    PMD_PART *const channel = &ssg->channels[part - 6];
    channel->new_note = false;
    if (mask != 0) {
        channel->note.key_flags = 255;
    } else if (PmdPart_KeyOffDue(channel, (uint8_t)(remaining - 1))) {
        M_Release(channel);
    }
    if (remaining == 1) {
        channel->lfo_flags &= 0xf7;
    }
}

void MusicSsg_Advance(
    MUSIC_SSG *const ssg, const uint8_t part, const uint8_t mask,
    const uint8_t timer_delta)
{
    if (part >= 6 && part <= 8) {
        PMD_PART *const channel = &ssg->channels[part - 6];
        if (mask == 0 && !channel->new_note) {
            M_Advance(ssg, channel, part - 6, timer_delta);
        }
    }
}

RESULT MusicSsg_Note(
    MUSIC_SSG *const ssg, const uint8_t part, const uint8_t mask,
    const uint8_t note, const uint8_t duration, const bool slur,
    const bool next_tie, const uint8_t timer_delta)
{
    if (part < 6 || part > 8) {
        return OK;
    }
    PMD_PART *const channel = &ssg->channels[part - 6];
    if (mask == 0) {
        return M_Note(
            ssg, channel, part - 6, note, duration, slur, false, 0, next_tie,
            timer_delta);
    }
    channel->pitch.frequency = 0;
    channel->pitch.note = channel->note.previous = 255;
    if (--ssg->control->volume_push != 0) {
        channel->one_volume = 0;
    }
    ssg->control->tied = false;
    ssg->control->volume_push = 0;
    return OK;
}

RESULT MusicSsg_Glide(
    MUSIC_SSG *const ssg, const uint8_t part, const uint8_t mask,
    const uint8_t note, const uint8_t target, const uint8_t duration,
    const bool slur, const bool next_tie, const uint8_t timer_delta)
{
    if (part < 6 || part > 8 || mask != 0) {
        return OK;
    }
    return M_Note(
        ssg, &ssg->channels[part - 6], part - 6, note, duration, slur, true,
        target, next_tie, timer_delta);
}

void MusicSsg_End(MUSIC_SSG *const ssg, const uint8_t part)
{
    if (part >= 6 && part <= 8) {
        ssg->channels[part - 6].pitch.note = 255;
    }
}

void MusicSsg_Command(
    MUSIC_SSG *const ssg, const uint8_t part, const uint8_t mask,
    const uint8_t opcode, const uint8_t *const p)
{
    if (opcode == 0xc0 && (p[0] == 253 || p[0] == 252)) {
        if (p[0] == 253) {
            ssg->volume_down = p[1];
        } else {
            const int value = p[1] == 0 ? 0 : ssg->volume_down + (int8_t)p[1];
            ssg->volume_down = (uint8_t)(value < 0         ? 0
                                             : value > 255 ? 255
                                                           : value);
        }
    } else if (part >= 6 && part <= 8) {
        M_Command(ssg, &ssg->channels[part - 6], part - 6, mask, opcode, p);
    }
}
