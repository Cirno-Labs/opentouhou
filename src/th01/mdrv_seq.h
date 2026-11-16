#pragma once

#include "th01/mdrv_data.h"

typedef enum {
    MDRV_EVENT_START,
    MDRV_EVENT_STOP,
    MDRV_EVENT_NOTE,
    MDRV_EVENT_REST,
    MDRV_EVENT_KEY_OFF,
    MDRV_EVENT_TIE,
    MDRV_EVENT_REPEAT,
    MDRV_EVENT_LOOP,
    MDRV_EVENT_TRACK_END,
    MDRV_EVENT_PATTERN,
    MDRV_EVENT_DETUNE,
    MDRV_EVENT_TRANSPOSE,
    MDRV_EVENT_TEMPO,
    MDRV_EVENT_GATE,
    MDRV_EVENT_PROGRAM,
    MDRV_EVENT_VOLUME,
    MDRV_EVENT_PITCH_LFO,
    MDRV_EVENT_VOLUME_LFO,
    MDRV_EVENT_HARDWARE_LFO,
    MDRV_EVENT_REGISTER,
    MDRV_EVENT_DELAY_NOISE,
    MDRV_EVENT_FADE,
    MDRV_EVENT_PAN,
    MDRV_EVENT_PORTAMENTO,
    MDRV_EVENT_SYNC,
    MDRV_EVENT_EXTENDED,
    MDRV_EVENT_OUTPUT_MASK,
} MDRV_EVENT_KIND;

typedef struct {
    MDRV_EVENT_KIND kind;
    uint64_t pulse;
    uint32_t offset, duration;
    uint16_t track, effect_number;
    uint8_t channel, source_channel, opcode, parameter_count, parameters[7];
    bool legato, suppressed;
} MDRV_EVENT;

typedef void (*MDRV_EVENT_CALLBACK)(void *context, const MDRV_EVENT *event);

typedef struct {
    uint32_t position, remaining, gate_remaining;
    uint32_t return_position, repeat_one_start, repeat_one_end,
        repeat_two_start;
    uint32_t loops;
    uint8_t channel, source_channel, note, gate, program, volume, pan;
    int8_t detune, transpose;
    uint8_t repeat_one_count, repeat_two_count;
    bool active, key_on, legato, repeat_one, repeat_two, suppressed;
} MDRV_CHANNEL_STATE;

typedef struct {
    const MDRV_MUSIC *music;
    const MDRV_EFFECTS *effects;
    MDRV_CHANNEL_STATE *tracks;
    uint8_t *work;
    size_t work_size;
    uint32_t commands_start, commands_end;
    uint16_t track_count, capacity, effect_number;
    uint64_t pulse, loops;
    uint8_t tempo, sync;
    bool playing, failed;
    MDRV_EVENT_CALLBACK callback;
    void *context;
} MDRV_SEQUENCE;

// Interprets immutable MDT 2.3 and MDE 1.0 assets into ordered events.
// Derives commands from Regulus's MDRV2 3.4f documentation, compiled author
// examples, and TH01's driver analysis. Consults HertzDevil's MDT decoder
// for command sizes; verifies runtime branches against the original driver.
// Uses no driver source or machine code in the native implementation.
// References: https://www.vector.co.jp/soft/dos/art/se018677.html and
// https://gist.github.com/HertzDevil/036304c692b0f26b7a9d7cfe1126a0ac
//
// Init allocates one lifetime group from allocator. Keep the asset and that
// allocation alive as long as the sequence. Use separate instances for music
// and single-mode effects. Play resets the mutable command copy and counters;
// TriggerEffect uses DOS's 1-based numbering (zero leaves playback unchanged).
// Neither operation nor successful Pulse allocates. Effects interrupt the
// driver's last SSG slots: pointer zero uses SSG2, pointer one SSG1, then SSG0.
// Their identity is the pointer's track index and their channel is its output
// SSG tag. source_channel retains the descriptor tag plus pointer index. A
// future mixer must arbitrate these against music, which continues.
//
// Pulse advances one driver clock, starting at zero on the first call after
// Play/TriggerEffect. The original advances music and effects on timer B IRQs;
// use PulsePair for shared clocks, including effects while music is stopped.
// A clock is 1/192 of a whole note before @T compiler scaling. Raw E9 values
// and global tempo events retain that scaling. Timer reload quantization and
// mapping to samples belong to the later audio producer, never to game ticks.
// Callbacks run synchronously in FM, PCM, rhythm, SSG order, retaining
// directory order within music groups and ascending SSG output order for
// effects. Do not reenter the sequence.
// Events borrow no temporary parameter data beyond the callback's duration.
//
// Starts tracks with Q8, zero detune/transpose, centered pan and zero volume
// and program. Retains compiled volume bytes and encoded SSG program/mode bits.
// An FM program sets pan to center. A music FF holds a Q0 note until the
// remaining tracks end; effect FF silences that effect channel. Header repeat
// resets Q/pan/detune/transpose and retains volume, program and ownership,
// together with the mutable shared command counters. Fresh Play clears all.
// Merges adjacent same-pitch ties before Q arithmetic. A differing-pitch tie
// marks the following note legato and suppresses gate key-off. Q0 holds notes;
// other Q values key off at floor(duration*Q/8)-1 clocks from note onset.
// A duration byte zero advances no clock, as in TH01's driver. A portamento
// retains its encoded starting pitch, duration and signed chip-domain delta;
// its evolution is deferred to the synth, as are LFO/envelope/fade evolution.
// F3 jumps relative to its last operand byte. FA calls an absolute record
// pointer with one return slot. F6/F7 counters share a player-local byte copy.
// E0/E4 are single-level; F6 permits nested repeats. FF returns from FA, ends
// a track, or restarts all music tracks when header flag 80 requests repeat.
// F3 emits LOOP and updates sync to FE; completion sets sync FF, F8 sets it
// raw.
//
// Stop emits key-offs then STOP. Pulse errors preserve delivered events,
// stop all tracks, emit key-offs and STOP, and latch failed until Play/Trigger.
// Rejects invalid offsets, parameters, unsupported opcodes,
// nested FA/E0/E4 and streams exceeding 4096 commands per track per pulse.
// Emits FE as an EXTENDED event with zero operands, as TH01's dispatch does.
// No unbounded parsing occurs in Pulse, including malformed zero-time loops.
// Public state describes sequencing, not chip registers or audible output.
void MdrvSeq_InitMusic(
    MDRV_SEQUENCE *sequence, const MDRV_MUSIC *music,
    const ALLOCATOR *allocator, MDRV_EVENT_CALLBACK callback, void *context);
void MdrvSeq_InitEffects(
    MDRV_SEQUENCE *sequence, const MDRV_EFFECTS *effects,
    const ALLOCATOR *allocator, MDRV_EVENT_CALLBACK callback, void *context);
RESULT MdrvSeq_Play(MDRV_SEQUENCE *sequence);
RESULT MdrvSeq_TriggerEffect(MDRV_SEQUENCE *sequence, uint16_t number);
RESULT MdrvSeq_Pulse(MDRV_SEQUENCE *sequence);

// Advances music and single-mode effects on one shared driver clock. Requires
// one music instance and one effect instance, both initialized, distinct and
// not failed. Allows either or both to be stopped. Timestamps remain local to
// each Play/Trigger; callback order provides their ordering on this clock.
// Runs music FM, PCM, rhythm, then each SSG output in ascending order, with
// its effect before its music. Music still advances while its SSG is owned.
// Shares tempo and sync in that order; both instances hold the final globals
// on return. Effect completion alone does not signal global music completion.
// Emits OUTPUT_MASK with parameter 1 on takeover and 0 on release; persistent
// music state and control events continue while masked. Marks ordinary SSG
// output events suppressed, leaving raw REGISTER and global control events
// unmasked. Synth code must maintain music envelope/LFO state during masking.
// Keeps an ending channel masked through its music step while another effect
// remains active. The last active effect releases before that clock's music
// step; earlier channels release on the next clock. OUTPUT_MASK release permits
// subsequent music output;
// it neither restores a cached register snapshot nor emits a replacement note.
// Stop/retrigger actions between calls emit immediately. A failed advance
// stops both instances and latches both failed. Standalone Pulse removes
// paired output masks and advances just that instance.
RESULT MdrvSeq_PulsePair(MDRV_SEQUENCE *music, MDRV_SEQUENCE *effects);
void MdrvSeq_Stop(MDRV_SEQUENCE *sequence);
