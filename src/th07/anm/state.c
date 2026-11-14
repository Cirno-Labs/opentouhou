#include "th07/anm.h"
#include "th07/anm/private.h"

#include "th07/files.h"

#include "shell/game.h"
#include "core/vfs.h"
#include "core/base/assert.h"
#include "core/base/memory.h"
#include "core/io/reader.h"

#include <string.h>

#define M_HEADER_SIZE 0x40
#define M_INSTR_HEADER_SIZE 8
#define M_VERSION 2

typedef enum {
    M_OP_END = -1,
    M_OP_JUMP = 4,
    M_OP_LOOP = 5,
    M_OP_JUMP_IF_FIRST = 67,
    M_OP_JUMP_IF_LAST = 78,
} M_OP;

// Owns its scripts, so a VM keeps running one after the table forgets it.
// Entries a failed load did not reach are -1 or empty scripts.
typedef struct {
    bool loaded;
    int32_t count; // slots of the chain; 0 past its first slot
    int32_t sprite_count;
    int32_t *sprites;
    int32_t script_count;
    int32_t *script_numbers;
    ANM_SCRIPT *scripts;
} M_SLOT;

typedef struct {
    ANM_SPRITE sprites[ANM_SPRITE_COUNT];
    const ANM_SCRIPT *scripts[ANM_SCRIPT_COUNT];
    M_SLOT slots[ANM_SLOT_COUNT];
    int32_t next_sprite_id;
} M_STATE;

static M_STATE *m_State;
static const ALLOCATOR *m_Allocator;
static RNG *m_Rng;

static int32_t M_JumpArg(const int16_t opcode)
{
    if (opcode == M_OP_JUMP) {
        return 0;
    }
    if (opcode == M_OP_LOOP) {
        return 1;
    }
    if (opcode >= M_OP_JUMP_IF_FIRST && opcode <= M_OP_JUMP_IF_LAST) {
        return 2;
    }
    return -1;
}

// Returns the instruction's size, or 0 when it is malformed. The end
// marker's size field is unused.
static size_t M_ReadInstr(READER *const reader, ANM_INSTR *const instr)
{
    *instr = (ANM_INSTR) {};
    Reader_Read(reader, &instr->opcode);
    const uint16_t size = Reader_ReadU16(reader);
    Reader_Read(reader, &instr->time);
    Reader_Read(reader, &instr->param_mask);
    if (instr->opcode == M_OP_END) {
        return Reader_Failed(reader) ? 0 : M_INSTR_HEADER_SIZE;
    }
    if (size < M_INSTR_HEADER_SIZE || size % 4 != 0
        || (size - M_INSTR_HEADER_SIZE) / 4 > ANM_INSTR_ARG_COUNT) {
        return 0;
    }
    for (int32_t i = 0; i < (size - M_INSTR_HEADER_SIZE) / 4; i++) {
        Reader_Read(reader, &instr->args[i].i);
    }
    return Reader_Failed(reader) ? 0 : size;
}

static RESULT M_ParseScript(
    READER *const reader, const size_t offset, ANM_SCRIPT *const script)
{
    Reader_Seek(reader, offset);
    int32_t count = 0;
    ANM_INSTR instr;
    do {
        FAIL_IF(
            M_ReadInstr(reader, &instr) == 0,
            "instruction %d of the script at %zu is malformed", count, offset);
        count++;
    } while (instr.opcode != M_OP_END);

    ANM_INSTR *const instrs =
        Allocator_Alloc(m_Allocator, (size_t)count * sizeof(ANM_INSTR));
    uint32_t *const offsets = Mem_AllocArr((size_t)count, sizeof(uint32_t));
    Reader_Seek(reader, offset);
    uint32_t position = 0;
    for (int32_t i = 0; i < count; i++) {
        offsets[i] = position;
        position += (uint32_t)M_ReadInstr(reader, &instrs[i]);
    }
    *script = (ANM_SCRIPT) { .instrs = instrs, .count = count };

    RESULT result = OK;
    for (int32_t i = 0; i < count && IS_OK(result); i++) {
        const int32_t arg = M_JumpArg(instrs[i].opcode);
        if (arg < 0) {
            continue;
        }
        const uint32_t jump = (uint32_t)instrs[i].args[arg].i;
        int32_t target = 0;
        while (target < count && offsets[target] != jump) {
            target++;
        }
        if (target == count) {
            result = FAIL("jump to %u is not an instruction", jump);
        }
        instrs[i].args[arg].i = target;
    }
    Mem_Free(offsets);
    return result;
}

// Sets advance to one more than the highest sprite or script number.
static RESULT M_Parse(
    READER *const reader, const int32_t slot, const int32_t first_sprite,
    int32_t *const advance)
{
    M_SLOT *const loaded = &m_State->slots[slot];
    loaded->loaded = true;
    const int32_t sprite_count = Reader_ReadI32(reader);
    const int32_t script_count = Reader_ReadI32(reader);
    Reader_Skip(reader, 4);
    const int32_t width = Reader_ReadI32(reader);
    const int32_t height = Reader_ReadI32(reader);
    Reader_Seek(reader, 40);
    const int32_t version = Reader_ReadI32(reader);
    FAIL_IF(
        Reader_Failed(reader) || Reader_Size(reader) < M_HEADER_SIZE
            || sprite_count < 0 || script_count < 0 || width <= 0
            || height <= 0,
        "bad header");
    FAIL_IF(version != M_VERSION, "version %d is not %d", version, M_VERSION);
    FAIL_IF(
        4 * (uint64_t)sprite_count + 8 * (uint64_t)script_count
            > Reader_Size(reader) - M_HEADER_SIZE,
        "the sprite and script tables run past the file");
    MUST(AnmTexture_LoadSlot(reader, slot));
    GFX_TEXTURE *const texture = Anm_GetTexture(slot);
    const float texture_width = (float)GfxTexture_GetWidth(texture);
    const float texture_height = (float)GfxTexture_GetHeight(texture);
    const float scale_x = texture_width / (float)width;
    const float scale_y = texture_height / (float)height;

    loaded->sprites =
        Allocator_Alloc(m_Allocator, (size_t)sprite_count * sizeof(int32_t));
    loaded->script_numbers =
        Allocator_Alloc(m_Allocator, (size_t)script_count * sizeof(int32_t));
    loaded->scripts = Allocator_AllocZ(
        m_Allocator, (size_t)script_count * sizeof(ANM_SCRIPT));
    loaded->sprite_count = sprite_count;
    loaded->script_count = script_count;
    for (int32_t i = 0; i < sprite_count; i++) {
        loaded->sprites[i] = -1;
    }
    for (int32_t i = 0; i < script_count; i++) {
        loaded->script_numbers[i] = -1;
    }

    int32_t highest = 0;
    for (int32_t i = 0; i < sprite_count; i++) {
        Reader_Seek(reader, M_HEADER_SIZE + 4 * (size_t)i);
        Reader_Seek(reader, (size_t)Reader_ReadU32(reader));
        const int32_t id = Reader_ReadI32(reader);
        const float x = Reader_ReadF32(reader);
        const float y = Reader_ReadF32(reader);
        const float w = Reader_ReadF32(reader);
        const float h = Reader_ReadF32(reader);
        const int64_t number = (int64_t)id + first_sprite;
        FAIL_IF(
            Reader_Failed(reader) || number < 0 || number >= ANM_SPRITE_COUNT,
            "bad sprite %d", i);
        if (highest < id) {
            highest = id;
        }
        ANM_SPRITE *const sprite = &m_State->sprites[number];
        *sprite = (ANM_SPRITE) {
            .slot = slot,
            .x0 = scale_x * x,
            .y0 = scale_y * y,
            .x1 = (x + w) * scale_x,
            .y1 = (y + h) * scale_y,
            .texture_width = texture_width,
            .texture_height = texture_height,
            .scale_x = scale_x,
            .scale_y = scale_y,
            .id = m_State->next_sprite_id++,
        };
        sprite->u0 = sprite->x0 / sprite->texture_width;
        sprite->u1 = sprite->x1 / sprite->texture_width;
        sprite->v0 = sprite->y0 / sprite->texture_height;
        sprite->v1 = sprite->y1 / sprite->texture_height;
        sprite->width = (sprite->x1 - sprite->x0) / scale_x;
        sprite->height = (sprite->y1 - sprite->y0) / scale_y;
        loaded->sprites[i] = (int32_t)number;
    }

    for (int32_t i = 0; i < script_count; i++) {
        Reader_Seek(
            reader, M_HEADER_SIZE + 4 * (size_t)sprite_count + 8 * (size_t)i);
        const int32_t id = Reader_ReadI32(reader);
        const size_t offset = Reader_ReadU32(reader);
        const int64_t number = (int64_t)id + first_sprite;
        FAIL_IF(
            Reader_Failed(reader) || number < 0 || number >= ANM_SCRIPT_COUNT,
            "bad script %d", i);
        if (highest < id) {
            highest = id;
        }
        ANM_SCRIPT *const script = &loaded->scripts[i];
        const RESULT result = M_ParseScript(reader, offset, script);
        script->sprite_offset = first_sprite;
        loaded->script_numbers[i] = (int32_t)number;
        m_State->scripts[number] = script;
        MUST(result, "script %d", i);
    }
    *advance = highest + 1;
    return OK;
}

// Sets end to the slot after the last one reached.
static RESULT M_LoadChain(
    READER *const file, const int32_t slot, int32_t first_sprite,
    int32_t *const end)
{
    size_t base = 0;
    for (*end = slot;; (*end)++) {
        FAIL_IF(*end >= ANM_SLOT_COUNT, "ANM %d needs a slot", *end - slot);
        Anm_Release(*end);
        Reader_Seek(file, base);
        READER anm = Reader_Slice(file, Reader_Remaining(file));
        int32_t advance = 0;
        MUST(
            M_Parse(&anm, *end, first_sprite, &advance), "ANM %d", *end - slot);
        Reader_Seek(&anm, 56);
        const int32_t next = Reader_ReadI32(&anm);
        MUST(Reader_Result(&anm), "ANM %d", *end - slot);
        if (next == 0) {
            (*end)++;
            return OK;
        }
        FAIL_IF(
            (int64_t)base + next < 0
                || (int64_t)base + next >= (int64_t)Reader_Size(file),
            "ANM %d is outside the file", *end + 1 - slot);
        base = (size_t)((int64_t)base + next);
        first_sprite += advance;
    }
}

RESULT Anm_Load(
    const int32_t slot, const char *const path, const int32_t first_sprite)
{
    ASSERT(slot >= 0 && slot < ANM_SLOT_COUNT);
    READER reader;
    MUST(Vfs_Open(Shell_GetFiles(), path, VFS_IN_MEMORY, &reader), "%s", path);
    int32_t end;
    const RESULT result = M_LoadChain(&reader, slot, first_sprite, &end);
    Reader_Close(&reader);
    if (IS_OK(result)) {
        m_State->slots[slot].count = end - slot;
    } else {
        for (int32_t i = slot; i <= end && i < ANM_SLOT_COUNT; i++) {
            Anm_Release(i);
        }
    }
    return Result_Prefix(result, "%s", path);
}

void Anm_Release(const int32_t slot)
{
    if (slot < 0 || slot >= ANM_SLOT_COUNT || !m_State->slots[slot].loaded) {
        return;
    }
    M_SLOT *const loaded = &m_State->slots[slot];
    for (int32_t i = 1; i < loaded->count; i++) {
        Anm_Release(slot + i);
    }
    for (int32_t i = 0; i < loaded->sprite_count; i++) {
        if (loaded->sprites[i] >= 0) {
            m_State->sprites[loaded->sprites[i]] = (ANM_SPRITE) { .slot = -1 };
        }
    }
    // As the original, forgets entries another file has since replaced.
    for (int32_t i = 0; i < loaded->script_count; i++) {
        if (loaded->script_numbers[i] >= 0) {
            m_State->scripts[loaded->script_numbers[i]] = nullptr;
        }
        Allocator_Free(
            m_Allocator, loaded->scripts[i].instrs,
            (size_t)loaded->scripts[i].count * sizeof(ANM_INSTR));
    }
    Anm_ReleaseTexture(slot);
    const size_t sprite_count = (size_t)loaded->sprite_count;
    const size_t script_count = (size_t)loaded->script_count;
    Allocator_Free(
        m_Allocator, loaded->sprites, sprite_count * sizeof(int32_t));
    Allocator_Free(
        m_Allocator, loaded->script_numbers, script_count * sizeof(int32_t));
    Allocator_Free(
        m_Allocator, loaded->scripts, script_count * sizeof(ANM_SCRIPT));
    *loaded = (M_SLOT) {};
}

const ANM_SCRIPT *Anm_GetScript(const int32_t index)
{
    if (index < 0 || index >= ANM_SCRIPT_COUNT) {
        return nullptr;
    }
    return m_State->scripts[index];
}

void Anm_SetScriptEntry(const int32_t index, const ANM_SCRIPT *const script)
{
    if (index >= 0 && index < ANM_SCRIPT_COUNT) {
        m_State->scripts[index] = script;
    }
}

const ANM_SPRITE *Anm_GetSpriteEntry(const int32_t index)
{
    if (index < 0 || index >= ANM_SPRITE_COUNT) {
        return nullptr;
    }
    return &m_State->sprites[index];
}

void Anm_SetSpriteEntry(const int32_t index, const ANM_SPRITE *const sprite)
{
    if (index >= 0 && index < ANM_SPRITE_COUNT) {
        m_State->sprites[index] = *sprite;
    }
}

void Anm_Init(const ALLOCATOR *const allocator, RNG *const rng)
{
    m_Allocator = allocator;
    m_Rng = rng;
    m_State = Allocator_AllocZ(allocator, sizeof(M_STATE));
    for (int32_t i = 0; i < ANM_SPRITE_COUNT; i++) {
        m_State->sprites[i].slot = -1;
    }
}

void Anm_Shutdown(void)
{
    for (int32_t i = 0; i < ANM_SLOT_COUNT; i++) {
        Anm_Release(i);
    }
    for (int32_t i = 0; i < ANM_TEXTURE_COUNT; i++) {
        Anm_ReleaseTexture(i);
    }
    for (int32_t i = 0; i < ANM_SURFACE_COUNT; i++) {
        Anm_ReleaseSurface(i);
    }
    Allocator_Free(m_Allocator, m_State, sizeof(M_STATE));
    m_State = nullptr;
}
