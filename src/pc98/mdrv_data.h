#pragma once

#include "core/io/reader.h"

#define MDRV_FM_INSTRUMENT_SIZE 32
#define MDRV_SSG_ENVELOPE_SIZE 6
#define MDRV_EFFECT_MAX_CHANNELS 3

typedef struct {
    uint16_t offset;
    uint8_t channel, reserved;
} MDRV_TRACK;

typedef struct {
    uint8_t channel_count, base_channel;
    uint16_t block_offset;
    uint16_t stream_offsets[MDRV_EFFECT_MAX_CHANNELS];
} MDRV_EFFECT;

typedef struct {
    const uint8_t *bytes;
    size_t size;
    const MDRV_TRACK *tracks;
    uint8_t track_count, flags, metadata_flags, reserved;
    uint16_t hardware;
    uint16_t commands_offset, fm_offset, ssg_offset;
    uint16_t fm_count, ssg_count;
    const char *pcm_name, *spm_name, *title;
} MDRV_MUSIC;

typedef struct {
    const uint8_t *bytes;
    size_t size;
    const MDRV_EFFECT *effects;
    uint16_t effect_count, ssg_offset, ssg_count;
    uint16_t reserved[5];
} MDRV_EFFECTS;

// Loads MDRV2 MDT 2.3 music or MDE 1.0 effects from the entire remaining
// reader range, up to 65536 bytes. Use a slice to exclude trailing data.
// All offsets count from the entry position, including in a file slice.
// Keeps reader open; consumes the record on success. Leaves out empty and
// allocates nothing from allocator on failure, including file read errors.
// Allocates one block per asset from allocator, which owns its lifetime.
// All returned pointers remain valid after the reader closes.
//
// Retains immutable command bytes and raw 32-byte FM and 6-byte SSG banks.
// Does not interpret commands, resolve branches, or play sound. Track starts
// can share command data; the next initial offset does not bound a stream.
// A zero track channel disables it even when its offset is nonzero. Channel
// tags use high bits 80 FM, 40 SSG, 20 PCM, 10 rhythm and low channel bits.
// Accepts FM channels 0-5, SSG 0-2, PCM 0-3 and rhythm 0. Effects use SSG
// groups only. Derives bank counts from offsets and the record end; the
// terminal SSG bank has no declared length.
// Music flags retain bit 80 repeat; metadata text is decoded from CP932 to
// UTF-8, with absent fields nullptr. Reserved fields have no native meaning.
// Effects use zero-based array indices; the DOS playback API uses index + 1.
// Keep mutable player, loop and channel state separate from these assets.
RESULT MdrvData_LoadMusic(
    READER *reader, const ALLOCATOR *allocator, MDRV_MUSIC *out);
RESULT MdrvData_LoadEffects(
    READER *reader, const ALLOCATOR *allocator, MDRV_EFFECTS *out);
