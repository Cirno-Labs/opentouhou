#include "th01/pf.h"

#include "core/base/cp932.h"
#include "core/base/string.h"

#include <stdckdint.h>
#include <string.h>

#define M_ENTRY_COUNT 64
#define M_NAME_SIZE 13
#define M_UTF8_NAME_SIZE ((M_NAME_SIZE - 1) * 3 + 1)
#define M_KEY 0x76

typedef struct {
    char name[M_UTF8_NAME_SIZE];
    uint32_t offset, packed_size, decoded_size;
    bool compressed;
} PF_ENTRY;

typedef struct {
    VFS_SOURCE base;
    READER archive;
    PF_ENTRY entries[M_ENTRY_COUNT];
    int32_t count;
    const ALLOCATOR *allocator;
    char name[];
} PF_SOURCE;

static void M_FoldAscii(char *const text)
{
    for (char *c = text; *c != '\0'; c++) {
        if (*c >= 'A' && *c <= 'Z') {
            *c += 'a' - 'A';
        }
    }
}

static RESULT M_ReadName(READER *const reader, char out[M_UTF8_NAME_SIZE])
{
    char bytes[M_NAME_SIZE];
    size_t length = 0;
    bool ended = false;
    for (size_t i = 0; i < M_NAME_SIZE; i++) {
        const uint8_t byte = Reader_ReadU8(reader);
        if (!ended) {
            if (byte == 0) {
                ended = true;
            } else {
                bytes[length++] = (char)~byte;
            }
        }
    }
    FAIL_IF(!ended || length == 0, "invalid entry name");
    char *decoded;
    MUST(Cp932_Decode(bytes, length, &decoded));
    const size_t size = strlen(decoded) + 1;
    if (size > M_UTF8_NAME_SIZE) {
        Mem_Free(decoded);
        return FAIL("entry name is too long");
    }
    memcpy(out, decoded, size);
    Mem_Free(decoded);
    M_FoldAscii(out);
    FAIL_IF(
        strchr(out, '/') != nullptr || strchr(out, '\\') != nullptr
            || strchr(out, ':') != nullptr || strcmp(out, ".") == 0
            || strcmp(out, "..") == 0,
        "invalid entry path");
    return OK;
}

static RESULT M_ReadTable(PF_SOURCE *const source)
{
    Reader_Seek(&source->archive, 0);
    for (int32_t i = 0; i < M_ENTRY_COUNT; i++) {
        FAIL_IF(
            Reader_Remaining(&source->archive) < 32, "directory is truncated");
        const uint8_t type = Reader_ReadU8(&source->archive);
        const uint8_t second = Reader_ReadU8(&source->archive);
        Reader_ReadU8(&source->archive);
        if (type == 0) {
            Reader_Skip(&source->archive, 29);
            break;
        }
        PF_ENTRY *const entry = &source->entries[source->count++];
        MUST(M_ReadName(&source->archive, entry->name));
        entry->packed_size = Reader_ReadU32(&source->archive);
        entry->decoded_size = Reader_ReadU32(&source->archive);
        entry->offset = Reader_ReadU32(&source->archive);
        Reader_ReadU32(&source->archive);
        MUST(Reader_Result(&source->archive));
        entry->compressed = type == 0x95 && second == 0x95;
        FAIL_IF(
            entry->packed_size > UINT16_MAX || entry->decoded_size > UINT16_MAX,
            "%s: entry exceeds 16-bit byte counts", entry->name);
        FAIL_IF(
            entry->offset > Reader_Size(&source->archive)
                || entry->packed_size
                    > Reader_Size(&source->archive) - entry->offset,
            "%s: entry range is outside the archive", entry->name);
        FAIL_IF(
            !entry->compressed && entry->packed_size != entry->decoded_size,
            "%s: uncompressed sizes differ", entry->name);
    }
    for (int32_t i = 0; i < source->count; i++) {
        FAIL_IF(
            source->entries[i].offset < Reader_Tell(&source->archive),
            "%s: entry overlaps the directory", source->entries[i].name);
    }
    return OK;
}

static uint8_t M_ReadByte(READER *const reader)
{
    return Reader_ReadU8(reader) ^ M_KEY;
}

static RESULT M_DecodeRle(
    READER *const reader, const PF_ENTRY *const entry, uint8_t *const data)
{
    if (entry->decoded_size == 0) {
        return OK;
    }
    size_t written = 0;
    uint8_t value = M_ReadByte(reader);
    while (Reader_Tell(reader) < entry->packed_size && !Reader_Failed(reader)) {
        uint8_t previous;
        do {
            previous = value;
            data[written++] = value;
            if (written == entry->decoded_size) {
                return OK;
            }
            value = M_ReadByte(reader);
            MUST(Reader_Result(reader));
        } while (previous != value);
        data[written++] = value;
        if (written == entry->decoded_size) {
            return OK;
        }
        for (;;) {
            const uint8_t count = M_ReadByte(reader);
            MUST(Reader_Result(reader));
            const size_t remaining = entry->decoded_size - written;
            const size_t copies = count < remaining ? count : remaining;
            memset(data + written, previous, copies);
            written += copies;
            if (written == entry->decoded_size) {
                return OK;
            }
            value = M_ReadByte(reader);
            MUST(Reader_Result(reader));
            if (value != previous) {
                break;
            }
            data[written++] = previous;
            if (written == entry->decoded_size) {
                return OK;
            }
        }
    }
    MUST(Reader_Result(reader));
    return FAIL("decoded data is shorter than recorded");
}

static const PF_ENTRY *M_Find(
    const PF_SOURCE *const source, const char *const path)
{
    for (int32_t i = 0; i < source->count; i++) {
        if (Str_EqualIgnoreCase(source->entries[i].name, path)) {
            return &source->entries[i];
        }
    }
    return nullptr;
}

static bool M_Contains(const VFS_SOURCE *const source, const char *const path)
{
    return M_Find((const PF_SOURCE *)source, path) != nullptr;
}

static RESULT M_Open(
    VFS_SOURCE *const base, const char *const path,
    [[maybe_unused]] const VFS_MODE mode, READER *const out)
{
    *out = (READER) {};
    PF_SOURCE *const source = (PF_SOURCE *)base;
    const PF_ENTRY *const entry = M_Find(source, path);
    FAIL_IF(entry == nullptr, "%s: %s: not found", source->name, path);
    uint8_t *const data =
        Allocator_Alloc(source->allocator, entry->decoded_size);
    Reader_Seek(&source->archive, entry->offset);
    READER input = Reader_Slice(
        &source->archive, Reader_Size(&source->archive) - entry->offset);
    RESULT result = OK;
    if (entry->compressed) {
        result = M_DecodeRle(&input, entry, data);
    } else {
        Reader_ReadBytes(&input, data, entry->decoded_size);
        result = Reader_Result(&input);
        if (IS_OK(result)) {
            for (size_t i = 0; i < entry->decoded_size; i++) {
                data[i] ^= M_KEY;
            }
        }
    }
    if (!IS_OK(result)) {
        Allocator_Free(source->allocator, data, entry->decoded_size);
        return Result_Prefix(result, "%s: %s", source->name, path);
    }
    Reader_OpenOwnedBuffer(
        out, data, entry->decoded_size, source->allocator, nullptr);
    return OK;
}

static void M_List(
    const VFS_SOURCE *const base, const char *const dir,
    void (*const visit)(void *user, const char *name), void *const user)
{
    if (dir[0] != '\0') {
        return;
    }
    const PF_SOURCE *const source = (const PF_SOURCE *)base;
    for (int32_t i = 0; i < source->count; i++) {
        visit(user, source->entries[i].name);
    }
}

static void M_Close(VFS_SOURCE *const base)
{
    PF_SOURCE *const source = (PF_SOURCE *)base;
    Reader_Close(&source->archive);
    const size_t size = sizeof(*source) + strlen(source->name) + 1;
    Allocator_Free(source->allocator, source, size);
}

static const VFS_SOURCE_FUNCS m_Funcs = {
    .contains = M_Contains,
    .open = M_Open,
    .list = M_List,
    .close = M_Close,
};

RESULT Pf_Open(
    READER *const archive, const char *const name,
    const ALLOCATOR *const allocator, VFS_SOURCE **const out)
{
    *out = nullptr;
    PF_SOURCE parsed = { .archive = *archive };
    const RESULT result = M_ReadTable(&parsed);
    if (!IS_OK(result)) {
        RESULT failure = Result_Prefix(result, "%s", name);
        Reader_Close(&parsed.archive);
        return failure;
    }
    const size_t name_size = strlen(name) + 1;
    size_t size;
    if (ckd_add(&size, sizeof(PF_SOURCE), name_size)) {
        Reader_Close(&parsed.archive);
        return FAIL("Archive name is too long");
    }
    PF_SOURCE *const source = Allocator_Alloc(allocator, size);
    *source = parsed;
    source->base.funcs = &m_Funcs;
    memcpy(source->name, name, name_size);
    source->base.name = source->name;
    source->allocator = allocator;
    *out = &source->base;
    return OK;
}
