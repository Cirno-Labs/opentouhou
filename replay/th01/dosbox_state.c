#include "replay/th01/dosbox_state.h"

#include "replay/dosbox/dosbox.h"
#include "replay/harness/state.h"
#include "replay/th01/addresses.h"

#include "core/base/memory.h"

#include <string.h>

// The conventional memory that the resident block's search reads.
#define M_MEMORY_SIZE 0xa0000
// The five paragraphs of the resident block's MCB.
#define M_RESIDENT_PARAGRAPHS 5

// A pellet record: 42 bytes, of which the hash reads these.
#define M_PELLET_COUNT 100
#define M_PELLET_SIZE 42
#define M_PELLET_MOTION_TYPE 1
#define M_PELLET_LEFT 2
#define M_PELLET_TOP 4
#define M_PELLET_VELOCITY_X 16
#define M_PELLET_VELOCITY_Y 18
static_assert(M_PELLET_COUNT * M_PELLET_SIZE + 27 == 0x1083);

// The shots: arrays of eight 16-bit lefts, tops and a third field, then
// eight moving flags.
#define M_SHOT_COUNT 8
#define M_SHOT_TOP 16
#define M_SHOT_MOVING 48

// The cards: far pointers to their arrays, and the count.
typedef struct [[gnu::packed]] {
    uint16_t left[2];
    uint16_t top[2];
    uint16_t flag[2];
    int16_t count;
    uint16_t flip_frame[2];
    uint16_t hp[2];
} M_CARDS;
static_assert(sizeof(M_CARDS) == 0x16);

#define M_FIELD(name, type, offset) type name;
#define M_LOAD(name, type, offset)                                             \
    Dosbox_Read(segment, offset, &out->name, sizeof(out->name));

typedef struct {
    RETAIL_OP_DATA(M_FIELD)
} M_OP;
static void M_LoadOp(const uint16_t segment, M_OP *const out)
{
    RETAIL_OP_DATA(M_LOAD)
}

typedef struct {
    RETAIL_REIIDEN_DATA(M_FIELD)
} M_REIIDEN;
static void M_LoadReiiden(const uint16_t segment, M_REIIDEN *const out)
{
    RETAIL_REIIDEN_DATA(M_LOAD)
}

typedef struct {
    RETAIL_FUUIN_DATA(M_FIELD)
} M_FUUIN;
static void M_LoadFuuin(const uint16_t segment, M_FUUIN *const out) {
    RETAIL_FUUIN_DATA(M_LOAD)
}

#undef M_FIELD
#undef M_LOAD

#define M_OBJECT(name, offset, size) M_OBJECT_##name = offset,
enum {
    RETAIL_REIIDEN_OBJECTS(M_OBJECT)
};
#undef M_OBJECT

static uint32_t M_I32(const int32_t value)
{
    return (uint32_t)value;
}

static uint16_t M_Word(const uint8_t *const bytes, const size_t offset)
{
    return (uint16_t)(bytes[offset] | bytes[offset + 1] << 8);
}

static uint32_t M_Short(const uint8_t *const bytes, const size_t offset)
{
    return M_I32((int16_t)M_Word(bytes, offset));
}

static bool M_IsResident(const uint16_t segment, const RETAIL_RESIDENT *out)
{
    Dosbox_Read(segment, 0, (void *)out, sizeof(*out));
    if (memcmp(out->id, RETAIL_RESIDENT_ID, sizeof(out->id)) != 0) {
        return false;
    }
    uint8_t mcb[5];
    Dosbox_Read((uint16_t)(segment - 1), 0, mcb, sizeof(mcb));
    return (mcb[0] == 'M' || mcb[0] == 'Z')
        && M_Word(mcb, 3) == M_RESIDENT_PARAGRAPHS;
}

// Finds the resident block once by its id at the start of an MCB's data.
bool Th01Dosbox_Resident(TH01_DOSBOX *const th01, RETAIL_RESIDENT *const out)
{
    if (th01->resident != 0 && M_IsResident(th01->resident, out)) {
        return true;
    }
    th01->resident = 0;
    uint8_t *const memory = Mem_Alloc(M_MEMORY_SIZE);
    Dosbox_Read(0, 0, memory, M_MEMORY_SIZE);
    for (uint32_t at = 0x10; at + sizeof(*out) <= M_MEMORY_SIZE; at += 16) {
        if (memcmp(memory + at, RETAIL_RESIDENT_ID, sizeof(out->id)) == 0
            && M_IsResident((uint16_t)(at / 16), out)) {
            th01->resident = (uint16_t)(at / 16);
            break;
        }
    }
    Mem_Free(memory);
    return th01->resident != 0;
}

static void M_Double(const double value, uint32_t *const lo, uint32_t *hi)
{
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    *lo = (uint32_t)bits;
    *hi = (uint32_t)(bits >> 32);
}

static void M_Far(const uint16_t *const pointer, void *out, const size_t size)
{
    Dosbox_Read(pointer[1], pointer[0], out, size);
}

static uint32_t M_HashCards(
    TH01_DOSBOX *const th01, const uint16_t segment, uint32_t *const count)
{
    M_CARDS cards;
    Dosbox_Read(segment, M_OBJECT_cards, &cards, sizeof(cards));
    // CCards's constructor sets the count to 1 with no arrays, and the first
    // stageobjs_init_and_render sets it; until then there are no cards.
    if (cards.count != 1 || cards.flag[0] != 0 || cards.flag[1] != 0) {
        th01->cards_set = true;
    }
    // cards.free() at a stage's end and at Game Over clears the pointers;
    // the port keeps its cards, so the sample keeps the last ones.
    if (th01->cards_set && cards.count > 0 && cards.flag[0] == 0
        && cards.flag[1] == 0) {
        *count = th01->card_count;
        return th01->card_hash;
    }
    uint32_t hash = STATE_HASH_INIT;
    *count = th01->cards_set && cards.count > 0 ? (uint32_t)cards.count : 0;
    for (uint32_t i = 0; i < *count; i++) {
        const uint16_t flag_at[2] = { (uint16_t)(cards.flag[0] + i),
                                      cards.flag[1] };
        const uint16_t hp_at[2] = { (uint16_t)(cards.hp[0] + i), cards.hp[1] };
        const uint16_t frame_at[2] = { (uint16_t)(cards.flip_frame[0] + i * 2),
                                       cards.flip_frame[1] };
        uint8_t flag;
        uint8_t hp;
        int16_t frame;
        M_Far(flag_at, &flag, sizeof(flag));
        M_Far(hp_at, &hp, sizeof(hp));
        M_Far(frame_at, &frame, sizeof(frame));
        hash = State_Hash(hash, i);
        hash = State_Hash(hash, flag | (uint32_t)hp << 8);
        hash = State_Hash(hash, M_I32(frame));
    }
    th01->card_count = *count;
    th01->card_hash = hash;
    return hash;
}

static uint32_t M_HashPellets(const uint16_t segment, uint32_t *const count)
{
    uint8_t pellets[M_PELLET_COUNT * M_PELLET_SIZE];
    Dosbox_Read(segment, M_OBJECT_Pellets, pellets, sizeof(pellets));
    uint32_t hash = STATE_HASH_INIT;
    *count = 0;
    for (uint32_t i = 0; i < M_PELLET_COUNT; i++) {
        const uint8_t *const pellet = pellets + i * M_PELLET_SIZE;
        if (pellet[0] == 0) {
            continue;
        }
        (*count)++;
        hash = State_Hash(hash, i);
        hash = State_Hash(
            hash, pellet[0] | (uint32_t)pellet[M_PELLET_MOTION_TYPE] << 8);
        hash = State_Hash(hash, M_Short(pellet, M_PELLET_LEFT));
        hash = State_Hash(hash, M_Short(pellet, M_PELLET_TOP));
        hash = State_Hash(hash, M_Short(pellet, M_PELLET_VELOCITY_X));
        hash = State_Hash(hash, M_Short(pellet, M_PELLET_VELOCITY_Y));
    }
    return hash;
}

static uint32_t M_HashShots(const uint16_t segment, uint32_t *const count)
{
    uint8_t shots[M_SHOT_MOVING + M_SHOT_COUNT];
    Dosbox_Read(segment, M_OBJECT_Shots, shots, sizeof(shots));
    uint32_t hash = STATE_HASH_INIT;
    *count = 0;
    for (uint32_t i = 0; i < M_SHOT_COUNT; i++) {
        if (shots[M_SHOT_MOVING + i] == 0) {
            continue;
        }
        (*count)++;
        hash = State_Hash(hash, i);
        hash = State_Hash(hash, M_Short(shots, i * 2));
        hash = State_Hash(hash, M_Short(shots, M_SHOT_TOP + i * 2));
    }
    return hash;
}

static void M_Op(
    TH01_DOSBOX *const th01, const uint16_t segment, uint32_t *const row)
{
    M_OP op;
    M_LoadOp(segment, &op);
    row[TH01_COLUMN_vsync_count1] = M_I32(op.z_vsync_Count1);
    row[TH01_COLUMN_frame_rand] = op.frame_rand;
    row[TH01_COLUMN_rng] = op.random_seed;
    row[TH01_COLUMN_track] = (uint32_t)op.song_index;
    row[TH01_COLUMN_track_starts] = th01->track_starts;
    th01->choices[0] = (uint32_t)op.opts_rank;
    th01->choices[1] = (uint32_t)op.opts_credit_lives_extra;
    th01->choices[2] = (uint32_t)op.opts_credit_bombs;
    th01->choices[3] = (uint32_t)op.opts_bgm_mode;
}

static void M_Reiiden(
    TH01_DOSBOX *const th01, const uint16_t segment,
    const RETAIL_RESIDENT *const resident, uint32_t *const row)
{
    M_REIIDEN r;
    M_LoadReiiden(segment, &r);
    row[TH01_COLUMN_vsync_count1] = M_I32(r.z_vsync_Count1);
    row[TH01_COLUMN_frame_rand] = r.frame_rand;
    row[TH01_COLUMN_rng] = r.random_seed;
    row[TH01_COLUMN_game_frames] = r.frame_since_start_of_binary;
    row[TH01_COLUMN_stage] = resident->stage_id;
    row[TH01_COLUMN_route] = (uint32_t)(uint8_t)r.route;
    row[TH01_COLUMN_score] = r.score;
    row[TH01_COLUMN_lives] = M_I32(r.rem_lives);
    row[TH01_COLUMN_bombs] = M_I32(r.rem_bombs);
    row[TH01_COLUMN_pellet_speed] = M_I32(resident->pellet_speed);
    row[TH01_COLUMN_stage_time] = r.stage_timer;

    row[TH01_COLUMN_player_left] = M_I32(r.player_left);
    row[TH01_COLUMN_player_flags] = (uint32_t)(r.player_is_hit != 0)
        | (uint32_t)(r.player_invincible != 0) << 1
        | (uint32_t)(r.player_deflecting != 0) << 2
        | (uint32_t)(r.player_sliding != 0) << 3;
    row[TH01_COLUMN_invincibility_time] = M_I32(r.player_invincibility_time);
    row[TH01_COLUMN_bomb_frame] = r.bomb_frame;

    row[TH01_COLUMN_orb_left] = M_I32(r.orb_cur_left);
    row[TH01_COLUMN_orb_top] = M_I32(r.orb_cur_top);
    row[TH01_COLUMN_orb_velocity_x] = M_I32(r.orb_velocity_x);
    M_Double(
        r.orb_velocity_y, &row[TH01_COLUMN_orb_velocity_y_lo],
        &row[TH01_COLUMN_orb_velocity_y_hi]);
    M_Double(
        r.orb_force, &row[TH01_COLUMN_orb_force_lo],
        &row[TH01_COLUMN_orb_force_hi]);
    row[TH01_COLUMN_orb_force_frame] = M_I32(r.orb_force_frame);

    row[TH01_COLUMN_card_combo] = M_I32(r.cardcombo_cur);
    row[TH01_COLUMN_card_flip_cycle] = r.card_flip_cycle;
    uint32_t card_count;
    row[TH01_COLUMN_card_hash] = M_HashCards(th01, segment, &card_count);
    row[TH01_COLUMN_card_count] = card_count;

    row[TH01_COLUMN_boss] = (uint32_t)(uint8_t)r.boss_id;
    if (r.boss_id == 1) {
        row[TH01_COLUMN_boss_hp] = M_I32(r.singyoku_hp);
        row[TH01_COLUMN_boss_phase] = M_I32(r.singyoku_phase);
        row[TH01_COLUMN_boss_phase_frame] = M_I32(r.singyoku_phase_frame);
    } else if (r.boss_id != 0) {
        row[TH01_COLUMN_boss_hp] = M_I32(r.boss_hp);
        row[TH01_COLUMN_boss_phase] =
            M_I32(r.boss_id == 4 ? r.kikuri_phase : r.boss_phase);
        row[TH01_COLUMN_boss_phase_frame] = M_I32(r.boss_phase_frame);
    }

    uint32_t pellet_count;
    row[TH01_COLUMN_pellet_hash] = M_HashPellets(segment, &pellet_count);
    row[TH01_COLUMN_pellet_count] = pellet_count;
    uint32_t shot_count;
    row[TH01_COLUMN_shot_hash] = M_HashShots(segment, &shot_count);
    row[TH01_COLUMN_shot_count] = shot_count;
}

static void M_Fuuin(const uint16_t segment, uint32_t *const row)
{
    M_FUUIN f;
    M_LoadFuuin(segment, &f);
    row[TH01_COLUMN_vsync_count1] = M_I32(f.z_vsync_Count1);
    row[TH01_COLUMN_rng] = f.random_seed;
    row[TH01_COLUMN_route] = f.end_flag;
    row[TH01_COLUMN_score] = (uint32_t)f.score;
}

void Th01Dosbox_Sample(void *const context, uint32_t *const row)
{
    TH01_DOSBOX *const th01 = context;
    memset(row, 0, TH01_COLUMN_COUNT * sizeof(*row));
    row[TH01_COLUMN_buttons] = Dosbox_Buttons();
    const int32_t program = Dosbox_Program();
    row[TH01_COLUMN_program] = (uint32_t)(program + 1);
    const uint16_t load = Dosbox_LoadSegment();
    RETAIL_RESIDENT resident;
    const bool has_resident = program > RETAIL_PROGRAM_ZUNSOFT
        && Th01Dosbox_Resident(th01, &resident);
    if (program == RETAIL_PROGRAM_OP) {
        M_Op(th01, (uint16_t)(load + RETAIL_OP_DGROUP), row);
    } else if (program == RETAIL_PROGRAM_REIIDEN && has_resident) {
        M_Reiiden(
            th01, (uint16_t)(load + RETAIL_REIIDEN_DGROUP), &resident, row);
    } else if (program == RETAIL_PROGRAM_FUUIN) {
        M_Fuuin((uint16_t)(load + RETAIL_FUUIN_DGROUP), row);
    }
    memcpy(&row[TH01_COLUMN_difficulty], th01->choices, sizeof(th01->choices));
}
