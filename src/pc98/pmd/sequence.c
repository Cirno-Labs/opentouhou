#include "pc98/pmd/data.h"
#include "pc98/pmd/driver.h"

#include <string.h>

static bool M_Fm(const uint8_t part)
{
    return part < 6 || part >= 11;
}

static uint8_t M_Byte(
    const MUSIC_SEQUENCE *const sequence, const uint16_t offset)
{
    return sequence->work[(size_t)offset + 1];
}

static uint16_t M_Word(const uint8_t *const bytes)
{
    return (uint16_t)(bytes[0] | (uint16_t)bytes[1] << 8);
}

static int M_Parameters(const uint8_t opcode)
{
    switch (opcode) {
    case 0xfb:
    case 0xf6:
    case 0xf4:
    case 0xf3:
    case 0xc1:
        return 0;
    case 0xfa:
    case 0xf9:
    case 0xf7:
    case 0xef:
    case 0xe5:
    case 0xd6:
    case 0xd5:
    case 0xc3:
    case 0xbd:
    case 0xb8:
    case 0xb5:
        return 2;
    case 0xda:
    case 0xc8:
    case 0xc7:
        return 3;
    case 0xf8:
    case 0xf2:
    case 0xf0:
    case 0xbf:
        return 4;
    case 0xcd:
        return 5;
    case 0xce:
    case 0xc6:
        return 6;
    case 0xb4:
        return 16;
    default:
        return opcode >= 0xb2 ? 1 : -1;
    }
}

static void M_Tempo(
    MUSIC_SEQUENCE *const sequence, const uint8_t *const parameters)
{
    const uint8_t kind = parameters[0];
    if (kind < 251 || kind == 254) {
        int value =
            kind < 251 ? kind : sequence->timer_b + (int8_t)parameters[1];
        value = value < 0 ? 0 : value > 250 ? 250 : value;
        sequence->timer_b = (uint8_t)value;
        const uint8_t divisor = (uint8_t)(0u - sequence->timer_b);
        sequence->tempo = divisor < 18
            ? 255
            : (uint8_t)(4396 / divisor + ((4396 % divisor) >= 128));
    } else {
        int value = kind == 255 ? parameters[1]
                                : sequence->tempo + (int8_t)parameters[1];
        value = value < 18 ? 18 : value > 255 ? 255 : value;
        sequence->tempo = (uint8_t)value;
        sequence->timer_b =
            (uint8_t)(0u - (4396 / value) - ((4396 % value) >= 128));
    }
}

static RESULT M_Position(
    const MUSIC_SEQUENCE *const sequence, const uint16_t position)
{
    FAIL_IF(
        position < 26 || position >= sequence->music->command_end,
        "PMD cursor outside commands");
    return OK;
}

static bool M_Slur(MUSIC_SEQUENCE *const sequence, MUSIC_PART *const part)
{
    if (part->mask == 0 && part->position < sequence->music->command_end
        && M_Byte(sequence, part->position) == 0xc1) {
        part->position++;
        return true;
    }
    return false;
}

static bool M_NextTie(
    const MUSIC_SEQUENCE *const sequence, const MUSIC_PART *const part)
{
    return part->position < sequence->music->command_end
        && M_Byte(sequence, part->position) == 0xfb;
}

static void M_ApplyDeferredMask(PMD *const pmd, const uint8_t index)
{
    const uint16_t bit = (uint16_t)(1u << index);
    if ((pmd->deferred_masks & bit) != 0) {
        MusicSeq_SetMask(
            &pmd->sequence, index, (pmd->sequence.parts[index].mask & 15) | 2);
        pmd->deferred_masks &= (uint16_t)~bit;
    }
}

static void M_LatchVoiceError(PMD *const pmd, const RESULT result)
{
    if (!IS_OK(result)) {
        pmd->error = result;
        pmd->failed = true;
    }
}

static void M_CheckNote(PMD *const pmd, const uint8_t opcode)
{
    MUSIC_PART *const part = &pmd->sequence.parts[8];
    if (!pmd->failed && (part->mask & 1) == 0 && pmd->drum.priority < 2
        && (opcode & 15) != 15) {
        if (pmd->drum.priority == 1) {
            MusicClock_StopSsgEffect(pmd);
        }
        MusicSeq_SetMask(&pmd->sequence, 8, part->mask & 13);
    }
}

static void M_VoiceCommand(
    PMD *const pmd, const uint8_t index, const uint8_t opcode,
    const uint8_t *const parameters, const int count, const bool slur)
{
    if (pmd->failed) {
        return;
    }
    MUSIC_SEQUENCE *const sequence = &pmd->sequence;
    MUSIC_PART *const part = &sequence->parts[index];
    if ((opcode == 0xc0 && parameters[0] < 2)
        || (opcode == 0xcf && (parameters[0] & 0xf0) != part->slots)) {
        M_ApplyDeferredMask(pmd, index);
    }
    RESULT result = OK;
    if (opcode == 0xd2) {
        pmd->control.fade.speed = parameters[0];
        pmd->control.fade.inline_requested = true;
        return;
    }
    if (opcode == 0xd3 || opcode == 0xd4) {
        if (part->mask == 0 && (pmd->deferred_masks & (1u << index)) == 0) {
            const uint8_t id = parameters[0];
            if (opcode == 0xd3) {
                result = id == 0 ? MusicClock_StopEffect(pmd)
                                 : MusicClock_PlayEffect(pmd, id);
            } else if (id == 0) {
                MusicClock_StopSsgEffect(pmd);
            } else {
                result = MusicClock_PlaySsgEffect(pmd, id);
            }
        }
        M_LatchVoiceError(pmd, result);
        return;
    }
    if (pmd->board == PMD_BOARD_86
        && ((opcode >= 0xe8 && opcode <= 0xeb) || opcode == 0xe5
            || opcode == 0xe6)) {
        M_LatchVoiceError(
            pmd, PmdRhythm_Command(&pmd->rhythm, opcode, parameters));
        return;
    }
    const bool ssg = index >= 6 && index <= 8;
    const bool attenuation = opcode == 0xc0;
    const bool ssg_attenuation =
        attenuation && (parameters[0] == 253 || parameters[0] == 252);
    if ((!ssg && !ssg_attenuation) || (attenuation && parameters[0] >= 254)) {
        if (opcode == 0xda && count == 3) {
            result = MusicFm_Glide(
                &pmd->fm, index, part->mask, parameters[0], parameters[1],
                parameters[2], slur, M_NextTie(sequence, part),
                pmd->timer_delta);
        } else {
            result = MusicFm_Command(
                &pmd->fm, index, part->mask, sequence->music, opcode,
                parameters);
        }
    }
    if (IS_OK(result) && (ssg || ssg_attenuation)) {
        if (opcode == 0xda && count == 3) {
            result = MusicSsg_Glide(
                &pmd->ssg, index, part->mask, parameters[0], parameters[1],
                parameters[2], slur, M_NextTie(sequence, part),
                pmd->timer_delta);
        } else {
            MusicSsg_Command(&pmd->ssg, index, part->mask, opcode, parameters);
        }
    }
    M_LatchVoiceError(pmd, result);
}

static void M_Note(
    PMD *const pmd, const uint8_t index, const uint8_t note,
    const uint8_t duration, const bool slur)
{
    if (pmd->failed) {
        return;
    }
    const MUSIC_PART *const part = &pmd->sequence.parts[index];
    const bool next_tie = M_NextTie(&pmd->sequence, part);
    M_LatchVoiceError(
        pmd,
        index >= 6 && index <= 8
            ? MusicSsg_Note(
                  &pmd->ssg, index, part->mask, note, duration, slur, next_tie,
                  pmd->timer_delta)
            : MusicFm_Note(
                  &pmd->fm, index, part->mask, note, duration, slur, next_tie,
                  pmd->timer_delta));
}

static void M_Rhythm(PMD *const pmd, uint16_t mask)
{
    if (pmd->failed) {
        return;
    }
    pmd->control.tied = false;
    pmd->control.volume_push = 0;
    mask = pmd->sequence.parts[10].mask != 0 ? 0 : mask & 0x3fff;
    if (pmd->board == PMD_BOARD_86 && mask != 0) {
        PmdRhythm_Play(&pmd->rhythm, mask);
    }
    if (mask != 0 && pmd->control.fade.volume == 0) {
        uint8_t effect = 0;
        while ((mask & 1) == 0) {
            mask >>= 1;
            effect++;
        }
        M_LatchVoiceError(pmd, MusicClock_PlaySsgEffect(pmd, effect));
    }
}

static void M_Begin(PMD *const pmd, const uint8_t index)
{
    pmd->dispatching = true;
    pmd->dispatch_part = index;
    if (pmd->failed) {
        return;
    }
    MUSIC_PART *const part = &pmd->sequence.parts[index];
    if ((pmd->board == PMD_BOARD_86 ? index == 5 : (index >= 2 && index <= 5))
        && !pmd->fm.effect_active && (part->mask & 2) != 0
        && part->remaining == 1) {
        MusicSeq_SetMask(&pmd->sequence, index, part->mask & 13);
        if (index == (pmd->board == PMD_BOARD_86 ? 5 : 2)) {
            pmd->fm.channels[index].note.key_flags = 255;
        }
    }
    if (index >= 6 && index <= 8) {
        MusicSsg_Begin(&pmd->ssg, index, part->mask, part->remaining);
    } else {
        MusicFm_Begin(&pmd->fm, index, part->mask, part->remaining);
    }
}

static void M_Advance(PMD *const pmd, const uint8_t index)
{
    if (!pmd->failed) {
        const MUSIC_PART *const part = &pmd->sequence.parts[index];
        if (index >= 6 && index <= 8) {
            MusicSsg_Advance(&pmd->ssg, index, part->mask, pmd->timer_delta);
        } else {
            MusicFm_Advance(&pmd->fm, index, part->mask, pmd->timer_delta);
        }
    }
    M_ApplyDeferredMask(pmd, index);
    pmd->dispatching = false;
}

static RESULT M_Command(
    PMD *const pmd, const uint8_t index, const uint16_t offset,
    const uint8_t opcode, bool *const waits)
{
    MUSIC_SEQUENCE *const sequence = &pmd->sequence;
    MUSIC_PART *const part = &sequence->parts[index];
    uint8_t parameters[16] = {};
    bool slur = false;
    if (index == 8 && opcode == 0xda && (part->mask & 2) != 0) {
        M_CheckNote(pmd, opcode);
    }
    int count = M_Parameters(opcode);
    if (opcode == 0xda && (index == 10 || part->mask != 0)) {
        count = 1;
    }
    if (opcode == 0xfc || opcode == 0xc0) {
        MUST(M_Position(sequence, part->position));
        const uint8_t first = M_Byte(sequence, part->position);
        count = first >= (opcode == 0xfc ? 251 : 247) ? 2 : 1;
    }
    if (count < 0) {
        sequence->work[(size_t)offset + 1] = 0x80;
        part->position = offset;
        return OK;
    }
    FAIL_IF(
        (size_t)part->position + count > sequence->music->command_end,
        "Truncated PMD command");
    memcpy(parameters, sequence->work + part->position + 1, count);
    part->position = (uint16_t)(part->position + count);
    if (opcode == 0xda && count == 3) {
        FAIL_IF(parameters[2] == 0, "Zero PMD portamento duration");
        slur = M_Slur(sequence, part);
    }
    M_VoiceCommand(pmd, index, opcode, parameters, count, slur);
    uint16_t target;
    switch (opcode) {
    case 0xf9:
    case 0xf7:
        target = M_Word(parameters);
        MUST(M_Position(sequence, target));
        MUST(M_Position(sequence, (uint16_t)(target + 1)));
        if (opcode == 0xf9) {
            sequence->work[(size_t)target + 2] = 0;
        } else if (
            (uint8_t)(M_Byte(sequence, target) - 1)
            == M_Byte(sequence, (uint16_t)(target + 1))) {
            part->position = (uint16_t)(target + 4);
        }
        break;
    case 0xf8:
        if (parameters[0] != 0) {
            uint8_t *const counter = sequence->work + offset + 3;
            (*counter)++;
            if (*counter == parameters[0]) {
                break;
            }
        } else {
            part->loop_check = 1;
        }
        part->position = (uint16_t)(M_Word(parameters + 2) + 2);
        break;
    case 0xf6:
        part->repeat_position = part->position;
        break;
    case 0xfc:
        M_Tempo(sequence, parameters);
        break;
    case 0xdf:
        sequence->measure_length = parameters[0];
        break;
    case 0xdc:
        sequence->status = parameters[0];
        break;
    case 0xdb:
        sequence->status = (uint8_t)(sequence->status + parameters[0]);
        break;
    case 0xda:
        if (count == 3) {
            part->remaining = parameters[2];
            part->key_ons++;
            *waits = true;
        }
        break;
    case 0xcf:
        if (M_Fm(index)) {
            const uint8_t slots = parameters[0] & 0xf0;
            if (slots != part->slots) {
                part->slots = slots;
                part->mask = slots == 0 ? part->mask | 0x20 : part->mask & 0xdf;
            }
        }
        break;
    case 0xc6:
        if (M_Fm(index)) {
            for (uint8_t i = 0; i < 3; i++) {
                target = M_Word(parameters + i * 2);
                if (target != 0) {
                    MUST(M_Position(sequence, target));
                    MUSIC_PART *const extra =
                        &sequence
                             ->parts[(pmd->board == PMD_BOARD_86 ? 11 : 3) + i];
                    extra->position = target;
                    extra->remaining = 1;
                    extra->mask |= 0x20;
                    extra->active = true;
                }
            }
        }
        break;
    case 0xc0:
        if (parameters[0] < 2) {
            part->mask =
                parameters[0] != 0 ? part->mask | 0x40 : part->mask & 0xbf;
        } else if (count == 1) {
            part->position--;
            sequence->work[(size_t)part->position + 1] = 0x80;
        }
        break;
    default:
        break;
    }
    return OK;
}

static RESULT M_Dispatch(PMD *const pmd, const uint8_t index)
{
    MUSIC_SEQUENCE *const sequence = &pmd->sequence;
    MUSIC_PART *const part = &sequence->parts[index];
    for (size_t calls = 0; calls < 4096; calls++) {
        if (index == 10 && sequence->rhythm_position != 0) {
            const uint16_t offset = sequence->rhythm_position;
            MUST(M_Position(sequence, offset));
            const uint8_t byte = M_Byte(sequence, offset);
            if (byte == 0xff) {
                sequence->rhythm_position = 0;
                continue;
            }
            if ((byte & 0xc0) == 0xc0) {
                const uint16_t position = part->position;
                part->position = (uint16_t)(offset + 1);
                bool waits = false;
                const RESULT result =
                    M_Command(pmd, index, offset, byte, &waits);
                sequence->rhythm_position = part->position;
                part->position = position;
                MUST(result);
                continue;
            }
            const uint8_t count = (byte & 0x80) != 0 ? 2 : 1;
            FAIL_IF(
                (size_t)offset + count >= sequence->music->command_end,
                "Truncated PMD rhythm stroke");
            const uint16_t mask = (byte & 0x80) != 0
                ? (uint16_t)(((uint16_t)byte << 8
                              | M_Byte(sequence, (uint16_t)(offset + 1)))
                             & 0x3fff)
                : 0;
            sequence->rhythm_position = (uint16_t)(offset + count + 1);
            part->remaining = M_Byte(sequence, (uint16_t)(offset + count));
            part->key_ons++;
            M_Rhythm(pmd, mask);
            return OK;
        }
        MUST(M_Position(sequence, part->position));
        const uint16_t offset = part->position;
        const uint8_t opcode = M_Byte(sequence, offset);
        part->position++;
        if (opcode == 0x80) {
            part->position = offset;
            part->loop_check = 3;
            if (!pmd->failed) {
                if (index >= 6 && index <= 8) {
                    MusicSsg_End(&pmd->ssg, index);
                } else {
                    MusicFm_End(&pmd->fm, index);
                }
            }
            if (part->repeat_position == 0) {
                return OK;
            }
            part->position = part->repeat_position;
            part->loop_check = 1;
            continue;
        }
        if (opcode < 0x80) {
            if (index == 10) {
                FAIL_IF(
                    opcode >= sequence->music->rhythm_count,
                    "Invalid PMD R index");
                sequence->rhythm_position = sequence->music->rhythm[opcode];
                continue;
            }
            if (index == 8 && (part->mask & 2) != 0) {
                M_CheckNote(pmd, opcode);
            }
            MUST(M_Position(sequence, part->position));
            const uint8_t duration = M_Byte(sequence, part->position++);
            part->remaining = duration;
            const bool slur = M_Slur(sequence, part);
            part->key_ons++;
            M_Note(pmd, index, opcode, duration, slur);
            return OK;
        }
        bool waits = false;
        MUST(M_Command(pmd, index, offset, opcode, &waits));
        if (waits) {
            return OK;
        }
    }
    return FAIL("PMD dispatch limit exceeded");
}

void MusicSeq_Init(PMD *const pmd, const ALLOCATOR *const allocator)
{
    MUSIC_SEQUENCE *const sequence = &pmd->sequence;

    memset(sequence, 0, sizeof(*sequence));
    sequence->work = Allocator_AllocZ(allocator, MUSIC_WORK_SIZE);
    sequence->timer_b = 200;
    sequence->tempo = 78;
}

RESULT MusicSeq_Load(PMD *const pmd, const PMD_SONG *const music)
{
    MUSIC_SEQUENCE *const sequence = &pmd->sequence;
    FAIL_IF(
        sequence->work == nullptr || music == nullptr || music->bytes == nullptr
            || music->size > MUSIC_WORK_SIZE,
        "Invalid PMD load");
    memcpy(sequence->work, music->bytes, music->size);
    sequence->music = music;
    sequence->failed = false;
    return OK;
}

RESULT MusicSeq_Start(PMD *const pmd)
{
    MUSIC_SEQUENCE *const sequence = &pmd->sequence;
    FAIL_IF(
        sequence->music == nullptr || sequence->work == nullptr,
        "Missing PMD song");
    MusicClock_Silence(pmd);
    for (uint8_t i = 0; i < MUSIC_PART_COUNT; i++) {
        MUSIC_PART *const part = &sequence->parts[i];
        const uint8_t mask = part->mask & 15;
        const uint8_t key_ons = part->key_ons;
        memset(part, 0, sizeof(*part));
        part->mask = mask;
        part->key_ons = key_ons;
        if (i < PMD_TRACK_COUNT) {
            part->position = sequence->music->tracks[i];
            part->remaining = 1;
            part->active = M_Byte(sequence, part->position) != 0x80;
            if (M_Fm(i)) {
                if (i >= (pmd->board == PMD_BOARD_86 ? 6 : 3)) {
                    part->mask |= 0x20;
                } else {
                    part->slots = 0xf0;
                }
            }
        }
    }
    sequence->step = 0;
    sequence->measure = 0;
    sequence->measure_tick = 0;
    sequence->measure_length = 96;
    sequence->rhythm_position = 0;
    sequence->status = sequence->loops = 0;
    sequence->failed = false;
    sequence->playing = true;
    pmd->dispatching = false;
    pmd->deferred_masks = 0;
    pmd->timer_a = pmd->last_timer_a = pmd->timer_delta = 0;
    pmd->control.fade.volume = pmd->control.fade.speed = 0;
    pmd->control.fade.pending_stop = false;
    if (!pmd->failed) {
        MusicFm_Reset(&pmd->fm);
        MusicSsg_Reset(&pmd->ssg);
        MusicClock_StartPorts(pmd);
    }
    return OK;
}

void MusicSeq_Stop(PMD *const pmd)
{
    MUSIC_SEQUENCE *const sequence = &pmd->sequence;
    if (sequence->playing) {
        sequence->playing = false;
        sequence->loops = 255;
        MusicClock_Silence(pmd);
    }
}

void MusicSeq_SetMask(
    MUSIC_SEQUENCE *const sequence, const uint8_t part, const uint8_t mask)
{
    if (part < MUSIC_PART_COUNT) {
        sequence->parts[part].mask =
            (sequence->parts[part].mask & 0xf0) | (mask & 15);
    }
}

RESULT MusicSeq_Step(PMD *const pmd)
{
    MUSIC_SEQUENCE *const sequence = &pmd->sequence;
    if (!sequence->playing) {
        return OK;
    }
    static const uint8_t parts26[] = { 6, 7, 8, 0, 1, 2, 3, 4, 5, 10 };
    static const uint8_t parts86[] = {
        6, 7, 8, 3, 4, 5, 0, 1, 2, 11, 12, 13, 10
    };
    const uint8_t *const parts = pmd->board == PMD_BOARD_86 ? parts86 : parts26;
    const size_t part_count =
        pmd->board == PMD_BOARD_86 ? ARRAY_SIZE(parts86) : ARRAY_SIZE(parts26);
    uint8_t loops = 3;
    for (size_t i = 0; i < part_count; i++) {
        MUSIC_PART *const part = &sequence->parts[parts[i]];
        if (!part->active) {
            continue;
        }
        M_Begin(pmd, parts[i]);
        part->remaining--;
        if (part->remaining == 0) {
            const RESULT result = M_Dispatch(pmd, parts[i]);
            if (!IS_OK(result)) {
                sequence->failed = true;
                MusicSeq_Stop(pmd);
                return result;
            }
        }
        M_Advance(pmd, parts[i]);
        loops &= part->loop_check;
    }
    if (loops != 0) {
        for (size_t i = 0; i < PMD_TRACK_COUNT; i++) {
            if (sequence->parts[i].loop_check != 3) {
                sequence->parts[i].loop_check = 0;
            }
        }
        if (loops == 3) {
            sequence->loops = 255;
        } else {
            sequence->loops++;
            if (sequence->loops == 255) {
                sequence->loops = 1;
            }
        }
    }
    sequence->measure_tick++;
    if (sequence->measure_tick == sequence->measure_length) {
        sequence->measure_tick = 0;
        sequence->measure++;
    }
    sequence->step++;
    return OK;
}
