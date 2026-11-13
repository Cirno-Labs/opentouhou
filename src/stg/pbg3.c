// SPDX-License-Identifier: GPL-3.0-only

#include "stg/pbg3.h"

#include "core/base/array.h"
#include "core/base/cp932.h"
#include "core/base/hash_map.h"
#include "core/base/memory.h"
#include "core/base/string.h"

#include <string.h>

#define M_MAGIC 0x33474250u
#define M_MAX_NAME_LENGTH 255
#define M_DICT_SIZE 0x2000u

typedef struct {
    char *name;
    uint32_t checksum;
    uint32_t data_offset;
    uint32_t data_size;
    uint32_t decoded_size;
} PBG3_ENTRY;

typedef ARRAY(PBG3_ENTRY) PBG3_ENTRY_ARRAY;
typedef HASH_MAP(char *, int32_t) PBG3_ENTRY_INDEX;

typedef struct {
    VFS_SOURCE base;
    READER archive;
    PBG3_ENTRY_ARRAY entries;
    PBG3_ENTRY_INDEX by_name;
    char *name;
    const ALLOCATOR *allocator;
} PBG3_SOURCE;

// Reads bits most significant bit first out of a READER, and keeps the
// wrapping sum of every byte it fetches from the reader, for the entry
// decoder's checksum.
typedef struct {
    READER *reader;
    uint32_t byte;
    int32_t bits_left;
    uint32_t checksum;
} BIT_READER;

static void M_BitReader_Init(BIT_READER *const br, READER *const reader)
{
    br->reader = reader;
    br->byte = 0;
    br->bits_left = 0;
    br->checksum = 0;
}

static uint32_t M_ReadBits(BIT_READER *const br, const int32_t count)
{
    uint32_t value = 0;
    for (int32_t i = 0; i < count; i++) {
        if (br->bits_left == 0) {
            br->byte = Reader_ReadU8(br->reader);
            br->checksum += br->byte;
            br->bits_left = 8;
        }
        br->bits_left--;
        value = (value << 1) | ((br->byte >> br->bits_left) & 1);
    }
    return value;
}

// Reads a varint: 2 bits giving the byte count minus 1, then that many
// bytes, most significant bit first.
static uint32_t M_ReadVarint(BIT_READER *const br)
{
    const uint32_t byte_count = M_ReadBits(br, 2) + 1;
    return M_ReadBits(br, (int32_t)byte_count * 8);
}

static RESULT M_ReadName(BIT_READER *const br, char **const out)
{
    uint8_t bytes[M_MAX_NAME_LENGTH];
    int32_t length = 0;
    for (;;) {
        const uint8_t byte = (uint8_t)M_ReadBits(br, 8);
        if (byte == 0) {
            break;
        }
        FAIL_IF(length >= M_MAX_NAME_LENGTH, "entry name is too long");
        bytes[length++] = byte;
    }
    return Cp932_Decode((const char *)bytes, (size_t)length, out);
}

// Folds the ASCII letters of text to lower case in place, matching how the
// VFS normalises the paths that contains and open receive.
static void M_FoldAscii(char *const text)
{
    for (char *c = text; *c != '\0'; c++) {
        if (*c >= 'A' && *c <= 'Z') {
            *c = (char)(*c - 'A' + 'a');
        }
    }
}

static const char *M_LastComponent(const char *const path)
{
    const char *const slash = strrchr(path, '/');
    return slash == nullptr ? path : slash + 1;
}

static RESULT M_ReadTable(
    PBG3_SOURCE *const source, const uint32_t entry_count,
    const uint32_t table_offset)
{
    Reader_Seek(&source->archive, table_offset);
    BIT_READER br;
    M_BitReader_Init(&br, &source->archive);

    for (uint32_t i = 0; i < entry_count; i++) {
        M_ReadVarint(&br); // unk1, unused
        M_ReadVarint(&br); // unk0, unused
        const uint32_t checksum = M_ReadVarint(&br);
        const uint32_t data_offset = M_ReadVarint(&br);
        const uint32_t decoded_size = M_ReadVarint(&br);
        char *name;
        const RESULT name_result = M_ReadName(&br, &name);
        if (!IS_OK(name_result)) {
            return Result_Prefix(name_result, "%s", source->name);
        }
        if (Reader_Failed(&source->archive)) {
            Mem_Free(name);
            return FAIL(
                "%s: table runs past the end of the archive", source->name);
        }

        PBG3_ENTRY *const entry = Array_AddOne(&source->entries);
        entry->name = name;
        entry->checksum = checksum;
        entry->data_offset = data_offset;
        entry->decoded_size = decoded_size;
        entry->data_size = 0;
    }

    for (int32_t i = 0; i < source->entries.count; i++) {
        PBG3_ENTRY *const entry = &source->entries.items[i];
        const uint32_t end = i + 1 < source->entries.count
            ? source->entries.items[i + 1].data_offset
            : table_offset;
        FAIL_IF(
            end < entry->data_offset, "%s: %s: entry data overlaps the table",
            source->name, entry->name);
        entry->data_size = end - entry->data_offset;

        char *const key = Str_Dup(entry->name);
        M_FoldAscii(key);
        HashMap_Put(&source->by_name, key, i);
        Mem_Free(key);
    }
    return OK;
}

// Decompresses the LZSS stream of one entry's data into a new block of
// exactly entry->decoded_size bytes.
static RESULT M_Decode(
    PBG3_SOURCE *const source, const PBG3_ENTRY *const entry, void **const out)
{
    *out = nullptr;
    Reader_Seek(&source->archive, entry->data_offset);
    READER slice = Reader_Slice(&source->archive, entry->data_size);
    BIT_READER br;
    M_BitReader_Init(&br, &slice);

    uint8_t dict[M_DICT_SIZE] = { 0 };
    uint32_t dict_pos = 1;
    uint8_t *const decoded =
        Allocator_Alloc(source->allocator, entry->decoded_size);
    uint32_t decoded_count = 0;

    while (decoded_count < entry->decoded_size) {
        if (M_ReadBits(&br, 1) != 0) {
            const uint8_t byte = (uint8_t)M_ReadBits(&br, 8);
            decoded[decoded_count++] = byte;
            dict[dict_pos] = byte;
            dict_pos = (dict_pos + 1) & (M_DICT_SIZE - 1);
            continue;
        }
        const uint32_t offset = M_ReadBits(&br, 13);
        if (offset == 0) {
            break;
        }
        const uint32_t length = M_ReadBits(&br, 4) + 3;
        for (uint32_t i = 0; i < length; i++) {
            if (decoded_count >= entry->decoded_size) {
                Allocator_Free(source->allocator, decoded, entry->decoded_size);
                return FAIL(
                    "%s: %s: decoded data is longer than recorded",
                    source->name, entry->name);
            }
            const uint8_t byte = dict[(offset + i) & (M_DICT_SIZE - 1)];
            decoded[decoded_count++] = byte;
            dict[dict_pos] = byte;
            dict_pos = (dict_pos + 1) & (M_DICT_SIZE - 1);
        }
    }

    if (decoded_count != entry->decoded_size) {
        Allocator_Free(source->allocator, decoded, entry->decoded_size);
        return FAIL(
            "%s: %s: decoded data is shorter than recorded", source->name,
            entry->name);
    }
    if (br.checksum != entry->checksum) {
        Allocator_Free(source->allocator, decoded, entry->decoded_size);
        return FAIL("%s: %s: checksum mismatch", source->name, entry->name);
    }
    *out = decoded;
    return OK;
}

static const PBG3_ENTRY *M_Find(
    const PBG3_SOURCE *const source, const char *const path)
{
    const char *const component = M_LastComponent(path);
    char *const key = Str_Dup(component);
    M_FoldAscii(key);
    const int32_t *const index = HashMap_Get(&source->by_name, key);
    Mem_Free(key);
    return index == nullptr ? nullptr : &source->entries.items[*index];
}

static bool M_Contains(const VFS_SOURCE *const source, const char *const path)
{
    return M_Find((const PBG3_SOURCE *)source, path) != nullptr;
}

static RESULT M_Open(
    VFS_SOURCE *const source, const char *const path,
    [[maybe_unused]] const VFS_MODE mode, READER *const out)
{
    PBG3_SOURCE *const pbg3 = (PBG3_SOURCE *)source;
    const PBG3_ENTRY *const entry = M_Find(pbg3, path);
    FAIL_IF(entry == nullptr, "%s: %s: not found", pbg3->name, path);
    void *data;
    MUST(M_Decode(pbg3, entry, &data), "%s: %s", pbg3->name, path);
    Reader_OpenOwnedBuffer(out, data, entry->decoded_size, pbg3->allocator);
    return OK;
}

static void M_List(
    const VFS_SOURCE *const source,
    void (*const visit)(void *user, const char *path), void *const user)
{
    const PBG3_SOURCE *const pbg3 = (const PBG3_SOURCE *)source;
    for (int32_t i = 0; i < pbg3->entries.count; i++) {
        char *const name = Str_Dup(pbg3->entries.items[i].name);
        M_FoldAscii(name);
        visit(user, name);
        Mem_Free(name);
    }
}

static void M_Close(VFS_SOURCE *const source)
{
    PBG3_SOURCE *const pbg3 = (PBG3_SOURCE *)source;
    for (int32_t i = 0; i < pbg3->entries.count; i++) {
        Mem_Free(pbg3->entries.items[i].name);
    }
    Array_Free(&pbg3->entries);
    HashMap_Free(&pbg3->by_name);
    Reader_Close(&pbg3->archive);
    Mem_Free(pbg3->name);
    Allocator_Free(pbg3->allocator, pbg3, sizeof(*pbg3));
}

static const VFS_SOURCE_FUNCS M_Funcs = {
    .contains = M_Contains,
    .open = M_Open,
    .list = M_List,
    .close = M_Close,
};

RESULT Pbg3_Open(
    READER *const archive, const char *const name,
    const ALLOCATOR *const allocator, VFS_SOURCE **const out)
{
    *out = nullptr;
    uint32_t magic;
    Reader_Read(archive, &magic);
    if (magic != M_MAGIC || Reader_Failed(archive)) {
        const RESULT result = FAIL("%s: bad magic", name);
        Reader_Close(archive);
        return result;
    }

    BIT_READER br;
    M_BitReader_Init(&br, archive);
    const uint32_t entry_count = M_ReadVarint(&br);
    const uint32_t table_offset = M_ReadVarint(&br);
    if (Reader_Failed(archive)) {
        const RESULT result = FAIL("%s: header runs past the end", name);
        Reader_Close(archive);
        return result;
    }

    PBG3_SOURCE *const source = Allocator_AllocZ(allocator, sizeof(*source));
    source->base.funcs = &M_Funcs;
    source->archive = *archive;
    source->name = Str_Dup(name);
    source->base.name = source->name;
    source->allocator = allocator;
    Array_Init(&source->entries, allocator);
    HashMap_Init(&source->by_name, allocator);

    const RESULT result = M_ReadTable(source, entry_count, table_offset);
    if (!IS_OK(result)) {
        M_Close(&source->base);
        return result;
    }
    *out = &source->base;
    return OK;
}
