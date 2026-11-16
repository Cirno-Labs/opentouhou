#include "th07/anm.h"
#include "th07/anm/private.h"

#include "th07/game.h"

#include "shell/game.h"
#include "core/vfs.h"
#include "core/base/assert.h"
#include "core/base/color.h"
#include "core/base/memory.h"
#include "core/io/reader.h"
#include "core/math/common.h"
#include "core/math/x87.h"

#include "stg/angle.h"
#include "stg/note.h"
#include "stg/timer.h"

#include <string.h>

#define M_HEADER_SIZE 0x40
#define M_INSTR_HEADER_SIZE 8
#define M_VERSION 2

typedef enum {
    M_OP_END = -1,
    M_OP_DELETE = 1,
    M_OP_STATIC = 2,
    M_OP_SPRITE = 3,
    M_OP_JUMP = 4,
    M_OP_LOOP = 5,
    M_OP_POS = 6,
    M_OP_SCALE = 7,
    M_OP_ALPHA = 8,
    M_OP_COLOR = 9,
    M_OP_FLIP_X = 10,
    M_OP_FLIP_Y = 11,
    M_OP_ROTATION = 12,
    M_OP_ROTATION_SPEED = 13,
    M_OP_SCALE_SPEED = 14,
    M_OP_FADE = 15,
    M_OP_BLEND = 16,
    M_OP_MOVE = 17,
    M_OP_MOVE_OUT = 18,
    M_OP_MOVE_OUT4 = 19,
    M_OP_STOP = 20,
    M_OP_LABEL = 21,
    M_OP_ANCHOR_TOP_LEFT = 22,
    M_OP_STOP_HIDDEN = 23,
    M_OP_USE_POS_OFFSET = 24,
    M_OP_AUTO_ROTATE = 25,
    M_OP_SCROLL_U = 26,
    M_OP_SCROLL_V = 27,
    M_OP_SHOWN = 28,
    M_OP_SCALE_TO = 29,
    M_OP_Z_WRITE_OFF = 30,
    M_OP_STAGE_CAMERA = 31,
    M_OP_EASE_POS = 32,
    M_OP_EASE_COLOR = 33,
    M_OP_EASE_ALPHA = 34,
    M_OP_EASE_ROTATION = 35,
    M_OP_EASE_SCALE = 36,
    M_OP_SET_INT = 37,
    M_OP_SET_FLOAT = 38,
    M_OP_ADD_INT = 39,
    M_OP_ADD_FLOAT = 40,
    M_OP_SUB_INT = 41,
    M_OP_SUB_FLOAT = 42,
    M_OP_MUL_INT = 43,
    M_OP_MUL_FLOAT = 44,
    M_OP_DIV_INT = 45,
    M_OP_DIV_FLOAT = 46,
    M_OP_MOD_INT = 47,
    M_OP_MOD_FLOAT = 48,
    M_OP_ADD3_INT = 49,
    M_OP_ADD3_FLOAT = 50,
    M_OP_SUB3_INT = 51,
    M_OP_SUB3_FLOAT = 52,
    M_OP_MUL3_INT = 53,
    M_OP_MUL3_FLOAT = 54,
    M_OP_DIV3_INT = 55,
    M_OP_DIV3_FLOAT = 56,
    M_OP_MOD3_INT = 57,
    M_OP_MOD3_FLOAT = 58,
    M_OP_RAND_INT = 59,
    M_OP_RAND_FLOAT = 60,
    M_OP_SIN = 61,
    M_OP_COS = 62,
    M_OP_TAN = 63,
    M_OP_ACOS = 64,
    M_OP_ATAN = 65,
    M_OP_NORMALIZE_ANGLE = 66,
    M_OP_JUMP_IF_FIRST = 67,
    M_OP_JUMP_EQ_INT = 67,
    M_OP_JUMP_EQ_FLOAT = 68,
    M_OP_JUMP_NE_INT = 69,
    M_OP_JUMP_NE_FLOAT = 70,
    M_OP_JUMP_LT_INT = 71,
    M_OP_JUMP_LT_FLOAT = 72,
    M_OP_JUMP_LE_INT = 73,
    M_OP_JUMP_LE_FLOAT = 74,
    M_OP_JUMP_GT_INT = 75,
    M_OP_JUMP_GT_FLOAT = 76,
    M_OP_JUMP_GE_INT = 77,
    M_OP_JUMP_GE_FLOAT = 78,
    M_OP_JUMP_IF_LAST = 78,
    M_OP_WAIT = 79,
    M_OP_SCROLL_SPEED_U = 80,
    M_OP_SCROLL_SPEED_V = 81,
} M_OP;

#define M_VAR_FIRST 10000

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
static uint32_t m_PresentationGeneration;

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
    Note_Remove();
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

void Anm_RenewPresentation(ANM_VM *const vm)
{
    ASSERT(m_PresentationGeneration != UINT32_MAX);
    vm->presentation_generation = ++m_PresentationGeneration;
}

void Anm_CreateVm(ANM_VM *const vm)
{
    *vm = (ANM_VM) {};
    Anm_RenewPresentation(vm);
    vm->sprite = -1;
}

void Anm_InitVm(ANM_VM *const vm)
{
    Anm_RenewPresentation(vm);
    vm->rotation = (VEC3) {};
    vm->rotation_speed = (VEC3) {};
    vm->scale_x = 1.0f;
    vm->scale_y = 1.0f;
    vm->scale_speed_x = 0.0f;
    vm->scale_speed_y = 0.0f;
    vm->uv_scroll_x = 0.0f;
    vm->uv_scroll_y = 0.0f;
    vm->uv_scroll_speed_x = 0.0f;
    vm->uv_scroll_speed_y = 0.0f;
    Timer_Set(&vm->script_time, 0);
    vm->wait_timer = (TIMER) {};
    for (int32_t i = 0; i < ANM_INTERP_COUNT; i++) {
        vm->interp_time[i] = (TIMER) {};
        vm->interp_length[i] = 0;
        vm->interp_ease[i] = ANM_EASE_LINEAR;
    }
    for (int32_t i = 0; i < ANM_INT_VAR_COUNT; i++) {
        vm->int_vars[i] = 0;
    }
    for (int32_t i = 0; i < ANM_FLOAT_VAR_COUNT; i++) {
        vm->float_vars[i] = 0.0f;
    }
    vm->matrix = Mat4_Identity();
    vm->draw_matrix = (MAT4) {};
    vm->size_matrix = (MAT4) {};
    vm->color = COLOR_WHITE;
    vm->color2 = 0;
    vm->shown = true;
    vm->visible = true;
    vm->rotation_changed = true;
    vm->scale_changed = false;
    vm->blend_add = false;
    vm->use_pos_offset = false;
    vm->flip = 0;
    vm->anchor = 0;
    vm->z_write_off = false;
    vm->stopped = false;
    vm->stage_camera = false;
    vm->keep_draw_matrix = false;
    vm->use_color2 = false;
    vm->auto_rotate = 0;
    vm->pending_interrupt = 0;
}

void Anm_SetSprite(ANM_VM *const vm, const int32_t sprite)
{
    if (sprite < 0 || sprite >= ANM_SPRITE_COUNT
        || m_State->sprites[sprite].slot < 0) {
        return;
    }
    if (vm->sprite != sprite || vm->sprite_data != &m_State->sprites[sprite]) {
        Anm_RenewPresentation(vm);
    }
    vm->sprite = (int16_t)sprite;
    vm->sprite_data = &m_State->sprites[sprite];
    const ANM_SPRITE *const data = vm->sprite_data;
    vm->matrix = Mat4_Identity();
    vm->size_matrix = Mat4_Identity();
    vm->matrix.m[0][0] = data->width / 256.0f;
    vm->matrix.m[1][1] = data->height / 256.0f;
    vm->size_matrix.m[0][0] = data->width / data->texture_width * data->scale_x;
    vm->size_matrix.m[1][1] =
        data->height / data->texture_height * data->scale_y;
    vm->draw_matrix = vm->matrix;
}

static void M_SetScript(ANM_VM *const vm, const ANM_SCRIPT *const script)
{
    // Retail clears the whole VM, pos and script number included.
    if (script == nullptr) {
        *vm = (ANM_VM) {};
        Anm_RenewPresentation(vm);
        return;
    }
    Anm_InitVm(vm);
    vm->script = script;
    vm->pc = 0;
    Timer_Set(&vm->script_time, 0);
    vm->shown = false;
    Anm_Step(vm);
}

void Anm_StartScript(ANM_VM *const vm, const int32_t script)
{
    vm->script_index = (int16_t)script;
    M_SetScript(vm, Anm_GetScript(script));
}

void Anm_StartScriptAtOrigin(ANM_VM *const vm, const int32_t script)
{
    vm->pos = (VEC3) {};
    vm->pos_offset = (VEC3) {};
    vm->font_width = 15;
    vm->font_height = 15;
    Anm_StartScript(vm, script);
}

static int32_t M_IntValue(const ANM_VM *const vm, const int32_t value)
{
    const int32_t n = value - M_VAR_FIRST;
    if ((n >= 0 && n < 4) || n == 8 || n == 9) {
        return vm->int_vars[n < 4 ? n : n - 4];
    }
    if (n >= 4 && n < 8) {
        return X87_Ftol(vm->float_vars[n - 4]);
    }
    return value;
}

static float M_FloatValue(const ANM_VM *const vm, const float value)
{
    const int32_t n = X87_Ftol(value) - M_VAR_FIRST;
    if ((n >= 0 && n < 4) || n == 8 || n == 9) {
        return (float)vm->int_vars[n < 4 ? n : n - 4];
    }
    if (n >= 4 && n < 8) {
        return vm->float_vars[n - 4];
    }
    return value;
}

static bool M_IsVar(const ANM_INSTR *const instr, const int32_t n)
{
    return (instr->param_mask & (1u << n)) != 0;
}

static int32_t M_Int(
    const ANM_VM *const vm, const ANM_INSTR *const instr, const int32_t n)
{
    return M_IsVar(instr, n) ? M_IntValue(vm, instr->args[n].i)
                             : instr->args[n].i;
}

static float M_Float(
    const ANM_VM *const vm, const ANM_INSTR *const instr, const int32_t n)
{
    return M_IsVar(instr, n) ? M_FloatValue(vm, instr->args[n].f)
                             : instr->args[n].f;
}

// Without a variable, writes go to the argument in the script, which every
// VM running the script shares.
static int32_t *M_IntVar(
    ANM_VM *const vm, ANM_INSTR *const instr, const int32_t n)
{
    int32_t *const value = &instr->args[n].i;
    const int32_t var = *value - M_VAR_FIRST;
    if (M_IsVar(instr, n) && ((var >= 0 && var < 4) || var == 8 || var == 9)) {
        return &vm->int_vars[var < 4 ? var : var - 4];
    }
    return value;
}

static float *M_FloatVar(
    ANM_VM *const vm, ANM_INSTR *const instr, const int32_t n)
{
    float *const value = &instr->args[n].f;
    const int32_t var = X87_Ftol(*value) - M_VAR_FIRST;
    if (M_IsVar(instr, n) && var >= 4 && var < 8) {
        return &vm->float_vars[var - 4];
    }
    return value;
}

// Wrap as the original's 32-bit arithmetic does. Division by 0 and
// INT32_MIN / -1 fault in the original; they give 0 and INT32_MIN.
static int32_t M_Add(const int32_t a, const int32_t b)
{
    return (int32_t)((uint32_t)a + (uint32_t)b);
}

static int32_t M_Sub(const int32_t a, const int32_t b)
{
    return (int32_t)((uint32_t)a - (uint32_t)b);
}

static int32_t M_Mul(const int32_t a, const int32_t b)
{
    return (int32_t)((uint32_t)a * (uint32_t)b);
}

static int32_t M_Div(const int32_t a, const int32_t b)
{
    if (b == 0) {
        return 0;
    }
    return b == -1 ? M_Sub(0, a) : a / b;
}

static int32_t M_Mod(const int32_t a, const int32_t b)
{
    return b == 0 || b == -1 ? 0 : a % b;
}

// Reproduce the CRT's acos and atan on the x87.
static float M_Acos(const float x)
{
    return X87_Atan2(X87_Sqrt(1.0f - x * x), x);
}

static float M_Atan(const float x)
{
    return X87_Atan2(x, 1.0f);
}

static bool M_Compare(
    const ANM_VM *const vm, const ANM_INSTR *const instr, const int16_t opcode)
{
    if ((opcode - M_OP_JUMP_IF_FIRST) % 2 == 0) {
        const int32_t a = M_Int(vm, instr, 0);
        const int32_t b = M_Int(vm, instr, 1);
        switch (opcode) {
        case M_OP_JUMP_EQ_INT:
            return a == b;
        case M_OP_JUMP_NE_INT:
            return a != b;
        case M_OP_JUMP_LT_INT:
            return a < b;
        case M_OP_JUMP_LE_INT:
            return a <= b;
        case M_OP_JUMP_GT_INT:
            return a > b;
        default:
            return a >= b;
        }
    }
    const float a = M_Float(vm, instr, 0);
    const float b = M_Float(vm, instr, 1);
    switch (opcode) {
    case M_OP_JUMP_EQ_FLOAT:
        return a == b;
    case M_OP_JUMP_NE_FLOAT:
        return a != b;
    case M_OP_JUMP_LT_FLOAT:
        return a < b;
    case M_OP_JUMP_LE_FLOAT:
        return a <= b;
    case M_OP_JUMP_GT_FLOAT:
        return a > b;
    default:
        return a >= b;
    }
}

// Jumps past the interrupt's label, or else the last -1 label. Returns
// false, holding script_time, when there is neither.
static bool M_Interrupt(ANM_VM *const vm)
{
    const ANM_INSTR *const instrs = vm->script->instrs;
    int32_t i = 0;
    int32_t fallback = -1;
    while ((instrs[i].opcode != M_OP_LABEL
            || vm->pending_interrupt != instrs[i].args[0].i)
           && instrs[i].opcode != M_OP_END) {
        if (instrs[i].opcode == M_OP_LABEL && instrs[i].args[0].i == -1) {
            fallback = i;
        }
        i++;
    }
    vm->pending_interrupt = 0;
    vm->stopped = false;
    if (instrs[i].opcode != M_OP_LABEL) {
        if (fallback < 0) {
            Timer_SubAt(&vm->script_time, 1, g_GameSpeed);
            return false;
        }
        i = fallback;
    }
    vm->pc = i + 1;
    Timer_Set(&vm->script_time, instrs[vm->pc].time);
    vm->shown = true;
    return true;
}

static void M_StartInterp(
    ANM_VM *const vm, const ANM_INTERP interp, const int32_t length,
    const int32_t ease)
{
    Timer_Set(&vm->interp_time[interp], 0);
    vm->interp_length[interp] = length;
    vm->interp_ease[interp] = (uint8_t)ease;
}

static void M_StartMove(
    ANM_VM *const vm, const ANM_INSTR *const instr, const int32_t first)
{
    vm->move_from = vm->use_pos_offset ? vm->pos_offset : vm->pos;
    vm->move_to = (VEC3) {
        M_Float(vm, instr, first),
        M_Float(vm, instr, first + 1),
        M_Float(vm, instr, first + 2),
    };
}

static void M_StartScale(
    ANM_VM *const vm, const ANM_INSTR *const instr, const int32_t first)
{
    vm->scale_from = (VEC2) { vm->scale_x, vm->scale_y };
    vm->scale_to = (VEC2) {
        M_Float(vm, instr, first),
        M_Float(vm, instr, first + 1),
    };
}

static void M_StartAlpha(ANM_VM *const vm, const uint32_t alpha)
{
    vm->color_from = (vm->color_from & 0xffffffu) | (vm->color & 0xff000000u);
    vm->color_to = (vm->color_to & 0xffffffu) | ((alpha & 0xffu) << 24);
}

static float M_Scroll(float scroll, const float add)
{
    scroll += add;
    if (scroll >= 1.0f) {
        scroll -= 1.0f;
    } else if (scroll < 0.0f) {
        scroll += 1.0f;
    }
    return scroll;
}

// Returns false when the script waits for the next frame. Each instruction
// moves pc itself.
static bool M_Run(ANM_VM *const vm, ANM_INSTR *const instr)
{
    switch (instr->opcode) {
    case M_OP_SPRITE:
        vm->shown = true;
        Anm_SetSprite(vm, M_Int(vm, instr, 0) + vm->script->sprite_offset);
        vm->sprite_time = vm->script_time.current;
        break;
    case M_OP_SCALE:
        vm->scale_x = M_Float(vm, instr, 0);
        vm->scale_y = M_Float(vm, instr, 1);
        vm->scale_changed = true;
        break;
    case M_OP_ALPHA:
        vm->color =
            (vm->color & 0xffffffu) | ((uint32_t)instr->args[0].i << 24);
        break;
    case M_OP_COLOR:
        vm->color = (vm->color & 0xff000000u)
            | ((uint32_t)instr->args[0].i & 0xffffffu);
        break;
    case M_OP_JUMP:
        Timer_Set(&vm->script_time, instr->args[1].i);
        vm->pc = instr->args[0].i;
        return true;
    case M_OP_LOOP: {
        int32_t *const counter = M_IntVar(vm, instr, 0);
        *counter = M_Sub(*counter, 1);
        if (M_Int(vm, instr, 0) > 0) {
            Timer_Set(&vm->script_time, instr->args[2].i);
            vm->pc = instr->args[1].i;
            return true;
        }
        break;
    }
    case M_OP_FLIP_X:
        vm->flip ^= 1;
        vm->scale_x *= -1.0f;
        vm->scale_changed = true;
        break;
    case M_OP_USE_POS_OFFSET:
        vm->use_pos_offset = (instr->args[0].i & 1) != 0;
        break;
    case M_OP_FLIP_Y:
        vm->flip ^= 2;
        vm->scale_y *= -1.0f;
        vm->scale_changed = true;
        break;
    case M_OP_ROTATION:
        vm->rotation = (VEC3) {
            M_Float(vm, instr, 0),
            M_Float(vm, instr, 1),
            M_Float(vm, instr, 2),
        };
        vm->rotation_changed = true;
        break;
    case M_OP_ROTATION_SPEED:
        vm->rotation_speed = (VEC3) {
            M_Float(vm, instr, 0),
            M_Float(vm, instr, 1),
            M_Float(vm, instr, 2),
        };
        vm->rotation_changed = true;
        break;
    case M_OP_SCALE_SPEED:
        vm->scale_speed_x = M_Float(vm, instr, 0);
        vm->scale_speed_y = M_Float(vm, instr, 1);
        break;
    case M_OP_SCALE_TO:
        M_StartInterp(
            vm, ANM_INTERP_SCALE, M_Int(vm, instr, 2), ANM_EASE_LINEAR);
        M_StartScale(vm, instr, 0);
        break;
    case M_OP_FADE:
        M_StartAlpha(vm, (uint32_t)instr->args[0].i);
        M_StartInterp(
            vm, ANM_INTERP_ALPHA, M_Int(vm, instr, 1), ANM_EASE_LINEAR);
        break;
    case M_OP_BLEND:
        vm->blend_add = (instr->args[0].i & 1) != 0;
        break;
    case M_OP_POS: {
        const VEC3 pos = {
            M_Float(vm, instr, 0),
            M_Float(vm, instr, 1),
            M_Float(vm, instr, 2),
        };
        *(vm->use_pos_offset ? &vm->pos_offset : &vm->pos) = pos;
        break;
    }
    case M_OP_MOVE:
    case M_OP_MOVE_OUT:
    case M_OP_MOVE_OUT4: {
        const ANM_EASE ease = instr->opcode == M_OP_MOVE ? ANM_EASE_LINEAR
            : instr->opcode == M_OP_MOVE_OUT             ? ANM_EASE_OUT2
                                                         : ANM_EASE_OUT4;
        M_StartMove(vm, instr, 0);
        M_StartInterp(vm, ANM_INTERP_POS, M_Int(vm, instr, 3), ease);
        break;
    }
    case M_OP_WAIT:
        if (vm->wait_timer.current == 0) {
            Timer_Set(&vm->wait_timer, M_Int(vm, instr, 0));
        } else {
            Timer_SubAt(&vm->wait_timer, 1, g_GameSpeed);
        }
        if (vm->wait_timer.current <= 0) {
            Timer_Set(&vm->wait_timer, 0);
            break;
        }
        Timer_SubAt(&vm->script_time, 1, g_GameSpeed);
        return false;
    case M_OP_STOP_HIDDEN:
        vm->shown = false;
        [[fallthrough]];
    case M_OP_STOP:
        if (vm->pending_interrupt == 0) {
            vm->stopped = true;
            Timer_SubAt(&vm->script_time, 1, g_GameSpeed);
            return false;
        }
        return M_Interrupt(vm);
    case M_OP_SHOWN:
        vm->shown = (instr->args[0].i & 1) != 0;
        break;
    case M_OP_ANCHOR_TOP_LEFT:
        vm->anchor = 3;
        break;
    case M_OP_AUTO_ROTATE:
        vm->auto_rotate = (int16_t)instr->args[0].i;
        break;
    case M_OP_SCROLL_U:
        vm->uv_scroll_x = M_Scroll(vm->uv_scroll_x, M_Float(vm, instr, 0));
        break;
    case M_OP_SCROLL_V:
        vm->uv_scroll_y = M_Scroll(vm->uv_scroll_y, M_Float(vm, instr, 0));
        break;
    case M_OP_SCROLL_SPEED_U:
        vm->uv_scroll_speed_x = M_Float(vm, instr, 0);
        break;
    case M_OP_SCROLL_SPEED_V:
        vm->uv_scroll_speed_y = M_Float(vm, instr, 0);
        break;
    case M_OP_Z_WRITE_OFF:
        vm->z_write_off = (instr->args[0].i & 1) != 0;
        break;
    case M_OP_STAGE_CAMERA:
        vm->stage_camera = (instr->args[0].i & 1) != 0;
        break;
    case M_OP_EASE_POS:
        M_StartInterp(
            vm, ANM_INTERP_POS, M_Int(vm, instr, 0), instr->args[1].i);
        M_StartMove(vm, instr, 2);
        break;
    case M_OP_EASE_COLOR: {
        M_StartInterp(
            vm, ANM_INTERP_COLOR, M_Int(vm, instr, 0), instr->args[1].i);
        // Red is the low byte, the other way from opcode 9.
        const uint32_t rgb = (uint32_t)instr->args[2].i;
        vm->color_from =
            (vm->color_from & 0xff000000u) | (vm->color & 0xffffffu);
        vm->color_to = (vm->color_to & 0xff000000u) | ((rgb & 0xffu) << 16)
            | (rgb & 0xff00u) | ((rgb >> 16) & 0xffu);
        break;
    }
    case M_OP_EASE_ALPHA:
        M_StartInterp(
            vm, ANM_INTERP_ALPHA, M_Int(vm, instr, 0), instr->args[1].i);
        M_StartAlpha(vm, (uint32_t)instr->args[2].i);
        break;
    case M_OP_EASE_ROTATION:
        M_StartInterp(
            vm, ANM_INTERP_ROTATION, M_Int(vm, instr, 0), instr->args[1].i);
        vm->rotate_from = vm->rotation;
        vm->rotate_to = (VEC3) {
            M_Float(vm, instr, 2),
            M_Float(vm, instr, 3),
            M_Float(vm, instr, 4),
        };
        vm->rotation_changed = true;
        break;
    case M_OP_EASE_SCALE:
        M_StartInterp(
            vm, ANM_INTERP_SCALE, M_Int(vm, instr, 0), instr->args[1].i);
        M_StartScale(vm, instr, 2);
        vm->scale_changed = true;
        break;
    case M_OP_SET_INT:
        *M_IntVar(vm, instr, 0) = M_Int(vm, instr, 1);
        break;
    case M_OP_SET_FLOAT:
        *M_FloatVar(vm, instr, 0) = M_Float(vm, instr, 1);
        break;
    case M_OP_ADD3_INT:
        *M_IntVar(vm, instr, 0) =
            M_Add(M_Int(vm, instr, 1), M_Int(vm, instr, 2));
        break;
    case M_OP_ADD3_FLOAT:
        *M_FloatVar(vm, instr, 0) =
            M_Float(vm, instr, 1) + M_Float(vm, instr, 2);
        break;
    case M_OP_SUB3_INT:
        *M_IntVar(vm, instr, 0) =
            M_Sub(M_Int(vm, instr, 1), M_Int(vm, instr, 2));
        break;
    case M_OP_SUB3_FLOAT:
        *M_FloatVar(vm, instr, 0) =
            M_Float(vm, instr, 1) - M_Float(vm, instr, 2);
        break;
    case M_OP_MUL3_INT:
        *M_IntVar(vm, instr, 0) =
            M_Mul(M_Int(vm, instr, 1), M_Int(vm, instr, 2));
        break;
    case M_OP_MUL3_FLOAT:
        *M_FloatVar(vm, instr, 0) =
            M_Float(vm, instr, 1) * M_Float(vm, instr, 2);
        break;
    case M_OP_DIV3_INT:
        *M_IntVar(vm, instr, 0) =
            M_Div(M_Int(vm, instr, 1), M_Int(vm, instr, 2));
        break;
    case M_OP_DIV3_FLOAT:
        *M_FloatVar(vm, instr, 0) =
            M_Float(vm, instr, 1) / M_Float(vm, instr, 2);
        break;
    case M_OP_MOD3_INT:
        *M_IntVar(vm, instr, 0) =
            M_Mod(M_Int(vm, instr, 1), M_Int(vm, instr, 2));
        break;
    case M_OP_MOD3_FLOAT:
        *M_FloatVar(vm, instr, 0) =
            Math_Fmod(M_Float(vm, instr, 1), M_Float(vm, instr, 2));
        break;
    case M_OP_ADD_INT: {
        int32_t *const var = M_IntVar(vm, instr, 0);
        *var = M_Add(*var, M_Int(vm, instr, 1));
        break;
    }
    case M_OP_ADD_FLOAT:
        *M_FloatVar(vm, instr, 0) += M_Float(vm, instr, 1);
        break;
    case M_OP_SUB_INT: {
        int32_t *const var = M_IntVar(vm, instr, 0);
        *var = M_Sub(*var, M_Int(vm, instr, 1));
        break;
    }
    case M_OP_SUB_FLOAT:
        *M_FloatVar(vm, instr, 0) -= M_Float(vm, instr, 1);
        break;
    case M_OP_MUL_INT: {
        int32_t *const var = M_IntVar(vm, instr, 0);
        *var = M_Mul(*var, M_Int(vm, instr, 1));
        break;
    }
    case M_OP_MUL_FLOAT:
        *M_FloatVar(vm, instr, 0) *= M_Float(vm, instr, 1);
        break;
    case M_OP_DIV_INT: {
        int32_t *const var = M_IntVar(vm, instr, 0);
        *var = M_Div(*var, M_Int(vm, instr, 1));
        break;
    }
    case M_OP_DIV_FLOAT:
        *M_FloatVar(vm, instr, 0) /= M_Float(vm, instr, 1);
        break;
    case M_OP_MOD_INT: {
        int32_t *const var = M_IntVar(vm, instr, 0);
        *var = M_Mod(*var, M_Int(vm, instr, 1));
        break;
    }
    case M_OP_MOD_FLOAT:
        *M_FloatVar(vm, instr, 0) =
            Math_Fmod(M_Float(vm, instr, 0), M_Float(vm, instr, 1));
        break;
    case M_OP_RAND_INT:
        *M_IntVar(vm, instr, 0) =
            (int32_t)Rng_U32InRange(m_Rng, (uint32_t)M_Int(vm, instr, 1));
        break;
    case M_OP_RAND_FLOAT:
        *M_FloatVar(vm, instr, 0) =
            Rng_F32InRange(m_Rng, M_Float(vm, instr, 1));
        break;
    case M_OP_SIN:
        *M_FloatVar(vm, instr, 0) = X87_Sin(M_Float(vm, instr, 1));
        break;
    case M_OP_COS:
        *M_FloatVar(vm, instr, 0) = X87_Cos(M_Float(vm, instr, 1));
        break;
    case M_OP_TAN:
        *M_FloatVar(vm, instr, 0) = X87_Tan(M_Float(vm, instr, 1));
        break;
    case M_OP_ACOS:
        *M_FloatVar(vm, instr, 0) = M_Acos(M_Float(vm, instr, 1));
        break;
    case M_OP_ATAN:
        *M_FloatVar(vm, instr, 0) = M_Atan(M_Float(vm, instr, 1));
        break;
    case M_OP_NORMALIZE_ANGLE:
        *M_FloatVar(vm, instr, 0) =
            Angle_AddNormalize(M_Float(vm, instr, 0), 0.0f);
        break;
    default:
        if (instr->opcode >= M_OP_JUMP_IF_FIRST
            && instr->opcode <= M_OP_JUMP_IF_LAST
            && M_Compare(vm, instr, instr->opcode)) {
            Timer_Set(&vm->script_time, instr->args[3].i);
            vm->pc = instr->args[2].i;
            return true;
        }
        break;
    }
    vm->pc++;
    return true;
}

static float M_Ease(const uint8_t ease, const float t)
{
    switch (ease) {
    case ANM_EASE_IN2:
        return t * t;
    case ANM_EASE_IN3:
        return t * t * t;
    case ANM_EASE_IN4: {
        const float t2 = t * t;
        return t2 * t2;
    }
    case ANM_EASE_OUT2: {
        const float s = 1.0f - t;
        return 1.0f - s * s;
    }
    case ANM_EASE_OUT3: {
        const float s = 1.0f - t;
        return 1.0f - s * s * s;
    }
    case ANM_EASE_OUT4: {
        const float s = 1.0f - t;
        const float s2 = s * s;
        return 1.0f - s2 * s2;
    }
    default:
        return t;
    }
}

static float M_Lerp(const float from, const float to, const float t)
{
    return (to - from) * t + from;
}

static uint32_t M_LerpByte(
    const uint32_t color, const uint32_t from, const uint32_t to,
    const int32_t shift, const float t)
{
    const int32_t a = (int32_t)((from >> shift) & 0xffu);
    const int32_t b = (int32_t)((to >> shift) & 0xffu);
    const uint32_t byte = (uint32_t)X87_Ftol((float)(b - a) * t + (float)a);
    return (color & ~(0xffu << shift)) | ((byte & 0xffu) << shift);
}

static void M_Interpolate(ANM_VM *const vm, const ANM_INTERP interp)
{
    TIMER *const time = &vm->interp_time[interp];
    Timer_TickAt(time, g_GameSpeed);
    float t;
    if (time->current >= vm->interp_length[interp]) {
        t = 1.0f;
        vm->interp_length[interp] = 0;
    } else {
        t = Timer_AsFloat(time) / (float)vm->interp_length[interp];
    }
    t = M_Ease(vm->interp_ease[interp], t);
    switch (interp) {
    case ANM_INTERP_POS: {
        VEC3 *const pos = vm->use_pos_offset ? &vm->pos_offset : &vm->pos;
        pos->x = M_Lerp(vm->move_from.x, vm->move_to.x, t);
        pos->y = M_Lerp(vm->move_from.y, vm->move_to.y, t);
        pos->z = M_Lerp(vm->move_from.z, vm->move_to.z, t);
        break;
    }
    case ANM_INTERP_COLOR:
        for (int32_t shift = 16; shift >= 0; shift -= 8) {
            vm->color =
                M_LerpByte(vm->color, vm->color_from, vm->color_to, shift, t);
        }
        break;
    case ANM_INTERP_ALPHA:
        vm->color = M_LerpByte(vm->color, vm->color_from, vm->color_to, 24, t);
        break;
    case ANM_INTERP_ROTATION:
        vm->rotation.x = Angle_AddNormalize(
            (vm->rotate_to.x - vm->rotate_from.x) * t, vm->rotate_from.x);
        vm->rotation.y = Angle_AddNormalize(
            (vm->rotate_to.y - vm->rotate_from.y) * t, vm->rotate_from.y);
        vm->rotation.z = Angle_AddNormalize(
            (vm->rotate_to.z - vm->rotate_from.z) * t, vm->rotate_from.z);
        vm->rotation_changed = true;
        break;
    case ANM_INTERP_SCALE:
        vm->scale_x = M_Lerp(vm->scale_from.x, vm->scale_to.x, t);
        vm->scale_y = M_Lerp(vm->scale_from.y, vm->scale_to.y, t);
        vm->scale_changed = true;
        break;
    default:
        break;
    }
}

bool Anm_Step(ANM_VM *const vm)
{
    if (vm->script == nullptr || vm->pc < 0) {
        return true;
    }
    bool running = vm->pending_interrupt == 0 || M_Interrupt(vm);
    while (running) {
        ANM_INSTR *const instr = &vm->script->instrs[vm->pc];
        if (instr->time > vm->script_time.current) {
            break;
        }
        if (instr->opcode == M_OP_END || instr->opcode == M_OP_DELETE) {
            vm->shown = false;
        }
        if (instr->opcode == M_OP_END || instr->opcode == M_OP_DELETE
            || instr->opcode == M_OP_STATIC) {
            vm->pc = -1;
            return true;
        }
        running = M_Run(vm, instr);
    }

    const float speed = g_GameSpeed;
    if (vm->rotation_speed.x != 0.0f) {
        vm->rotation.x =
            Angle_AddNormalize(vm->rotation.x, speed * vm->rotation_speed.x);
        vm->rotation_changed = true;
    }
    if (vm->rotation_speed.y != 0.0f) {
        vm->rotation.y =
            Angle_AddNormalize(vm->rotation.y, speed * vm->rotation_speed.y);
        vm->rotation_changed = true;
    }
    if (vm->rotation_speed.z != 0.0f) {
        vm->rotation.z =
            Angle_AddNormalize(vm->rotation.z, speed * vm->rotation_speed.z);
        vm->rotation_changed = true;
    }
    for (int32_t i = 0; i < ANM_INTERP_COUNT; i++) {
        if (vm->interp_length[i] > 0) {
            M_Interpolate(vm, (ANM_INTERP)i);
        }
    }
    if (vm->scale_speed_y != 0.0f) {
        vm->scale_y += speed * vm->scale_speed_y;
        vm->scale_changed = true;
    }
    if (vm->scale_speed_x != 0.0f) {
        vm->scale_x += speed * vm->scale_speed_x;
        vm->scale_changed = true;
        vm->rotation_changed = true;
    }
    // Scrolls by a whole step each tick, whatever the game speed.
    vm->uv_scroll_x = M_Scroll(vm->uv_scroll_x, vm->uv_scroll_speed_x);
    vm->uv_scroll_y = M_Scroll(vm->uv_scroll_y, vm->uv_scroll_speed_y);
    Timer_TickAt(&vm->script_time, speed);
    return false;
}

static bool M_HasTexture(const ANM_VM *const vm)
{
    return vm->sprite_data != nullptr && vm->sprite_data->slot >= 0
        && Anm_GetTexture(vm->sprite_data->slot) != nullptr;
}

void Anm_StepVms(ANM_VM *const vms, const int32_t count)
{
    for (int32_t i = 0; i < count; i++) {
        if (vms[i].script_index >= 0) {
            Anm_Step(&vms[i]);
        }
    }
}

void Anm_StartVmSeries(
    ANM_VM *const vms, const int32_t script, const int32_t count)
{
    for (int32_t i = 0; i < count; i++) {
        Anm_StartScriptAtOrigin(&vms[i], script + i);
        vms[i].base_sprite = vms[i].sprite;
    }
}

void Anm_SendInterrupt(
    ANM_VM *const vms, const int32_t count, const int16_t interrupt)
{
    for (int32_t i = 0; i < count; i++) {
        if (M_HasTexture(&vms[i])) {
            vms[i].pending_interrupt = interrupt;
        }
    }
}
