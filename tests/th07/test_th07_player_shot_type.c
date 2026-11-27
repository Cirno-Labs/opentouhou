#include "th07/player/shot_type.h"

#include "th07/files.h"

#include "core/base/memory.h"

#include "test.h"
#include "test_dir.h"
#include "th07_data.h"

#include <string.h>

static const TH07_SHT_SHOT m_Plain = {
    .interval = 5,
    .phase = 1,
    .offset_x = -4.0f,
    .offset_y = -8.0f,
    .size_x = 12.0f,
    .size_y = 24.0f,
    .angle = -1.5707964f,
    .speed = 12.0f,
    .damage = 30,
    .option = 2,
    .kind = 3,
    .script = 0x440,
    .sound = -1,
    .fire = 4,
    .move = 3,
    .draw = 1,
    .hit = 2,
};

static TH07_SHT_SHOT m_Shots[2];
static TH07_SHT_TABLE m_Tables[2];

static TH07_SHT M_Sht(void)
{
    m_Shots[0] = m_Plain;
    m_Shots[1] = m_Plain;
    m_Shots[1].fire = 0;
    m_Shots[1].move = 0;
    m_Shots[1].draw = 0;
    m_Shots[1].hit = 0;
    m_Shots[1].kind = 0;
    m_Tables[0] = (TH07_SHT_TABLE) { 8, 1, m_Shots };
    m_Tables[1] = (TH07_SHT_TABLE) { 999, 2, m_Shots };
    return (TH07_SHT) {
        .bombs = 3.0f,
        .bomb_window = 15,
        .floats = { 2.0f, 41.0f, 8.0f, 32.0f, 0.5f, 128.0f, 4.0f, 2.0f,
                    2.828427f, 1.4142135f },
        .table_count = 2,
        .tables = m_Tables,
    };
}

static RESULT M_ReadBytes(
    const uint8_t *const bytes, const size_t size, SHOT_TYPE *const out)
{
    READER reader;
    Reader_OpenBuffer(&reader, bytes, size, nullptr);
    const RESULT result = ShotType_Read(&reader, out);
    Reader_Close(&reader);
    return result;
}

static RESULT M_Read(const TH07_SHT *const sht, SHOT_TYPE *const out)
{
    size_t size;
    uint8_t *const bytes = Th07Data_BuildSht(sht, &size);
    const RESULT result = M_ReadBytes(bytes, size, out);
    Mem_Free(bytes);
    return result;
}

static bool M_Rejects(const TH07_SHT *const sht)
{
    SHOT_TYPE shot_type;
    if (Result_Ignore(M_Read(sht, &shot_type))) {
        ShotType_Release(&shot_type);
        return false;
    }
    return shot_type.tables == nullptr && shot_type.table_count == 0;
}

// Reads the shot type with one shot of the second table changed.
static bool M_RejectsShot(const TH07_SHT_SHOT shot)
{
    TH07_SHT sht = M_Sht();
    m_Shots[1] = shot;
    return M_Rejects(&sht);
}

TEST(reads_the_header_and_every_shot_field)
{
    const TH07_SHT sht = M_Sht();
    SHOT_TYPE out;
    CHECK(Result_Log(M_Read(&sht, &out), LOG_LEVEL_ERROR));
    CHECK(out.bombs == 3.0f);
    CHECK_EQ_INT(out.bomb_window, 15);
    CHECK(out.hitbox_size == 2.0f);
    CHECK(out.graze_size == 41.0f);
    CHECK(out.item_pull_speed == 8.0f);
    CHECK(out.item_collect_size == 32.0f);
    CHECK(out.death_cherry_share == 0.5f);
    CHECK(out.item_line == 128.0f);
    CHECK(out.speed == 4.0f);
    CHECK(out.speed_focused == 2.0f);
    CHECK(out.speed_diagonal == 2.828427f);
    CHECK(out.speed_diagonal_focused == 1.4142135f);
    CHECK_EQ_INT(out.table_count, 2);
    CHECK_EQ_INT(out.tables[0].max_power, 8);
    CHECK_EQ_INT(out.tables[0].count, 1);
    CHECK_EQ_INT(out.tables[1].max_power, 999);
    CHECK_EQ_INT(out.tables[1].count, 2);
    const SHOT *const shot = &out.tables[1].shots[0];
    CHECK_EQ_INT(shot->interval, 5);
    CHECK_EQ_INT(shot->phase, 1);
    CHECK(shot->offset_x == -4.0f);
    CHECK(shot->offset_y == -8.0f);
    CHECK(shot->size_x == 12.0f);
    CHECK(shot->size_y == 24.0f);
    CHECK(shot->angle == -1.5707964f);
    CHECK(shot->speed == 12.0f);
    CHECK_EQ_INT(shot->damage, 30);
    CHECK_EQ_INT(shot->option, 2);
    CHECK_EQ_INT(shot->kind, SHOT_KIND_PIERCING);
    CHECK_EQ_INT(shot->script, 0x440);
    CHECK_EQ_INT(shot->sound, -1);
    CHECK_EQ_INT(shot->fire, SHOT_FIRE_HOMING);
    CHECK_EQ_INT(shot->move, SHOT_MOVE_RISING);
    CHECK_EQ_INT(shot->draw, SHOT_DRAW_TRAIL);
    CHECK_EQ_INT(shot->hit, SHOT_HIT_SPARK);
    CHECK_EQ_INT(out.tables[1].shots[1].fire, SHOT_FIRE_PLAIN);
    ShotType_Release(&out);
    CHECK(out.tables == nullptr);
    CHECK_EQ_INT(out.table_count, 0);
}

TEST(behavior_indices_map_onto_their_enums)
{
    static const uint32_t fires[] = { 0, 1, 2, 3, 4, 5 };
    static const SHOT_FIRE fired[] = {
        SHOT_FIRE_PLAIN,       SHOT_FIRE_PLAIN,  SHOT_FIRE_LASER,
        SHOT_FIRE_TRAIL_LASER, SHOT_FIRE_HOMING, SHOT_FIRE_AIMED,
    };
    static const SHOT_KIND kinds[] = {
        SHOT_KIND_PLAIN,    SHOT_KIND_PLAIN, SHOT_KIND_PLAIN,
        SHOT_KIND_PIERCING, SHOT_KIND_LASER, SHOT_KIND_TRAIL_LASER,
    };
    for (uint32_t i = 0; i < 6; i++) {
        TH07_SHT sht = M_Sht();
        m_Shots[0].phase = 2;
        m_Shots[0].interval = 16;
        m_Shots[0].fire = fires[i];
        m_Shots[0].move = (i + 2) % 6;
        m_Shots[0].draw = i % 2;
        m_Shots[0].hit = i % 3;
        m_Shots[0].kind = (uint8_t)i;
        SHOT_TYPE out;
        CHECK(Result_Log(M_Read(&sht, &out), LOG_LEVEL_ERROR));
        const SHOT *const shot = &out.tables[0].shots[0];
        CHECK_EQ_INT(shot->fire, fired[i]);
        CHECK_EQ_INT(shot->move, (SHOT_MOVE)((i + 2) % 6));
        CHECK_EQ_INT(shot->draw, (SHOT_DRAW)(i % 2));
        CHECK_EQ_INT(shot->hit, (SHOT_HIT)(i % 3));
        CHECK_EQ_INT(shot->kind, kinds[i]);
        ShotType_Release(&out);
    }
}

TEST(rejects_an_index_outside_its_enum)
{
    TH07_SHT_SHOT shot = m_Plain;
    shot.fire = 6;
    CHECK(M_RejectsShot(shot));
    shot = m_Plain;
    shot.move = 6;
    CHECK(M_RejectsShot(shot));
    shot = m_Plain;
    shot.draw = 2;
    CHECK(M_RejectsShot(shot));
    shot = m_Plain;
    shot.hit = 3;
    CHECK(M_RejectsShot(shot));
    shot = m_Plain;
    shot.kind = 6;
    CHECK(M_RejectsShot(shot));
    shot = m_Plain;
    shot.option = 3;
    CHECK(M_RejectsShot(shot));
}

TEST(rejects_a_laser_entry_outside_the_lasers)
{
    for (uint32_t fire = 2; fire <= 3; fire++) {
        TH07_SHT_SHOT shot = m_Plain;
        shot.fire = fire;
        shot.phase = 3;
        CHECK(M_RejectsShot(shot));
        shot.phase = -1;
        CHECK(M_RejectsShot(shot));
        shot.phase = 2;
        CHECK(!M_RejectsShot(shot));
    }
}

TEST(rejects_a_laser_move_without_a_laser_fire_from_an_option)
{
    TH07_SHT_SHOT shot = m_Plain;
    shot.move = 4;
    shot.phase = 0;
    for (uint32_t fire = 0; fire <= 5; fire++) {
        shot.fire = fire;
        CHECK_EQ_INT(M_RejectsShot(shot), fire != 2 && fire != 3);
    }
    shot.fire = 2;
    shot.option = 0;
    CHECK(M_RejectsShot(shot));
    shot.move = 5;
    CHECK(!M_RejectsShot(shot));
}

TEST(rejects_a_trail_longer_than_16)
{
    TH07_SHT_SHOT shot = m_Plain;
    shot.fire = 3;
    shot.phase = 2;
    shot.interval = 17;
    CHECK(M_RejectsShot(shot));
    shot.interval = 16;
    CHECK(!M_RejectsShot(shot));
    shot.fire = 2;
    shot.interval = 17;
    CHECK(!M_RejectsShot(shot));
}

TEST(rejects_a_plain_interval_of_0_but_not_a_laser_one)
{
    TH07_SHT_SHOT shot = m_Plain;
    shot.interval = 0;
    for (uint32_t fire = 0; fire <= 5; fire++) {
        shot.fire = fire;
        shot.phase = 0;
        CHECK_EQ_INT(M_RejectsShot(shot), fire != 2 && fire != 3);
    }
}

TEST(rejects_no_table_past_full_power)
{
    TH07_SHT sht = M_Sht();
    m_Tables[1].max_power = 128;
    CHECK(M_Rejects(&sht));
    sht = M_Sht();
    m_Tables[1].max_power = 129;
    CHECK(!M_Rejects(&sht));
    sht = M_Sht();
    m_Tables[0].max_power = 200;
    m_Tables[1].max_power = 0;
    CHECK(!M_Rejects(&sht));
    sht = M_Sht();
    sht.table_count = 0;
    CHECK(M_Rejects(&sht));
}

TEST(rejects_a_table_without_shots)
{
    TH07_SHT sht = M_Sht();
    m_Tables[0].count = 0;
    CHECK(M_Rejects(&sht));
}

TEST(rejects_a_table_without_its_closing_shot)
{
    const TH07_SHT sht = M_Sht();
    size_t size;
    uint8_t *const bytes = Th07Data_BuildSht(&sht, &size);
    SHOT_TYPE out;
    CHECK(Result_Ignore(M_ReadBytes(bytes, size - 0x32, &out)));
    ShotType_Release(&out);
    CHECK(!Result_Ignore(M_ReadBytes(bytes, size - 0x33, &out)));
    CHECK(out.tables == nullptr);
    Mem_Free(bytes);
}

TEST(rejects_a_table_offset_outside_the_file)
{
    const TH07_SHT sht = M_Sht();
    size_t size;
    uint8_t *const bytes = Th07Data_BuildSht(&sht, &size);
    SHOT_TYPE out;
    memcpy(&bytes[0x3c], &(uint32_t) { (uint32_t)size }, 4);
    CHECK(!Result_Ignore(M_ReadBytes(bytes, size, &out)));
    memcpy(&bytes[0x3c], &(uint32_t) { UINT32_MAX }, 4);
    CHECK(!Result_Ignore(M_ReadBytes(bytes, size, &out)));
    CHECK(out.tables == nullptr);
    Mem_Free(bytes);
}

TEST(rejects_a_short_header_or_table_list)
{
    const TH07_SHT sht = M_Sht();
    size_t size;
    uint8_t *const bytes = Th07Data_BuildSht(&sht, &size);
    SHOT_TYPE out;
    CHECK(!Result_Ignore(M_ReadBytes(bytes, 0x33, &out)));
    bytes[2] = 0xff;
    CHECK(!Result_Ignore(M_ReadBytes(bytes, size, &out)));
    CHECK(out.tables == nullptr);
    Mem_Free(bytes);
}

TEST(loads_a_shot_type_through_the_files)
{
    char *const directory = TestDir_Prepare("shot_type");
    const TH07_DATA_FILE version = { "th07_0100b.ver", "0100b" };
    Th07Data_WriteArchive(directory, &version, 1);
    Th07Data_WriteMusic(directory, nullptr, 0);
    const TH07_SHT sht = M_Sht();
    size_t size;
    uint8_t *const bytes = Th07Data_BuildSht(&sht, &size);
    Th07Data_WriteFile(directory, "ply00a.sht", bytes, size);
    Mem_Free(bytes);
    CHECK(Result_Log(Th07Data_Mount(directory), LOG_LEVEL_ERROR));
    SHOT_TYPE out;
    CHECK(Result_Log(ShotType_Load("ply00a.sht", &out), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(out.tables[1].count, 2);
    ShotType_Release(&out);
    CHECK(!Result_Ignore(ShotType_Load("ply00b.sht", &out)));
    CHECK(out.tables == nullptr);
    Th07Data_Unmount();
    Mem_Free(directory);
}
