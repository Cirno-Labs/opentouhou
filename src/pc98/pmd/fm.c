#include "pc98/pmd/fm.h"

#include "pc98/pmd/data.h"

#include <string.h>

static const uint8_t m_Carriers[8] = { 0x80, 0x80, 0x80, 0x80,
                                       0xa0, 0xe0, 0xe0, 0xf0 };

static bool M_Ready(const PMD_PART *const channel, const uint8_t index)
{
    return !channel->partial_slots
        && (index != 2
            || !(
                ((channel->lfo_flags & 1) != 0
                 && (channel->volume_masks[0] & 15) != 0)
                || ((channel->lfo_flags & 16) != 0
                    && (channel->volume_masks[1] & 15) != 0)));
}

static void M_Modulation(
    const PMD_PART *const channel, const bool volume,
    PMD_FM_MODULATION modulation[2])
{
    for (unsigned i = 0; i < 2; i++) {
        modulation[i].amount = (int16_t)channel->lfos[i].value;
        modulation[i].slots = channel->volume_masks[i];
        modulation[i].enabled =
            (channel->lfo_flags & ((volume ? 2 : 1) << (i * 4))) != 0;
    }
}

static void M_Volume(const MUSIC_FM *const fm, const PMD_PART *const channel)
{
    PMD_FM_MODULATION modulation[2];
    M_Modulation(channel, true, modulation);
    uint8_t volume =
        channel->one_volume != 0 ? channel->one_volume - 1 : channel->volume;
    if (fm->volume_down != 0) {
        volume = (uint8_t)((uint16_t)(256 - fm->volume_down) * volume >> 8);
    }
    if (fm->control->fade.volume >= 2) {
        volume =
            (uint8_t)((uint16_t)(256 - (fm->control->fade.volume >> 1)) * volume
                      >> 8);
    }
    PmdFm_Volume(&channel->fm, volume, modulation, fm->chip);
}

static void M_Pitch(const MUSIC_FM *const fm, const PMD_PART *const channel)
{
    PMD_FM_MODULATION modulation[2];
    M_Modulation(channel, false, modulation);
    PmdPitch_Render(
        &channel->pitch, channel->fm.channel, channel->detune, modulation,
        fm->chip);
}

static RESULT M_Program(
    MUSIC_FM *const fm, PMD_PART *const channel, const PMD_SONG *const music,
    const uint8_t id, const bool muted)
{
    const uint8_t *const parameters = PmdData_SongInstrument(music, id);
    FAIL_IF(parameters == nullptr, "Missing PMD instrument %u", id);
    channel->program = id;
    PmdFm_Program(&channel->fm, parameters, muted ? nullptr : fm->chip);
    for (unsigned i = 0; i < 2; i++) {
        if ((channel->volume_masks[i] & 15) == 0) {
            channel->volume_masks[i] = channel->fm.carriers;
        }
    }
    return OK;
}

static RESULT M_Note(
    MUSIC_FM *const fm, PMD_PART *const channel, const uint8_t note,
    const uint8_t duration, const bool slur, const bool glide,
    const uint8_t target, const bool next_tie, const uint8_t timer_delta)
{
    MUST(PmdPart_Note(
        channel, fm->control, false, note, duration, slur, glide, target,
        timer_delta));
    M_Volume(fm, channel);
    M_Pitch(fm, channel);
    if (channel->pitch.note != 255) {
        PmdFm_KeyOn(&channel->fm, fm->chip);
    }
    PmdPart_Arm(channel, next_tie);
    channel->new_note = true;
    fm->control->tied = false;
    fm->control->volume_push = 0;
    return OK;
}

static void M_Advance(
    MUSIC_FM *const fm, PMD_PART *const channel, const uint8_t timer_delta)
{
    const uint8_t changed =
        PmdPart_Advance(channel, &fm->control->random_seed, timer_delta);
    if ((changed & 0x19) != 0) {
        M_Pitch(fm, channel);
    }
    if ((changed & 0x22) != 0 || fm->control->fade.speed != 0) {
        M_Volume(fm, channel);
    }
}

static RESULT M_Command(
    MUSIC_FM *const fm, PMD_PART *const channel, const PMD_SONG *const song,
    const uint8_t part, const uint8_t mask, const uint8_t opcode,
    const uint8_t *const p)
{
    const uint8_t index = opcode >= 0xb9 && opcode <= 0xbf ? 1 : 0;
    const bool muted = mask != 0 || !M_Ready(channel, part);
    if (PmdPart_Command(channel, fm->control, false, opcode, p)) {
        return OK;
    }
    switch (opcode) {
    case 0xff:
        return M_Program(fm, channel, song, p[0], muted);
    case 0xef:
        PmdChip_Write(fm->chip, (channel->fm.channel / 3) * 256 + p[0], p[1]);
        break;
    case 0xec:
        if (fm->board != PMD_BOARD_86) {
            break;
        }
        fm->pans[part] = (p[0] & 3) << 6;
        if (!muted) {
            PmdChip_Write(
                fm->chip,
                (channel->fm.channel / 3) * 256 + 0xb4
                    + channel->fm.channel % 3,
                (p[0] & 3) << 6);
        }
        break;
    case 0xcf:
        if ((p[0] & 15) != 0) {
            channel->fm.carriers = (uint8_t)(p[0] << 4);
        } else {
            channel->fm.carriers =
                m_Carriers[channel->fm.algorithm_feedback & 7];
        }
        channel->partial_slots = (p[0] & 0xf0) != 0xf0;
        if (channel->partial_slots) {
            PmdFm_Silence(&channel->fm, fm->chip);
        }
        break;
    case 0xc5:
    case 0xba: {
        const uint8_t slots = p[0] & 15;
        channel->volume_masks[index] =
            slots != 0 ? (uint8_t)(slots << 4 | 15) : channel->fm.carriers;
        break;
    }
    case 0xc0:
        if (p[0] == 1 && !muted) {
            PmdFm_Silence(&channel->fm, fm->chip);
        } else if (p[0] == 0 && mask == 0x40) {
            return M_Program(
                fm, channel, song, channel->program, !M_Ready(channel, part));
        }
        break;
    case 0xc8:
    case 0xc7:
        if (part == 2) {
            channel->partial_slots = true;
            PmdFm_Silence(&channel->fm, fm->chip);
        }
        break;
    default:
        break;
    }
    return OK;
}

static RESULT M_Restore(
    MUSIC_FM *const fm, const PMD_SONG *const song, const uint8_t *const work,
    const size_t work_size, const uint8_t part)
{
    PMD_PART *const channel = &fm->channels[part];
    const uint8_t *parameters = PmdData_SongInstrument(song, channel->program);
    if (parameters == nullptr && channel->program == 0) {
        const size_t offset =
            song->instrument_offset + (size_t)song->instrument_count * 26 + 2;
        FAIL_IF(
            offset + PMD_INSTRUMENT_SIZE > work_size,
            "PMD default restore instrument exceeds work area");
        // The driver finds ID zero at the bank terminator when an unused
        // music part has no program. Its parameters continue into resident
        // work, including retained bytes from a previously loaded song.
        parameters = work + offset;
    }
    FAIL_IF(
        parameters == nullptr, "Missing PMD restore instrument %u",
        channel->program);
    uint8_t levels[4];
    uint8_t restored[PMD_INSTRUMENT_SIZE];
    memcpy(levels, channel->fm.levels, sizeof(levels));
    memcpy(restored, parameters, sizeof(restored));
    restored[24] = channel->fm.algorithm_feedback;
    PmdFm_Program(&channel->fm, restored, fm->chip);
    memcpy(channel->fm.levels, levels, sizeof(levels));
    static const uint8_t order[4] = { 3, 1, 2, 0 };
    for (uint8_t slot = 0; slot < 4; slot++) {
        if ((channel->fm.carriers & (0x80 >> slot)) == 0) {
            const uint8_t index = order[slot];
            PmdChip_Write(
                fm->chip,
                (uint16_t)((part / 3) * 256 + 0x40 + part % 3 + index * 4),
                levels[index]);
        }
    }
    if (fm->board == PMD_BOARD_86) {
        PmdChip_Write(
            fm->chip, (part / 3) * 256 + 0xb4 + part % 3, fm->pans[part]);
    }
    return OK;
}

void MusicFm_Init(
    MUSIC_FM *const fm, PMD_CONTROL *const control, PMD_CHIP *const chip,
    const PMD_BOARD board)
{
    memset(fm, 0, sizeof(*fm));
    fm->board = board;
    fm->channel_count = board == PMD_BOARD_86 ? 6 : 3;
    fm->control = control;
    fm->chip = chip;
    for (uint8_t i = 0; i < fm->channel_count; i++) {
        PmdFm_Init(&fm->channels[i].fm, i);
    }
}

void MusicFm_Stop(MUSIC_FM *const fm)
{
    for (uint8_t slot = 0; slot < 4; slot++) {
        for (uint8_t channel = 0;
             channel < (fm->channel_count - (fm->effect_active ? 1 : 0));
             channel++) {
            PmdChip_Write(
                fm->chip,
                (uint16_t)((channel / 3) * 256 + 0x80 + slot * 4 + channel % 3),
                255);
        }
    }
    for (uint8_t channel = 0;
         channel < (fm->channel_count - (fm->effect_active ? 1 : 0));
         channel++) {
        PmdFm_KeyOff(&fm->channels[channel].fm, fm->chip);
    }
}

void MusicFm_Reset(MUSIC_FM *const fm)
{
    memset(fm->channels, 0, sizeof(fm->channels));
    fm->control->volume_push = 0;
    fm->volume_down = fm->board == PMD_BOARD_86 ? 0 : 16;
    memset(fm->pans, 0xc0, sizeof(fm->pans));
    fm->control->tied = false;
    for (uint8_t i = 0; i < fm->channel_count; i++) {
        PMD_PART *const channel = &fm->channels[i];
        PmdFm_Init(&channel->fm, i);
        PmdPitch_Init(&channel->pitch);
        channel->pitch.note = 255;
        channel->note.previous = 255;
        channel->note.key_flags = 255;
        channel->volume = 108;
        for (unsigned j = 0; j < 2; j++) {
            channel->lfos[j].depth_count = 255;
            channel->lfos[j].initial_depth_count = 255;
        }
    }
}

void MusicFm_Begin(
    MUSIC_FM *const fm, const uint8_t index, const uint8_t mask,
    const uint8_t remaining)
{
    if (index >= fm->channel_count) {
        return;
    }
    PMD_PART *const channel = &fm->channels[index];
    const bool muted = mask != 0 || !M_Ready(channel, index);
    channel->new_note = false;
    if (muted) {
        channel->note.key_flags = 255;
    }
    if (!muted && PmdPart_KeyOffDue(channel, (uint8_t)(remaining - 1))
        && channel->pitch.note != 255) {
        PmdFm_KeyOff(&channel->fm, fm->chip);
    }
    if (remaining == 1) {
        channel->lfo_flags &= 0xf7;
    }
}

void MusicFm_Advance(
    MUSIC_FM *const fm, const uint8_t index, const uint8_t mask,
    const uint8_t timer_delta)
{
    if (index < fm->channel_count) {
        PMD_PART *const channel = &fm->channels[index];
        if (mask == 0 && M_Ready(channel, index) && !channel->new_note) {
            M_Advance(fm, channel, timer_delta);
        }
    }
}

RESULT MusicFm_Note(
    MUSIC_FM *const fm, const uint8_t index, const uint8_t mask,
    const uint8_t note, const uint8_t duration, const bool slur,
    const bool next_tie, const uint8_t timer_delta)
{
    if (index >= fm->channel_count) {
        fm->control->tied = false;
        fm->control->volume_push = 0;
        return OK;
    }
    PMD_PART *const channel = &fm->channels[index];
    if (mask == 0 && M_Ready(channel, index)) {
        return M_Note(
            fm, channel, note, duration, slur, false, 0, next_tie, timer_delta);
    }
    channel->pitch.frequency = 0;
    channel->pitch.note = channel->note.previous = 255;
    channel->note.key_flags = 255;
    fm->control->tied = false;
    fm->control->volume_push--;
    if (fm->control->volume_push != 0) {
        channel->one_volume = 0;
    }
    fm->control->volume_push = 0;
    return OK;
}

RESULT MusicFm_Glide(
    MUSIC_FM *const fm, const uint8_t index, const uint8_t mask,
    const uint8_t note, const uint8_t target, const uint8_t duration,
    const bool slur, const bool next_tie, const uint8_t timer_delta)
{
    if (index >= fm->channel_count) {
        return OK;
    }
    PMD_PART *const channel = &fm->channels[index];
    if (mask == 0 && M_Ready(channel, index)) {
        MUST(M_Note(
            fm, channel, note, duration, slur, true, target, next_tie,
            timer_delta));
    }
    if (!M_Ready(channel, index) && channel->fm.key_slots != 0) {
        PmdFm_Silence(&channel->fm, fm->chip);
    }
    return OK;
}

void MusicFm_End(MUSIC_FM *const fm, const uint8_t index)
{
    if (index < fm->channel_count) {
        fm->channels[index].pitch.note = 255;
    }
}

RESULT MusicFm_Command(
    MUSIC_FM *const fm, const uint8_t index, const uint8_t mask,
    const PMD_SONG *const song, const uint8_t opcode, const uint8_t *const p)
{
    if (opcode == 0xc0 && p[0] >= 254) {
        if (p[0] == 255) {
            fm->volume_down = p[1];
        } else {
            const int value = p[1] == 0 ? (fm->board == PMD_BOARD_86 ? 0 : 16)
                                        : fm->volume_down + (int8_t)p[1];
            fm->volume_down = (uint8_t)(value < 0         ? 0
                                            : value > 255 ? 255
                                                          : value);
        }
        return OK;
    }
    if (index >= fm->channel_count) {
        if (opcode == 0xfb) {
            fm->control->tied = true;
        }
        return OK;
    }
    PMD_PART *const channel = &fm->channels[index];
    MUST(M_Command(fm, channel, song, index, mask, opcode, p));
    if (!M_Ready(channel, index) && channel->fm.key_slots != 0) {
        PmdFm_Silence(&channel->fm, fm->chip);
    }
    return OK;
}

RESULT MusicFm_Restore(
    MUSIC_FM *const fm, const PMD_SONG *const song, const uint8_t *const work,
    const size_t work_size)
{
    return M_Restore(fm, song, work, work_size, fm->channel_count - 1);
}
