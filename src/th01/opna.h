#pragma once

#include "core/base/memory.h"

typedef struct OPNA OPNA;

typedef struct {
    int32_t fm_rhythm[2];
    int32_t ssg[3];
} OPNA_FRAME;

typedef void (*OPNA_TIMER_CALLBACK)(
    void *context, OPNA *chip, unsigned timer, uint64_t time);

// Synthesizes one shared YM2608 with six FM voices, stereo rhythm and three
// SSG voices sharing noise and envelope generators. Uses the libvgm model;
// hardware parity and the PC-98 analog output stage are not verified.
// External ADPCM sample memory, recording, DAC data and busy timing are not
// provided. Does not interpret MDRV commands or connect to an audio device.
//
// Init allocates one lifetime group from allocator. Release the group with
// its arena. Reset rebuilds the state in place and allocates nothing, as do
// register operations and Render. Initialize instances serially: the backend
// builds shared immutable lookup tables on first initialization. An instance
// must have one owner; do not call it from concurrent threads.
OPNA *Opna_Init(
    const ALLOCATOR *allocator, OPNA_TIMER_CALLBACK callback, void *context);
void Opna_Reset(OPNA *chip);

// Writes one of the four address/data ports, or an atomic banked register
// (000..1FF). Preserves the chip's address-latch and mode semantics. Reads
// return backend status/data port values, including timer flags. Irq reports
// the masked interrupt line. Reset selects three FM channels; register 29
// bit 7 enables all six. Register writes occur at the current Time.
// Prescaler changes restart the FM and SSG sample-divider phases at Time;
// running timer deadlines retain their loaded interval until the next reload.
void Opna_WritePort(OPNA *chip, uint8_t port, uint8_t value);
void Opna_Write(OPNA *chip, uint16_t reg, uint8_t value);
uint8_t Opna_ReadPort(OPNA *chip, uint8_t port);
bool Opna_Irq(const OPNA *chip);

// Renders 48000 frames per second at a rational 7987200 Hz chip clock.
// Each frame holds the interval average of native sample-and-hold output,
// without clipping, gain, DC removal or final mixing. fm_rhythm retains the
// backend's signed stereo bus; ssg retains each unipolar legacy DAC level
// (nominal peak 16384 per channel). Apply routing and gain in the producer.
// A zero frame count does nothing; output may be nullptr to discard frames.
// Maintains divider, output and timer phase across arbitrary block sizes.
//
// Time counts fifths of a chip cycle (39936000 units per second), starting
// at zero after Reset. One output interval is exactly 832 units. Timers use
// exact integer chip-cycle counts at the current prescaler, with no rounding
// to output frames. On a deadline the backend sets flags and reloads before
// callback. The callback may read/write registers, including timer control;
// it must not call Reset or Render. At equal times process A, then B if still
// due, then FM, then SSG. Complete these events before returning a frame
// ending at that time. Externally issued writes then occur after those events.
// Flags latch only with timer enable bits; overflow callbacks also occur when
// flags are disabled. Irq is a level, so an uncleared flag need not transition
// on every callback. No host time, files or device state enters synthesis.
void Opna_Render(OPNA *chip, OPNA_FRAME *output, size_t frames);
uint64_t Opna_Time(const OPNA *chip);
