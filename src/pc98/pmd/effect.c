#include "pc98/pmd/effect.h"

#include "pc98/pmd/data.h"

#include <string.h>

static int M_Parameters(const uint8_t opcode)
{
    switch (opcode) {
    case 0xff:
    case 0xfe:
    case 0xfd:
    case 0xf5:
    case 0xf1:
    case 0xec:
        return 1;
    case 0xfa:
    case 0xf9:
    case 0xf7:
    case 0xef:
        return 2;
    case 0xf8:
    case 0xf2:
        return 4;
    case 0xda:
        return 3;
    case 0xfb:
    case 0xf6:
    case 0xf4:
    case 0xf3:
    case 0xc1:
        return 0;
    default:
        return -1;
    }
}

static uint16_t M_Word(const uint8_t *const bytes)
{
    return (uint16_t)(bytes[0] | (uint16_t)bytes[1] << 8);
}

static void M_Modulation(
    const PMD_PART *const channel, const bool volume,
    PMD_FM_MODULATION modulation[2])
{
    for (unsigned i = 0; i < 2; i++) {
        modulation[i].amount = (int16_t)channel->lfos[i].value;
        modulation[i].slots = channel->volume_masks[i];
        const unsigned bit = (volume ? 2 : 1) << (i * 4);
        modulation[i].enabled = (channel->lfo_flags & bit) != 0;
    }
}

static void M_Volume(PMD_EFFECT_PLAYER *const player)
{
    PMD_FM_MODULATION modulation[2];
    M_Modulation(&player->channel, true, modulation);
    PmdFm_Volume(
        &player->channel.fm, player->channel.volume, modulation, player->chip);
}

static void M_Pitch(PMD_EFFECT_PLAYER *const player)
{
    PMD_FM_MODULATION modulation[2];
    M_Modulation(&player->channel, false, modulation);
    PmdPitch_Render(
        &player->channel.pitch, player->channel.fm.channel,
        player->channel.detune, modulation, player->chip);
}

static RESULT M_Program(PMD_EFFECT_PLAYER *const player, const uint8_t id)
{
    const size_t size = player->work_size;
    PMD_EFFECTS bank = {
        .bytes = player->work,
        .size = size,
        .instrument_offset = player->instrument_offset,
        .instrument_count = (uint16_t)((size - player->instrument_offset) / 26),
    };
    const uint8_t *const parameters = PmdData_Instrument(&bank, id);
    FAIL_IF(parameters == nullptr, "Missing EFC instrument %u", id);
    player->channel.program = id;
    PmdFm_Program(&player->channel.fm, parameters, player->chip);
    for (size_t i = 0; i < 2; i++) {
        if ((player->channel.volume_masks[i] & 0x0f) == 0) {
            player->channel.volume_masks[i] = player->channel.fm.carriers;
        }
    }
    return OK;
}

static RESULT M_Note(
    PMD_EFFECT_PLAYER *const player, const uint8_t opcode,
    const uint8_t *const parameters, const bool slur)
{
    PMD_PART *const channel = &player->channel;
    const bool glide = opcode == 0xda;
    MUST(PmdPart_Note(
        channel, &player->control, false, glide ? parameters[0] : opcode,
        glide ? parameters[2] : parameters[0], slur, glide, parameters[1],
        player->timer_delta));
    M_Volume(player);
    M_Pitch(player);
    if (channel->pitch.note != 0xff) {
        PmdFm_KeyOn(&channel->fm, player->chip);
    }
    const size_t position = player->position;
    FAIL_IF(position >= player->work_size, "EFC lookahead exceeds record");
    player->control.tied = false;
    PmdPart_Arm(channel, player->work[position] == 0xfb);
    channel->new_note = true;
    return OK;
}

static RESULT M_Command(
    PMD_EFFECT_PLAYER *const player, const uint8_t opcode,
    const uint8_t *const parameters, const bool slur)
{
    if (PmdPart_Command(
            &player->channel, &player->control, false, opcode, parameters)) {
        return OK;
    }
    switch (opcode) {
    case 0xff:
        return M_Program(player, parameters[0]);
    case 0xef:
        PmdChip_Write(
            player->chip,
            (player->channel.fm.channel / 3) * 256 + parameters[0],
            parameters[1]);
        break;
    case 0xec:
        if (player->channel.fm.channel >= 3) {
            PmdChip_Write(player->chip, 0x1b6, (parameters[0] & 3) << 6);
        }
        break;
    case 0xda:
        return M_Note(player, opcode, parameters, slur);
    default:
        break;
    }
    return OK;
}

static void M_Advance(PMD_EFFECT_PLAYER *const player, const bool fade_refresh)
{
    PMD_PART *const channel = &player->channel;
    const uint8_t changed = PmdPart_Advance(
        channel, &player->control.random_seed, player->timer_delta);
    if ((changed & 0x19) != 0) {
        M_Pitch(player);
    }
    if ((changed & 0x22) != 0 || fade_refresh) {
        M_Volume(player);
    }
}

static bool M_Slur(PMD_EFFECT_PLAYER *const player)
{
    if (player->position < player->work_size
        && player->work[player->position] == 0xc1) {
        player->position++;
        return true;
    }
    return false;
}

static RESULT M_Dispatch(PMD_EFFECT_PLAYER *const player)
{
    for (size_t count = 0; count < 4096; count++) {
        const size_t offset = player->position;
        FAIL_IF(offset >= player->work_size, "EFC stream exceeds record");
        const uint8_t opcode = player->work[offset];
        if (opcode == 0x80) {
            MUST(PmdPitch_Note(&player->channel.pitch, 0x0f, true));
            if (!player->repeat) {
                player->ended = true;
                return OK;
            }
            player->position = player->repeat_position;
            continue;
        }
        uint8_t parameters[4] = {};
        if (opcode < 0x80) {
            FAIL_IF(
                offset + 1 >= player->work_size, "Truncated EFC note duration");
            parameters[0] = player->work[offset + 1];
            player->remaining = parameters[0];
            player->position = (uint16_t)(offset + 2);
            const bool slur = M_Slur(player);
            return M_Note(player, opcode, parameters, slur);
        }
        const int count_parameters = M_Parameters(opcode);
        FAIL_IF(count_parameters < 0, "Unsupported EFC command %02X", opcode);
        FAIL_IF(
            (size_t)count_parameters > player->work_size - offset - 1,
            "Truncated EFC command %02X", opcode);
        memcpy(parameters, player->work + offset + 1, count_parameters);
        player->position = (uint16_t)(offset + 1 + count_parameters);
        uint16_t target = 0;
        uint16_t counter = 0;
        if (opcode == 0xf9 || opcode == 0xf7) {
            target = M_Word(parameters);
            counter = (uint16_t)(target + 1);
            FAIL_IF(
                target >= player->work_size || counter >= player->work_size,
                "EFC loop counter exceeds record");
        }
        FAIL_IF(
            opcode == 0xda && parameters[2] == 0,
            "Zero EFC portamento duration");
        const bool slur = opcode == 0xda && M_Slur(player);
        MUST(M_Command(player, opcode, parameters, slur));
        switch (opcode) {
        case 0xf9:
            player->work[counter] = 0;
            break;
        case 0xf8:
            if (parameters[0] != 0) {
                const uint16_t position = (uint16_t)(offset + 2);
                player->work[position]++;
                if (player->work[position] == parameters[0]) {
                    break;
                }
            }
            player->position = (uint16_t)(M_Word(parameters + 2) + 2);
            break;
        case 0xf7:
            if ((uint8_t)(player->work[target] - 1) == player->work[counter]) {
                player->position = (uint16_t)(target + 4);
            }
            break;
        case 0xf6:
            player->repeat = true;
            player->repeat_position = player->position;
            break;
        case 0xda:
            player->remaining = parameters[2];
            return OK;
        default:
            break;
        }
    }
    return FAIL("EFC dispatch limit exceeded");
}

RESULT PmdEffect_Init(
    PMD_EFFECT_PLAYER *const player, const PMD_EFFECTS *const bank,
    const ALLOCATOR *const allocator, PMD_CHIP *const chip,
    const uint8_t channel)
{
    memset(player, 0, sizeof(*player));
    player->id = 0xff;
    FAIL_IF(
        bank == nullptr || bank->bytes == nullptr || bank->size < 259
            || bank->size > 65536,
        "Missing compiled EFC bank");
    player->work_size = bank->size;
    player->work = Allocator_Alloc(allocator, bank->size);
    memcpy(player->work, bank->bytes, bank->size);
    player->chip = chip;
    PmdFm_Init(&player->channel.fm, channel);
    return OK;
}

RESULT PmdEffect_Play(PMD_EFFECT_PLAYER *const player, const uint8_t id)
{
    FAIL_IF(
        id >= PMD_EFFECT_COUNT || player->work == nullptr,
        "Invalid EFC effect ID");
    if (player->channel.fm.channel >= 3) {
        PmdChip_Write(player->chip, 0x1b6, 0xc0);
    }
    PmdEffect_Stop(player);
    player->position = M_Word(player->work + (size_t)id * 2);
    player->repeat_position = 0;
    player->remaining = 1;
    player->step = 0;
    player->repeat = false;
    player->ended = false;
    const uint8_t channel = player->channel.fm.channel;
    memset(&player->channel, 0, sizeof(player->channel));
    PmdFm_Init(&player->channel.fm, channel);
    player->channel.volume = 108;
    player->id = id;
    player->playing = true;
    player->failed = false;
    return OK;
}

void PmdEffect_Stop(PMD_EFFECT_PLAYER *const player)
{
    if (player->playing) {
        PmdFm_Silence(&player->channel.fm, player->chip);
    }
    player->playing = false;
    player->ended = true;
    player->id = 0xff;
}

RESULT PmdEffect_Step(
    PMD_EFFECT_PLAYER *const player, const uint8_t timer_delta,
    const bool fade_refresh)
{
    if (!player->playing) {
        return OK;
    }
    const size_t size = player->work_size;
    player->instrument_offset = M_Word(player->work + 254);
    if (player->instrument_offset > size - 2) {
        PmdEffect_Stop(player);
        player->failed = true;
        return FAIL("EFC instrument offset exceeds record");
    }
    player->timer_delta = timer_delta;
    player->channel.new_note = false;
    const uint8_t remaining = (uint8_t)(player->remaining - 1);
    if (PmdPart_KeyOffDue(&player->channel, remaining)
        && player->channel.pitch.note != 0xff) {
        PmdFm_KeyOff(&player->channel.fm, player->chip);
    }
    if (remaining == 0) {
        player->channel.lfo_flags &= 0xf7;
    }
    player->remaining--;
    RESULT result = OK;
    if (player->remaining == 0) {
        result = M_Dispatch(player);
    }
    player->step++;
    if (!IS_OK(result)) {
        PmdEffect_Stop(player);
        player->failed = true;
        return result;
    }
    if (!player->channel.new_note) {
        M_Advance(player, fade_refresh);
    }
    if (player->ended) {
        PmdEffect_Stop(player);
    }
    return OK;
}
