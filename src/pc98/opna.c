#define SNDDEV_YM2608

#include "pc98/opna.h"

#include "core/base/assert.h"
#include "opna/vendor/emu/cores/ayintf.h"
#include "opna/vendor/emu/cores/ay8910.h"
#include "opna/vendor/emu/cores/fmopn.h"

#include <stdalign.h>
#include <string.h>

#define M_FRAME_TIME 832
#define M_DISABLED UINT64_MAX

struct OPNA {
    void *fm, *ssg, *fm_storage, *ssg_storage;
    OPNA_TIMER_CALLBACK callback;
    void *context;
    uint64_t time, fm_next, ssg_next, timer[2];
    uint32_t fm_period, ssg_period;
    int32_t held[5];
    bool irq, rendering;
};

size_t pc98_ym2608_size(void);
uint32_t pc98_ym2608_divider(void *chip);
void *pc98_ym2608_init(
    void *storage, void *param, uint32_t clock, uint32_t rate,
    FM_TIMERHANDLER timer_handler, FM_IRQHANDLER irq_handler);
size_t pc98_ay8910_size(void);
uint32_t pc98_ay8910_init(
    void *storage, void **chip, uint32_t clock, uint8_t type, uint8_t flags);
void pc98_ay8910_output(void *chip, int32_t *output);

static size_t M_Align(const size_t size)
{
    return (size + alignof(max_align_t) - 1) / alignof(max_align_t)
        * alignof(max_align_t);
}

static void M_Timer(
    void *const context, const uint8_t timer, const int32_t count,
    const uint32_t clock)
{
    OPNA *const chip = context;
    ASSERT(clock == 7987200 && timer < 2 && count >= 0);
    chip->timer[timer] =
        count == 0 ? M_DISABLED : chip->time + (uint64_t)count * 5;
}

static void M_Irq(void *const context, const uint8_t irq)
{
    OPNA *const chip = context;
    chip->irq = irq != 0;
}

static void M_Dividers(OPNA *const chip)
{
    const uint32_t fm_period = pc98_ym2608_divider(chip->fm) * 5;
    const uint32_t ssg_period = 7987200 / ay8910_get_sample_rate(chip->ssg) * 5;
    if (fm_period != chip->fm_period) {
        chip->fm_period = fm_period;
        chip->fm_next = chip->time + fm_period;
    }
    if (ssg_period != chip->ssg_period) {
        chip->ssg_period = ssg_period;
        chip->ssg_next = chip->time + ssg_period;
    }
}

static void M_Events(OPNA *const chip)
{
    for (unsigned timer = 0; timer < 2; timer++) {
        if (chip->timer[timer] == chip->time) {
            ym2608_timer_over(chip->fm, timer);
            if (chip->callback != nullptr) {
                chip->callback(chip->context, chip, timer, chip->time);
            }
        }
    }
    if (chip->fm_next == chip->time) {
        DEV_SMPL *out[2] = { &chip->held[0], &chip->held[1] };
        ym2608_update_one(chip->fm, 1, out);
        chip->fm_next += chip->fm_period;
    }
    if (chip->ssg_next == chip->time) {
        DEV_SMPL left, right;
        DEV_SMPL *out[2] = { &left, &right };
        ay8910_update_one(chip->ssg, 1, out);
        pc98_ay8910_output(chip->ssg, &chip->held[2]);
        chip->ssg_next += chip->ssg_period;
    }
}

OPNA *Opna_Init(
    const ALLOCATOR *const allocator, const OPNA_TIMER_CALLBACK callback,
    void *const context)
{
    const size_t fm_offset = M_Align(sizeof(OPNA));
    const size_t ssg_offset = fm_offset + M_Align(pc98_ym2608_size());
    OPNA *const chip =
        Allocator_AllocZ(allocator, ssg_offset + pc98_ay8910_size());
    chip->fm_storage = (unsigned char *)chip + fm_offset;
    chip->ssg_storage = (unsigned char *)chip + ssg_offset;
    chip->callback = callback;
    chip->context = context;
    Opna_Reset(chip);
    return chip;
}

void Opna_Reset(OPNA *const chip)
{
    ASSERT(!chip->rendering);
    chip->time = 0;
    chip->timer[0] = chip->timer[1] = M_DISABLED;
    chip->fm_period = chip->ssg_period = 0;
    memset(chip->held, 0, sizeof(chip->held));
    chip->irq = false;
    pc98_ay8910_init(
        chip->ssg_storage, &chip->ssg, 1996800, AYTYPE_YM2608,
        AY8910_LEGACY_OUTPUT);
    chip->fm = pc98_ym2608_init(
        chip->fm_storage, chip, 7987200, 55466, M_Timer, M_Irq);
    const ssg_callbacks callbacks = {
        ay8910_set_clock,
        ay8910_write,
        ay8910_read,
        ay8910_reset,
    };
    ym2608_link_ssg(chip->fm, &callbacks, chip->ssg);
    ym2608_reset_chip(chip->fm);
    M_Dividers(chip);
}

void Opna_WritePort(OPNA *const chip, const uint8_t port, const uint8_t value)
{
    ASSERT(port < 4);
    ym2608_write(chip->fm, port, value);
    M_Dividers(chip);
}

void Opna_Write(OPNA *const chip, const uint16_t reg, const uint8_t value)
{
    ASSERT(reg < 512);
    const uint8_t port = reg >= 256 ? 2 : 0;
    Opna_WritePort(chip, port, reg & 255);
    Opna_WritePort(chip, port + 1, value);
}

uint8_t Opna_ReadPort(OPNA *const chip, const uint8_t port)
{
    ASSERT(port < 4);
    return ym2608_read(chip->fm, port);
}

bool Opna_Irq(const OPNA *const chip)
{
    return chip->irq;
}

void Opna_Render(
    OPNA *const chip, OPNA_FRAME *const output, const size_t frames)
{
    ASSERT(!chip->rendering);
    chip->rendering = true;
    for (size_t frame = 0; frame < frames; frame++) {
        ASSERT(chip->time <= UINT64_MAX - M_FRAME_TIME);
        const uint64_t end = chip->time + M_FRAME_TIME;
        int64_t sum[5] = {};
        while (chip->time < end) {
            uint64_t next = end;
            const uint64_t events[] = {
                chip->timer[0],
                chip->timer[1],
                chip->fm_next,
                chip->ssg_next,
            };
            for (size_t i = 0; i < 4; i++) {
                if (events[i] < next) {
                    next = events[i];
                }
            }
            for (size_t i = 0; i < 5; i++) {
                sum[i] += chip->held[i] * (int64_t)(next - chip->time);
            }
            chip->time = next;
            M_Events(chip);
        }
        if (output != nullptr) {
            for (size_t i = 0; i < 2; i++) {
                output[frame].fm_rhythm[i] = sum[i] / M_FRAME_TIME;
            }
            for (size_t i = 0; i < 3; i++) {
                output[frame].ssg[i] = sum[i + 2] / M_FRAME_TIME;
            }
        }
    }
    chip->rendering = false;
}

uint64_t Opna_Time(const OPNA *const chip)
{
    return chip->time;
}
