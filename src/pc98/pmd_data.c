#include "core/base/cp932.h"
#include "pc98/pmd/data.h"

#include <string.h>

#define M_EFFECT_HEADER_SIZE 256
#define M_SONG_HEADER_SIZE 26
#define M_SONG_TRAILER_SIZE 12
#define M_RECORD_SIZE (PMD_INSTRUMENT_SIZE + 1)

static RESULT M_ParseSong(
    const uint8_t *const bytes, const size_t size, PMD_SONG *const music)
{
    READER reader;
    Reader_OpenBuffer(&reader, bytes, size, nullptr);
    FAIL_IF(Reader_ReadU8(&reader) != 0, "Unsupported PMD sound format");
    for (size_t i = 0; i < PMD_TRACK_COUNT; i++) {
        music->tracks[i] = Reader_ReadU16(&reader);
    }
    const uint16_t rhythm = Reader_ReadU16(&reader);
    music->instrument_offset = Reader_ReadU16(&reader);
    MUST(Reader_Result(&reader));
    FAIL_IF(music->tracks[0] != M_SONG_HEADER_SIZE, "Unsupported PMD header");
    FAIL_IF(
        music->instrument_offset < M_SONG_HEADER_SIZE + M_SONG_TRAILER_SIZE
            || (size_t)music->instrument_offset + 3 > size,
        "Invalid PMD instrument offset");
    music->command_end = music->instrument_offset - M_SONG_TRAILER_SIZE;
    for (size_t i = 0; i < PMD_TRACK_COUNT; i++) {
        FAIL_IF(
            music->tracks[i] < M_SONG_HEADER_SIZE
                || music->tracks[i] >= music->command_end,
            "Invalid PMD track offset");
    }
    FAIL_IF(
        rhythm < M_SONG_HEADER_SIZE || rhythm > music->command_end,
        "Invalid PMD rhythm table");
    if (rhythm < music->command_end) {
        Reader_Seek(&reader, (size_t)rhythm + 1);
        const uint16_t first = Reader_ReadU16(&reader);
        MUST(Reader_Result(&reader));
        FAIL_IF(
            first <= rhythm || first >= music->command_end
                || (first - rhythm) % 2 != 0
                || (first - rhythm) / 2 > PMD_RHYTHM_LIMIT,
            "Invalid PMD rhythm extent");
        music->rhythm_count = (first - rhythm) / 2;
        Reader_Seek(&reader, (size_t)rhythm + 1);
        for (uint16_t i = 0; i < music->rhythm_count; i++) {
            music->rhythm[i] = Reader_ReadU16(&reader);
            FAIL_IF(
                music->rhythm[i] < first
                    || music->rhythm[i] >= music->command_end,
                "Invalid PMD rhythm offset");
        }
    }
    Reader_Seek(&reader, (size_t)music->command_end + 1);
    Reader_Skip(&reader, 8);
    const uint16_t memo = Reader_ReadU16(&reader);
    const uint16_t version = Reader_ReadU16(&reader);
    MUST(Reader_Result(&reader));
    FAIL_IF(version != 0xfe48, "Unsupported PMD compiler profile");
    FAIL_IF(
        memo < (size_t)music->instrument_offset + 2 || (size_t)memo + 3 > size,
        "Invalid PMD memo table");
    while (true) {
        const size_t position = Reader_Tell(&reader);
        FAIL_IF(
            position + 2 > (size_t)memo + 1, "Missing PMD voice end marker");
        if (Reader_ReadU16(&reader) == 0xff00) {
            break;
        }
        FAIL_IF(
            position + M_RECORD_SIZE > (size_t)memo + 1,
            "Incomplete PMD voice");
        Reader_Seek(&reader, position + M_RECORD_SIZE);
        music->instrument_count++;
    }
    const size_t text_start = Reader_Tell(&reader);
    Reader_Seek(&reader, (size_t)memo + 1);
    uint16_t memo_count = 0;
    while (true) {
        const uint16_t offset = Reader_ReadU16(&reader);
        MUST(Reader_Result(&reader));
        if (offset == 0) {
            break;
        }
        FAIL_IF(memo_count == PMD_MEMO_LIMIT, "Too many PMD memos");
        FAIL_IF(
            (size_t)offset + 1 < text_start || offset >= memo,
            "Invalid PMD memo offset");
        const char *const begin = (const char *)bytes + offset + 1;
        const char *const end = memchr(begin, 0, memo - offset);
        FAIL_IF(end == nullptr, "Unterminated PMD memo");
        memo_count++;
        if (end != begin) {
            char *decoded;
            MUST(Cp932_Decode(begin, (size_t)(end - begin), &decoded));
            Mem_Free(decoded);
        }
    }
    FAIL_IF(Reader_Remaining(&reader) != 0, "Unexpected PMD trailing data");
    return OK;
}

RESULT PmdData_LoadSong(
    READER *const reader, const ALLOCATOR *const allocator,
    PMD_SONG **const out)
{
    *out = nullptr;
    MUST(Reader_Result(reader));
    const size_t size = Reader_Remaining(reader);
    FAIL_IF(
        size < 1 + M_SONG_HEADER_SIZE + M_SONG_TRAILER_SIZE + 2 || size > 65536,
        "Invalid PMD music size");
    uint8_t *const bytes = Reader_ReadAlloc(reader, size, &g_HeapAllocator);
    RESULT result = Reader_Result(reader);
    PMD_SONG music = {};
    if (IS_OK(result)) {
        result = M_ParseSong(bytes, size, &music);
    }
    if (IS_OK(result)) {
        PMD_SONG *const song = Allocator_Alloc(allocator, sizeof(*song) + size);
        uint8_t *const storage = (uint8_t *)(song + 1);
        memcpy(storage, bytes, size);
        music.bytes = storage;
        music.size = size;
        *song = music;
        *out = song;
    }
    Mem_Free(bytes);
    return result;
}

RESULT PmdData_LoadEffects(
    READER *const reader, const ALLOCATOR *const allocator,
    PMD_EFFECTS **const out)
{
    *out = nullptr;
    MUST(Reader_Result(reader));
    const size_t start = Reader_Tell(reader);
    const size_t size = Reader_Remaining(reader);
    FAIL_IF(
        size < M_EFFECT_HEADER_SIZE + 3 || size > 65536, "Invalid EFC size");
    PMD_EFFECTS bank = {};
    for (size_t i = 0; i < PMD_EFFECT_COUNT; i++) {
        bank.offsets[i] = Reader_ReadU16(reader);
    }
    bank.instrument_offset = Reader_ReadU16(reader);
    MUST(Reader_Result(reader));
    FAIL_IF(
        bank.instrument_offset < M_EFFECT_HEADER_SIZE + 1
            || bank.instrument_offset > size - 2,
        "Invalid EFC instrument offset");
    for (size_t i = 0; i < PMD_EFFECT_COUNT; i++) {
        FAIL_IF(
            bank.offsets[i] < M_EFFECT_HEADER_SIZE
                || bank.offsets[i] >= bank.instrument_offset,
            "Invalid EFC effect offset");
    }
    const size_t instrument_size = size - bank.instrument_offset - 2;
    FAIL_IF(
        instrument_size % M_RECORD_SIZE != 0,
        "Incomplete EFC instrument record");
    Reader_Seek(reader, start + size - 2);
    const uint16_t marker = Reader_ReadU16(reader);
    MUST(Reader_Result(reader));
    FAIL_IF(marker != 0xff00, "Invalid EFC instrument end marker");
    Reader_Seek(reader, start);
    uint8_t *const bytes = Reader_ReadAlloc(reader, size, &g_HeapAllocator);
    const RESULT result = Reader_Result(reader);
    if (!IS_OK(result)) {
        Mem_Free(bytes);
        return result;
    }
    PMD_EFFECTS *const effects =
        Allocator_Alloc(allocator, sizeof(*effects) + size);
    bank.bytes = (uint8_t *)(effects + 1);
    bank.size = size;
    bank.instrument_count = (uint16_t)(instrument_size / M_RECORD_SIZE);
    memcpy((void *)bank.bytes, bytes, size);
    *effects = bank;
    Mem_Free(bytes);
    *out = effects;
    return OK;
}

const uint8_t *PmdData_Instrument(
    const PMD_EFFECTS *const bank, const uint8_t id)
{
    if (bank == nullptr || bank->bytes == nullptr) {
        return nullptr;
    }
    const uint8_t *record = bank->bytes + bank->instrument_offset;
    for (uint16_t i = 0; i < bank->instrument_count; i++) {
        if (*record == id) {
            return record + 1;
        }
        record += M_RECORD_SIZE;
    }
    return nullptr;
}

const uint8_t *PmdData_SongInstrument(
    const PMD_SONG *const music, const uint8_t id)
{
    if (music == nullptr || music->bytes == nullptr) {
        return nullptr;
    }
    const uint8_t *voice = music->bytes + music->instrument_offset + 1;
    for (uint16_t i = 0; i < music->instrument_count; i++) {
        if (voice[0] == id) {
            return voice + 1;
        }
        voice += M_RECORD_SIZE;
    }
    return nullptr;
}
