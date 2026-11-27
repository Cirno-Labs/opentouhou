#include "pc98/opna.h"

#include "core/base/arena.h"
#include "test.h"

typedef struct {
    ARENA arena;
    ALLOCATOR allocator;
    size_t allocations;
    OPNA *chip;
    uint64_t events[4096];
    unsigned timers[4096];
    size_t count;
    bool acknowledge;
    bool disable_b;
    bool synthesize;
} FIXTURE;

static void *M_Allocate(void *const context, const size_t size)
{
    FIXTURE *const f = context;
    f->allocations++;
    return Arena_Alloc(&f->arena, size);
}

static void M_Timer(
    void *const context, OPNA *const chip, const unsigned timer,
    const uint64_t time)
{
    FIXTURE *const f = context;
    if (f->count < 4096) {
        f->events[f->count] = time;
        f->timers[f->count] = timer;
    }
    f->count++;
    if (f->synthesize && timer == 1) {
        Opna_Write(chip, 0xa0, (time / 11520) & 255);
        Opna_Write(chip, 8, (time / 11520) & 15);
        Opna_Write(chip, 0x10, 1 << ((time / 11520) % 6));
    }
    if (f->acknowledge) {
        CHECK(Opna_Irq(chip));
        CHECK((Opna_ReadPort(chip, 0) & (1 << timer)) != 0);
        Opna_Write(chip, 0x27, f->disable_b ? 0x35 : 0x3f);
    }
}

static void M_Init(FIXTURE *const f)
{
    memset(f, 0, sizeof(*f));
    Arena_Init(&f->arena, "opna test", 1024 * 1024);
    f->allocator = (ALLOCATOR) { .alloc = M_Allocate, .context = f };
    f->chip = Opna_Init(&f->allocator, M_Timer, f);
    CHECK_EQ_INT(f->allocations, 1);
}

static void M_Fm(OPNA *const chip, const unsigned channel, const uint8_t pan)
{
    const uint16_t base = (channel / 3) * 256 + channel % 3;
    for (unsigned op = 0; op < 4; op++) {
        Opna_Write(chip, base + 0x30 + op * 4, 1);
        Opna_Write(chip, base + 0x40 + op * 4, 0x28);
        Opna_Write(chip, base + 0x50 + op * 4, 0x1f);
        Opna_Write(chip, base + 0x80 + op * 4, 0x0f);
    }
    Opna_Write(chip, base + 0xb0, 7);
    Opna_Write(chip, base + 0xb4, pan);
    Opna_Write(chip, base + 0xa4, 0x22);
    Opna_Write(chip, base + 0xa0, 0x69 + channel * 10);
    Opna_Write(chip, 0x28, 0xf0 | (channel / 3) * 4 | channel % 3);
}

static int64_t M_Energy(
    const OPNA_FRAME *const out, const size_t frames, const unsigned bus)
{
    int64_t total = 0;
    for (size_t i = 0; i < frames; i++) {
        const int32_t value =
            bus < 2 ? out[i].fm_rhythm[bus] : out[i].ssg[bus - 2];
        total += value < 0 ? -(int64_t)value : value;
    }
    return total;
}

static void M_AllVoices(OPNA *const chip)
{
    Opna_Write(chip, 0x29, 0x9f);
    for (unsigned ch = 0; ch < 6; ch++) {
        M_Fm(chip, ch, ch % 2 == 0 ? 0x80 : 0x40);
        Opna_Write(chip, 0x18 + ch, 0xdf);
    }
    Opna_Write(chip, 7, 0x00);
    Opna_Write(chip, 6, 7);
    for (unsigned ch = 0; ch < 3; ch++) {
        Opna_Write(chip, ch * 2, 31 + ch * 53);
        Opna_Write(chip, ch * 2 + 1, 0);
        Opna_Write(chip, 8 + ch, ch == 1 ? 0x10 : 15);
    }
    Opna_Write(chip, 11, 19);
    Opna_Write(chip, 12, 0);
    Opna_Write(chip, 13, 0x0e);
    Opna_Write(chip, 0x11, 0x3f);
    Opna_Write(chip, 0x10, 0x3f);
}

TEST(reset_silence_and_no_render_allocation)
{
    FIXTURE f;
    M_Init(&f);
    OPNA_FRAME out[1024];
    Opna_Render(f.chip, out, 1024);
    for (unsigned bus = 0; bus < 5; bus++) {
        CHECK_EQ_INT(M_Energy(out, 1024, bus), 0);
    }
    CHECK(!Opna_Irq(f.chip));
    CHECK_EQ_INT(Opna_Time(f.chip), 1024 * 832);
    M_AllVoices(f.chip);
    Opna_Render(f.chip, nullptr, 48000);
    Opna_Reset(f.chip);
    CHECK_EQ_INT(Opna_Time(f.chip), 0);
    Opna_Render(f.chip, out, 1024);
    for (unsigned bus = 0; bus < 5; bus++) {
        CHECK_EQ_INT(M_Energy(out, 1024, bus), 0);
    }
    CHECK_EQ_INT(f.allocations, 1);
    Arena_Free(&f.arena);
}

TEST(six_fm_voices_pan_keyoff_and_extended_mode)
{
    FIXTURE f;
    M_Init(&f);
    OPNA_FRAME out[4096];
    for (unsigned ch = 0; ch < 6; ch++) {
        Opna_Reset(f.chip);
        Opna_Write(f.chip, 0x29, 0x9f);
        M_Fm(f.chip, ch, 0x80);
        Opna_Render(f.chip, out, 4096);
        CHECK(M_Energy(out, 4096, 0) > 10000);
        CHECK_EQ_INT(M_Energy(out, 4096, 1), 0);
        Opna_Write(f.chip, (ch / 3) * 256 + 0xb4 + ch % 3, 0x40);
        Opna_Render(f.chip, out, 4096);
        CHECK(M_Energy(out + 2, 4094, 1) > 10000);
        CHECK_EQ_INT(M_Energy(out + 2, 4094, 0), 0);
        Opna_Write(f.chip, 0x28, (ch / 3) * 4 + ch % 3);
        Opna_Render(f.chip, nullptr, 48000);
        Opna_Render(f.chip, out, 4096);
        CHECK_EQ_INT(M_Energy(out, 4096, 1), 0);
    }
    Opna_Reset(f.chip);
    M_Fm(f.chip, 3, 0xc0);
    Opna_Render(f.chip, out, 4096);
    CHECK_EQ_INT(M_Energy(out, 4096, 0), 0);
    Opna_Write(f.chip, 0x29, 0x9f);
    Opna_Write(f.chip, 0x28, 0xf4);
    Opna_Render(f.chip, out, 4096);
    CHECK(M_Energy(out, 4096, 0) > 10000);
    Arena_Free(&f.arena);
}

TEST(ssg_noise_envelope_shared_state_and_register_ports)
{
    FIXTURE f;
    M_Init(&f);
    OPNA_FRAME out[4096];
    OPNA_FRAME tone[4096];
    Opna_Write(f.chip, 7, 0x38);
    for (unsigned ch = 0; ch < 3; ch++) {
        Opna_Write(f.chip, ch * 2, 101);
        Opna_Write(f.chip, 8 + ch, 15);
    }
    Opna_WritePort(f.chip, 0, 0);
    CHECK_EQ_INT(Opna_ReadPort(f.chip, 1), 101);
    Opna_WritePort(f.chip, 2, 0xa0);
    Opna_WritePort(f.chip, 1, 55);
    Opna_WritePort(f.chip, 0, 0);
    CHECK_EQ_INT(Opna_ReadPort(f.chip, 1), 101);
    Opna_Render(f.chip, tone, 4096);
    for (size_t i = 0; i < 4096; i++) {
        CHECK_EQ_INT(tone[i].ssg[0], tone[i].ssg[1]);
        CHECK_EQ_INT(tone[i].ssg[1], tone[i].ssg[2]);
    }
    CHECK(M_Energy(tone, 4096, 2) > 10000);
    Opna_Write(f.chip, 7, 7);
    Opna_Write(f.chip, 6, 3);
    Opna_Render(f.chip, out, 4096);
    CHECK(memcmp(tone, out, sizeof(out)) != 0);
    CHECK(M_Energy(out, 4096, 2) > 10000);
    for (size_t i = 0; i < 4096; i++) {
        CHECK_EQ_INT(out[i].ssg[0], out[i].ssg[2]);
    }
    Opna_Write(f.chip, 7, 0x3f);
    Opna_Write(f.chip, 11, 50);
    Opna_Write(f.chip, 13, 0x0e);
    Opna_Write(f.chip, 8, 0x10);
    Opna_Write(f.chip, 9, 0x10);
    Opna_Write(f.chip, 10, 0);
    Opna_Render(f.chip, out, 4096);
    CHECK(M_Energy(out, 4096, 2) > 10000);
    CHECK(out[100].ssg[0] != out[500].ssg[0]);
    for (size_t i = 2; i < 4096; i++) {
        CHECK_EQ_INT(out[i].ssg[0], out[i].ssg[1]);
        CHECK_EQ_INT(out[i].ssg[2], 0);
    }
    Arena_Free(&f.arena);
}

TEST(rhythm_six_voices_pan_and_dump)
{
    FIXTURE f;
    M_Init(&f);
    OPNA_FRAME out[4096];
    for (unsigned ch = 0; ch < 6; ch++) {
        Opna_Reset(f.chip);
        Opna_Write(f.chip, 0x11, 0x3f);
        Opna_Write(f.chip, 0x18 + ch, 0x9f);
        Opna_Write(f.chip, 0x10, 1 << ch);
        Opna_Render(f.chip, out, 4096);
        CHECK(M_Energy(out, 4096, 0) > 1000);
        CHECK_EQ_INT(M_Energy(out, 4096, 1), 0);
        Opna_Write(f.chip, 0x10, 0x80 | (1 << ch));
        Opna_Render(f.chip, out, 4096);
        CHECK_EQ_INT(M_Energy(out + 2, 4094, 0), 0);
    }
    Arena_Free(&f.arena);
}

static void M_Timers(OPNA *const chip)
{
    Opna_Write(chip, 0x24, 0xfc);
    Opna_Write(chip, 0x25, 0);
    Opna_Write(chip, 0x26, 255);
    Opna_Write(chip, 0x27, 15);
}

TEST(timer_exact_boundaries_irq_reload_and_disable)
{
    FIXTURE f;
    M_Init(&f);
    M_Timers(f.chip);
    Opna_Render(f.chip, nullptr, 13);
    CHECK_EQ_INT(f.count, 0);
    CHECK(!Opna_Irq(f.chip));
    Opna_Render(f.chip, nullptr, 1);
    CHECK_EQ_INT(f.count, 2);
    CHECK_EQ_INT(f.events[0], 16 * 144 * 5);
    CHECK_EQ_INT(f.events[0], f.events[1]);
    CHECK_EQ_INT(f.timers[0], 0);
    CHECK_EQ_INT(f.timers[1], 1);
    CHECK(Opna_Irq(f.chip));
    CHECK_EQ_INT(Opna_ReadPort(f.chip, 0) & 3, 3);
    Opna_Write(f.chip, 0x27, 0x3f);
    CHECK(!Opna_Irq(f.chip));
    CHECK_EQ_INT(Opna_ReadPort(f.chip, 0) & 3, 0);
    Opna_Write(f.chip, 0x26, 254);
    Opna_Render(f.chip, nullptr, 14);
    CHECK_EQ_INT(f.events[2], 2 * 16 * 144 * 5);
    CHECK_EQ_INT(f.events[3], 2 * 16 * 144 * 5);
    Opna_Write(f.chip, 0x27, 0x30);
    Opna_Render(f.chip, nullptr, 1000);
    CHECK_EQ_INT(f.count, 4);
    Opna_Reset(f.chip);
    f.count = 0;
    M_Timers(f.chip);
    Opna_Write(f.chip, 0x27, 3);
    Opna_Render(f.chip, nullptr, 14);
    CHECK_EQ_INT(f.count, 2);
    CHECK(!Opna_Irq(f.chip));
    CHECK_EQ_INT(Opna_ReadPort(f.chip, 0) & 3, 0);
    Arena_Free(&f.arena);
}

TEST(timer_callback_writes_precede_same_time_timer_b)
{
    FIXTURE f;
    M_Init(&f);
    f.acknowledge = f.disable_b = true;
    M_Timers(f.chip);
    Opna_Render(f.chip, nullptr, 100);
    CHECK_EQ_INT(f.count, 7);
    for (size_t i = 0; i < f.count; i++) {
        CHECK_EQ_INT(f.timers[i], 0);
        CHECK_EQ_INT(f.events[i], (i + 1) * 16 * 144 * 5);
    }
    CHECK(!Opna_Irq(f.chip));
    Arena_Free(&f.arena);
}

TEST(reset_replay_and_arbitrary_render_blocks_match)
{
    FIXTURE f;
    M_Init(&f);
    OPNA_FRAME whole[16000];
    OPNA_FRAME pieces[16000];
    M_AllVoices(f.chip);
    M_Timers(f.chip);
    f.acknowledge = f.synthesize = true;
    Opna_Render(f.chip, whole, 16000);
    const size_t event_count = f.count;
    uint64_t times[4096];
    memcpy(times, f.events, sizeof(times));
    Opna_Reset(f.chip);
    f.count = 0;
    M_AllVoices(f.chip);
    M_Timers(f.chip);
    for (size_t pos = 0; pos < 16000;) {
        size_t count = (pos * 17) % 173 + 1;
        if (count > 16000 - pos) {
            count = 16000 - pos;
        }
        Opna_Render(f.chip, nullptr, 0);
        Opna_Render(f.chip, pieces + pos, count);
        pos += count;
    }
    CHECK_EQ_INT(memcmp(whole, pieces, sizeof(whole)), 0);
    CHECK_EQ_INT(f.count, event_count);
    CHECK_EQ_INT(memcmp(times, f.events, event_count * sizeof(*times)), 0);
    CHECK_EQ_INT(f.allocations, 1);
    Arena_Free(&f.arena);
}

TEST(long_clock_phase_and_single_frame_chunks)
{
    FIXTURE f;
    M_Init(&f);
    Opna_Write(f.chip, 0x26, 0);
    Opna_Write(f.chip, 0x27, 2);
    for (size_t i = 0; i < 48000 * 10; i++) {
        Opna_Render(f.chip, nullptr, 1);
    }
    CHECK_EQ_INT(Opna_Time(f.chip), 7987200ULL * 5 * 10);
    CHECK_EQ_INT(f.count, (7987200ULL * 10) / (256 * 16 * 144));
    for (size_t i = 0; i < f.count; i++) {
        CHECK_EQ_INT(f.events[i], (i + 1) * 256ULL * 16 * 144 * 5);
    }
    CHECK_EQ_INT(f.allocations, 1);
    Arena_Free(&f.arena);
}

static size_t M_Crossings(const OPNA_FRAME *const out, const size_t frames)
{
    size_t count = 0;
    for (size_t i = 1; i < frames; i++) {
        count += out[i - 1].fm_rhythm[0] < 0 && out[i].fm_rhythm[0] >= 0;
    }
    return count;
}

TEST(prescalers_keep_native_pitch_and_exact_timer_reload)
{
    FIXTURE f;
    M_Init(&f);
    OPNA_FRAME out[4800];
    size_t crossings[3];
    const unsigned dividers[] = { 144, 72, 48 };
    for (unsigned mode = 0; mode < 3; mode++) {
        Opna_Reset(f.chip);
        f.count = 0;
        if (mode == 1) {
            Opna_WritePort(f.chip, 0, 0x2e);
        } else if (mode == 2) {
            Opna_WritePort(f.chip, 0, 0x2f);
        }
        M_Fm(f.chip, 0, 0xc0);
        M_Timers(f.chip);
        Opna_Render(f.chip, out, 4800);
        crossings[mode] = M_Crossings(out, 4800);
        CHECK(crossings[mode] > 10);
        CHECK_EQ_INT(f.events[0], 16 * dividers[mode] * 5);
        for (size_t i = 0; i < f.count; i++) {
            CHECK_EQ_INT(f.events[i], (i / 2 + 1) * 16 * dividers[mode] * 5);
        }
    }
    CHECK(crossings[1] >= 2 * crossings[0] - 2);
    CHECK(crossings[1] <= 2 * crossings[0] + 2);
    CHECK(crossings[2] >= 3 * crossings[0] - 2);
    CHECK(crossings[2] <= 3 * crossings[0] + 2);
    Opna_Reset(f.chip);
    f.count = 0;
    M_Timers(f.chip);
    Opna_Render(f.chip, nullptr, 1);
    Opna_WritePort(f.chip, 0, 0x2f);
    Opna_Render(f.chip, nullptr, 20);
    CHECK_EQ_INT(f.events[0], 16 * 144 * 5);
    CHECK_EQ_INT(f.events[2], 16 * (144 + 48) * 5);
    Arena_Free(&f.arena);
}

TEST(fm_algorithms_lfo_and_three_slot_mode_change_output)
{
    FIXTURE f;
    M_Init(&f);
    OPNA_FRAME normal[4096];
    OPNA_FRAME changed[4096];
    M_Fm(f.chip, 2, 0xc0);
    Opna_Render(f.chip, normal, 4096);
    Opna_Reset(f.chip);
    M_Fm(f.chip, 2, 0xc0);
    Opna_Write(f.chip, 0xb2, 0x38);
    Opna_Render(f.chip, changed, 4096);
    CHECK(memcmp(normal, changed, sizeof(normal)) != 0);
    Opna_Reset(f.chip);
    M_Fm(f.chip, 2, 0xc0);
    Opna_Write(f.chip, 0x22, 0x0f);
    Opna_Write(f.chip, 0xb6, 0xf7);
    for (unsigned op = 0; op < 4; op++) {
        Opna_Write(f.chip, 0x62 + op * 4, 0x80);
    }
    Opna_Render(f.chip, changed, 4096);
    CHECK(memcmp(normal, changed, sizeof(normal)) != 0);
    Opna_Reset(f.chip);
    M_Fm(f.chip, 2, 0xc0);
    for (unsigned slot = 0; slot < 3; slot++) {
        Opna_Write(f.chip, 0xac + slot, 0x19 + slot);
        Opna_Write(f.chip, 0xa8 + slot, 0x20 + slot * 50);
    }
    Opna_Write(f.chip, 0x27, 0x40);
    Opna_Render(f.chip, changed, 4096);
    CHECK(M_Energy(changed, 4096, 0) > 10000);
    CHECK(memcmp(normal, changed, sizeof(normal)) != 0);
    Arena_Free(&f.arena);
}

TEST(timer_output_boundary_and_irq_masks)
{
    FIXTURE f;
    M_Init(&f);
    M_Timers(f.chip);
    f.acknowledge = true;
    Opna_Render(f.chip, nullptr, 179);
    CHECK_EQ_INT(f.count, 24);
    Opna_Render(f.chip, nullptr, 1);
    CHECK_EQ_INT(f.count, 26);
    CHECK_EQ_INT(f.events[25], Opna_Time(f.chip));
    CHECK(!Opna_Irq(f.chip));
    Opna_Reset(f.chip);
    f.count = 0;
    f.acknowledge = false;
    M_Timers(f.chip);
    Opna_Write(f.chip, 0x29, 0x1c);
    Opna_Render(f.chip, nullptr, 14);
    CHECK_EQ_INT(Opna_ReadPort(f.chip, 0) & 3, 3);
    CHECK(!Opna_Irq(f.chip));
    Opna_Write(f.chip, 0x29, 0x1f);
    CHECK(Opna_Irq(f.chip));
    Opna_Write(f.chip, 0x27, 0x3f);
    CHECK(!Opna_Irq(f.chip));
    Opna_Write(f.chip, 0x24, 255);
    Opna_Write(f.chip, 0x25, 3);
    Opna_Write(f.chip, 0x26, 254);
    Opna_Render(f.chip, nullptr, 15);
    CHECK_EQ_INT(f.events[2], 23040);
    CHECK_EQ_INT(f.events[3], 23040);
    CHECK_EQ_INT(f.events[4], 23760);
    Opna_Render(f.chip, nullptr, 27);
    bool found_b = false;
    for (size_t i = 4; i < f.count; i++) {
        if (f.timers[i] == 1) {
            CHECK_EQ_INT(f.events[i], 46080);
            found_b = true;
        }
    }
    CHECK(found_b);
    Arena_Free(&f.arena);
}

TEST(all_fm_algorithms_and_feedback_are_defined)
{
    FIXTURE f;
    M_Init(&f);
    OPNA_FRAME out[1024];
    for (unsigned algorithm = 0; algorithm < 8; algorithm++) {
        for (unsigned feedback = 0; feedback < 8; feedback++) {
            Opna_Reset(f.chip);
            M_Fm(f.chip, 0, 0xc0);
            Opna_Write(f.chip, 0xb0, feedback * 8 + algorithm);
            Opna_Render(f.chip, out, 1024);
            CHECK(M_Energy(out, 1024, 0) > 1000);
        }
    }
    Arena_Free(&f.arena);
}
