// SPDX-License-Identifier: GPL-3.0-only

#include "th06/anm.h"
#include "th06/anm/private.h"

#include "th06/angle.h"
#include "th06/files.h"
#include "th06/timer.h"

#include "core/base/assert.h"
#include "core/base/memory.h"
#include "core/io/reader.h"
#include "core/math/x87.h"

#include <string.h>

#define M_HEADER_SIZE 0x40
#define M_MAX_ARGS_SIZE 16

typedef enum {
    M_OP_END = 0, // hides the sprite and ends the script
    M_OP_SPRITE = 1,
    M_OP_SCALE = 2,
    M_OP_ALPHA = 3,
    M_OP_COLOR = 4,
    M_OP_JUMP = 5,
    M_OP_FLIP_X = 7,
    M_OP_FLIP_Y = 8,
    M_OP_ROTATION = 9,
    M_OP_ROTATION_SPEED = 10,
    M_OP_SCALE_SPEED = 11,
    M_OP_FADE = 12,
    M_OP_BLEND_ADD = 13,
    M_OP_BLEND_ALPHA = 14,
    M_OP_KEEP = 15, // ends the script and keeps the sprite shown
    M_OP_RANDOM_SPRITE = 16,
    M_OP_POS = 17,
    M_OP_MOVE = 18,
    M_OP_MOVE_OUT = 19,
    M_OP_MOVE_OUT4 = 20,
    M_OP_STOP = 21,
    M_OP_LABEL = 22,
    M_OP_ANCHOR_TOP_LEFT = 23,
    M_OP_STOP_HIDDEN = 24,
    M_OP_USE_POS_OFFSET = 25,
    M_OP_AUTO_ROTATE = 26,
    M_OP_SCROLL_U = 27,
    M_OP_SCROLL_V = 28,
    M_OP_SHOWN = 29,
    M_OP_SCALE_TO = 30,
    M_OP_Z_WRITE_OFF = 31,
} M_OP;

// One loaded ANM file. It owns its scripts, so a VM keeps running one
// after the script table forgets it, and it records the table entries it
// set, for Anm_Release. Entries a failed load did not reach are -1, or a
// script without instructions.
typedef struct {
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

static float M_F32(const uint32_t bits)
{
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static bool M_IsEnd(const uint8_t opcode)
{
    return opcode == M_OP_END || opcode == M_OP_KEEP;
}

// Reads the instruction at the reader's position into instr and returns
// its size in bytes, or 0 when it runs past the file or its arguments are
// not 0 to 16 whole words.
static size_t M_ReadInstr(READER *const reader, ANM_INSTR *const instr)
{
    *instr = (ANM_INSTR) {};
    Reader_Read(reader, &instr->time);
    Reader_Read(reader, &instr->opcode);
    const uint8_t args_size = Reader_ReadU8(reader);
    if (args_size > M_MAX_ARGS_SIZE || args_size % 4 != 0) {
        return 0;
    }
    for (int32_t i = 0; i < args_size / 4; i++) {
        Reader_Read(reader, &instr->args[i]);
    }
    return Reader_Failed(reader) ? 0 : 4 + (size_t)args_size;
}

// Parses the script at offset up to its end instruction, and turns each
// jump's byte offset into an instruction index.
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
    } while (!M_IsEnd(instr.opcode));

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
        if (instrs[i].opcode != M_OP_JUMP) {
            continue;
        }
        int32_t target = 0;
        while (target < count && offsets[target] != instrs[i].args[0]) {
            target++;
        }
        if (target == count) {
            result =
                FAIL("jump to %u is not an instruction", instrs[i].args[0]);
        }
        instrs[i].args[0] = (uint32_t)target;
    }
    Mem_Free(offsets);
    return result;
}

// Parses the file in reader into slot, filling the sprite and script
// tables as it goes. On failure, slot holds what was filled so far.
static RESULT M_Parse(
    READER *const reader, const int32_t slot, const int32_t first_sprite)
{
    M_SLOT *const loaded = &m_State->slots[slot];
    const int32_t sprite_count = Reader_ReadI32(reader);
    const int32_t script_count = Reader_ReadI32(reader);
    Reader_Skip(reader, 4);
    const float texture_width = (float)Reader_ReadI32(reader);
    const float texture_height = (float)Reader_ReadI32(reader);
    FAIL_IF(
        Reader_Failed(reader) || sprite_count < 0 || script_count < 0,
        "bad header");

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

    for (int32_t i = 0; i < sprite_count; i++) {
        Reader_Seek(reader, M_HEADER_SIZE + 4 * (size_t)i);
        Reader_Seek(reader, (size_t)Reader_ReadI32(reader));
        const int32_t number = Reader_ReadI32(reader) + first_sprite;
        const float x = Reader_ReadF32(reader);
        const float y = Reader_ReadF32(reader);
        const float width = Reader_ReadF32(reader);
        const float height = Reader_ReadF32(reader);
        FAIL_IF(
            Reader_Failed(reader) || number < 0 || number >= ANM_SPRITE_COUNT,
            "bad sprite %d", i);
        ANM_SPRITE *const sprite = &m_State->sprites[number];
        *sprite = (ANM_SPRITE) {
            .slot = slot,
            .x0 = x,
            .y0 = y,
            .x1 = x + width,
            .y1 = y + height,
            .texture_width = texture_width,
            .texture_height = texture_height,
            .id = m_State->next_sprite_id++,
        };
        sprite->u0 = sprite->x0 / sprite->texture_width;
        sprite->u1 = sprite->x1 / sprite->texture_width;
        sprite->v0 = sprite->y0 / sprite->texture_height;
        sprite->v1 = sprite->y1 / sprite->texture_height;
        sprite->width = sprite->x1 - sprite->x0;
        sprite->height = sprite->y1 - sprite->y0;
        loaded->sprites[i] = number;
    }

    for (int32_t i = 0; i < script_count; i++) {
        Reader_Seek(
            reader, M_HEADER_SIZE + 4 * (size_t)sprite_count + 8 * (size_t)i);
        const int32_t number = Reader_ReadI32(reader) + first_sprite;
        const size_t offset = (size_t)Reader_ReadI32(reader);
        FAIL_IF(
            Reader_Failed(reader) || number < 0 || number >= ANM_SCRIPT_COUNT,
            "bad script %d", i);
        ANM_SCRIPT *const script = &loaded->scripts[i];
        const RESULT result = M_ParseScript(reader, offset, script);
        script->sprite_offset = first_sprite;
        loaded->script_numbers[i] = number;
        m_State->scripts[number] = script;
        MUST(result, "script %d", i);
    }
    return OK;
}

RESULT Anm_Load(
    const int32_t slot, const char *const path, const int32_t first_sprite)
{
    ASSERT(slot >= 0 && slot < ANM_SLOT_COUNT);
    Anm_Release(slot);
    READER reader;
    MUST(Files_Load(path, &reader), "%s", path);
    RESULT result = M_Parse(&reader, slot, first_sprite);
    if (IS_OK(result)) {
        result = AnmTexture_LoadSlot(&reader, slot);
    }
    Reader_Close(&reader);
    if (!IS_OK(result)) {
        Anm_Release(slot);
    }
    return Result_Prefix(result, "%s", path);
}

void Anm_Release(const int32_t slot)
{
    ASSERT(slot >= 0 && slot < ANM_SLOT_COUNT);
    M_SLOT *const loaded = &m_State->slots[slot];
    Anm_ReleaseTexture(slot);
    for (int32_t i = 0; i < loaded->sprite_count; i++) {
        if (loaded->sprites[i] >= 0) {
            m_State->sprites[loaded->sprites[i]] = (ANM_SPRITE) { .slot = -1 };
        }
    }
    // Forgets the table entries even when another file has since loaded
    // over the same script numbers, as the original does.
    for (int32_t i = 0; i < loaded->script_count; i++) {
        if (loaded->script_numbers[i] >= 0) {
            m_State->scripts[loaded->script_numbers[i]] = nullptr;
        }
        Allocator_Free(
            m_Allocator, (void *)loaded->scripts[i].instrs,
            (size_t)loaded->scripts[i].count * sizeof(ANM_INSTR));
    }
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

void Anm_ClearMenuScripts(void)
{
    for (int32_t i = 0x100; i < 0x100 + 0x7a; i++) {
        m_State->scripts[i] = nullptr;
    }
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
    Timer_Init(&vm->script_time);
    Timer_Init(&vm->move_timer);
    Timer_Init(&vm->scale_timer);
    Timer_Init(&vm->fade_timer);
}

void Anm_InitVm(ANM_VM *const vm)
{
    Anm_RenewPresentation(vm);
    vm->rotation = (VEC3) {};
    vm->rotation_speed = (VEC3) {};
    vm->scale_speed_x = 0.0f;
    vm->scale_speed_y = 0.0f;
    vm->scale_target_x = 0.0f;
    vm->scale_target_y = 0.0f;
    vm->uv_scroll_x = 0.0f;
    vm->uv_scroll_y = 0.0f;
    vm->scale_x = 1.0f;
    vm->scale_y = 1.0f;
    vm->fade_time = 0;
    vm->scale_time = 0;
    vm->color = 0xffffffffu;
    vm->uv_scale_x = 1.0f;
    vm->uv_scale_y = 1.0f;
    vm->shown = true;
    vm->visible = true;
    vm->blend_add = false;
    vm->color_op = false;
    vm->use_pos_offset = false;
    vm->flip = 0;
    vm->anchor = 0;
    vm->move_mode = ANM_MOVE_LINEAR;
    vm->z_write_off = false;
    vm->stopped = false;
    vm->auto_rotate = 0;
    vm->pending_interrupt = 0;
    vm->move_time = 0;
    Timer_Init(&vm->script_time);
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
    vm->uv_scale_x = vm->sprite_data->width / vm->sprite_data->texture_width;
    vm->uv_scale_y = vm->sprite_data->height / vm->sprite_data->texture_height;
}

static void M_SetScript(ANM_VM *const vm, const ANM_SCRIPT *const script)
{
    Anm_InitVm(vm);
    vm->script = script;
    vm->pc = 0;
    Timer_Set(&vm->script_time, 0);
    vm->shown = false;
    if (script != nullptr) {
        Anm_Step(vm);
    }
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

// Clears the pending interrupt and the stop, and finds the label for the
// interrupt, or else the -1 label, before the script's end. Sets *pc past
// the label and returns true, or holds script_time and returns false
// when there is neither.
static bool M_HandleInterrupt(ANM_VM *const vm, int32_t *const pc)
{
    int32_t found = -1;
    int32_t fallback = -1;
    for (int32_t i = 0; i < vm->script->count; i++) {
        const ANM_INSTR *const instr = &vm->script->instrs[i];
        if (instr->opcode == M_OP_LABEL
            && (int32_t)instr->args[0] == vm->pending_interrupt) {
            found = i;
            break;
        }
        if (M_IsEnd(instr->opcode)) {
            break;
        }
        if (instr->opcode == M_OP_LABEL && (int32_t)instr->args[0] == -1) {
            fallback = i;
        }
    }
    vm->pending_interrupt = 0;
    vm->stopped = false;
    const int32_t label = found >= 0 ? found : fallback;
    if (label < 0) {
        vm->script_time.current -= 1;
        return false;
    }
    *pc = label + 1;
    Timer_Set(&vm->script_time, vm->script->instrs[*pc].time);
    vm->shown = true;
    return true;
}

bool Anm_Step(ANM_VM *const vm)
{
    if (vm->script == nullptr || vm->pc < 0) {
        return true;
    }
    int32_t pc = vm->pc;
    bool running = true;
    if (vm->pending_interrupt != 0) {
        running = M_HandleInterrupt(vm, &pc);
    }
    while (running && pc >= 0 && pc < vm->script->count
           && vm->script->instrs[pc].time <= vm->script_time.current) {
        const ANM_INSTR *const instr = &vm->script->instrs[pc];
        switch (instr->opcode) {
        case M_OP_END:
            vm->shown = false;
            [[fallthrough]];
        case M_OP_KEEP:
            vm->pc = -1;
            return true;
        case M_OP_SPRITE:
            vm->shown = true;
            Anm_SetSprite(
                vm, (int32_t)instr->args[0] + vm->script->sprite_offset);
            vm->sprite_time = vm->script_time.current;
            break;
        case M_OP_RANDOM_SPRITE:
            vm->shown = true;
            Anm_SetSprite(
                vm,
                (int32_t)instr->args[0]
                    + Rng_U16InRange(m_Rng, (uint16_t)instr->args[1])
                    + vm->script->sprite_offset);
            vm->sprite_time = vm->script_time.current;
            break;
        case M_OP_SCALE:
            vm->scale_x = M_F32(instr->args[0]);
            vm->scale_y = M_F32(instr->args[1]);
            break;
        case M_OP_ALPHA:
            vm->color =
                (vm->color & 0x00ffffffu) | ((instr->args[0] & 0xffu) << 24);
            break;
        case M_OP_COLOR:
            vm->color =
                (vm->color & 0xff000000u) | (instr->args[0] & 0xffffffu);
            break;
        case M_OP_JUMP:
            pc = (int32_t)instr->args[0];
            vm->script_time.current = vm->script->instrs[pc].time;
            continue;
        case M_OP_FLIP_X:
            vm->flip ^= 1;
            vm->scale_x *= -1.0f;
            break;
        case M_OP_USE_POS_OFFSET:
            vm->use_pos_offset = (instr->args[0] & 1) != 0;
            break;
        case M_OP_FLIP_Y:
            vm->flip ^= 2;
            vm->scale_y *= -1.0f;
            break;
        case M_OP_ROTATION:
            vm->rotation.x = M_F32(instr->args[0]);
            vm->rotation.y = M_F32(instr->args[1]);
            vm->rotation.z = M_F32(instr->args[2]);
            break;
        case M_OP_ROTATION_SPEED:
            vm->rotation_speed.x = M_F32(instr->args[0]);
            vm->rotation_speed.y = M_F32(instr->args[1]);
            vm->rotation_speed.z = M_F32(instr->args[2]);
            break;
        case M_OP_SCALE_SPEED:
            vm->scale_speed_x = M_F32(instr->args[0]);
            vm->scale_speed_y = M_F32(instr->args[1]);
            vm->scale_time = 0;
            break;
        case M_OP_SCALE_TO:
            vm->scale_target_x = M_F32(instr->args[0]);
            vm->scale_target_y = M_F32(instr->args[1]);
            vm->scale_time = (int16_t)instr->args[2];
            Timer_Set(&vm->scale_timer, 0);
            vm->scale_from_x = vm->scale_x;
            vm->scale_from_y = vm->scale_y;
            vm->scale_speed_x = 0.0f;
            vm->scale_speed_y = 0.0f;
            break;
        case M_OP_FADE:
            vm->fade_from = vm->color;
            vm->fade_to =
                (vm->color & 0xffffffu) | ((instr->args[0] & 0xffu) << 24);
            vm->fade_time = (int16_t)instr->args[1];
            Timer_Set(&vm->fade_timer, 0);
            break;
        case M_OP_BLEND_ADD:
            vm->blend_add = true;
            break;
        case M_OP_BLEND_ALPHA:
            vm->blend_add = false;
            break;
        case M_OP_POS:
            if (!vm->use_pos_offset) {
                vm->pos = (VEC3) {
                    M_F32(instr->args[0]),
                    M_F32(instr->args[1]),
                    M_F32(instr->args[2]),
                };
            } else {
                vm->pos_offset = (VEC3) {
                    M_F32(instr->args[0]),
                    M_F32(instr->args[1]),
                    M_F32(instr->args[2]),
                };
            }
            break;
        case M_OP_MOVE:
        case M_OP_MOVE_OUT:
        case M_OP_MOVE_OUT4:
            vm->move_mode = instr->opcode == M_OP_MOVE ? ANM_MOVE_LINEAR
                : instr->opcode == M_OP_MOVE_OUT       ? ANM_MOVE_OUT
                                                       : ANM_MOVE_OUT4;
            vm->move_from = vm->use_pos_offset ? vm->pos_offset : vm->pos;
            vm->move_to = (VEC3) {
                M_F32(instr->args[0]),
                M_F32(instr->args[1]),
                M_F32(instr->args[2]),
            };
            vm->move_time = (int16_t)instr->args[3];
            Timer_Set(&vm->move_timer, 0);
            break;
        case M_OP_STOP_HIDDEN:
            vm->shown = false;
            [[fallthrough]];
        case M_OP_STOP:
            if (vm->pending_interrupt == 0) {
                vm->stopped = true;
                vm->script_time.current -= 1;
                running = false;
            } else {
                running = M_HandleInterrupt(vm, &pc);
            }
            continue;
        case M_OP_SHOWN:
            vm->shown = (instr->args[0] & 1) != 0;
            break;
        case M_OP_ANCHOR_TOP_LEFT:
            vm->anchor = 3;
            break;
        case M_OP_AUTO_ROTATE:
            vm->auto_rotate = (int16_t)instr->args[0];
            break;
        case M_OP_SCROLL_U:
            vm->uv_scroll_x += M_F32(instr->args[0]);
            if (vm->uv_scroll_x >= 1.0f) {
                vm->uv_scroll_x -= 1.0f;
            } else if (vm->uv_scroll_x < 0.0f) {
                vm->uv_scroll_x += 1.0f;
            }
            break;
        case M_OP_SCROLL_V:
            vm->uv_scroll_y += M_F32(instr->args[0]);
            if (vm->uv_scroll_y >= 1.0f) {
                vm->uv_scroll_y -= 1.0f;
            } else if (vm->uv_scroll_y < 0.0f) {
                vm->uv_scroll_y += 1.0f;
            }
            break;
        case M_OP_Z_WRITE_OFF:
            vm->z_write_off = (instr->args[0] & 1) != 0;
            break;
        default:
            break;
        }
        pc++;
    }

    if (vm->rotation_speed.x != 0.0f) {
        vm->rotation.x =
            Angle_AddNormalize(vm->rotation.x, vm->rotation_speed.x);
    }
    if (vm->rotation_speed.y != 0.0f) {
        vm->rotation.y =
            Angle_AddNormalize(vm->rotation.y, vm->rotation_speed.y);
    }
    if (vm->rotation_speed.z != 0.0f) {
        vm->rotation.z =
            Angle_AddNormalize(vm->rotation.z, vm->rotation_speed.z);
    }

    if (vm->scale_time > 0) {
        Timer_Tick(&vm->scale_timer);
        if (vm->scale_timer.current >= vm->scale_time) {
            vm->scale_x = vm->scale_target_x;
            vm->scale_y = vm->scale_target_y;
            vm->scale_time = 0;
        } else {
            vm->scale_x = (vm->scale_target_x - vm->scale_from_x)
                    * (float)vm->scale_timer.current / (float)vm->scale_time
                + vm->scale_from_x;
            vm->scale_y = (vm->scale_target_y - vm->scale_from_y)
                    * (float)vm->scale_timer.current / (float)vm->scale_time
                + vm->scale_from_y;
        }
        if (vm->flip & 1) {
            vm->scale_x *= -1.0f;
        }
        if (vm->flip & 2) {
            vm->scale_y *= -1.0f;
        }
    } else {
        vm->scale_y += vm->scale_speed_y;
        vm->scale_x += vm->scale_speed_x;
    }

    if (vm->fade_time > 0) {
        Timer_Tick(&vm->fade_timer);
        float alpha = (float)vm->fade_timer.current / (float)vm->fade_time;
        if (alpha >= 1.0f) {
            alpha = 1.0f;
        }
        uint32_t col = 0;
        for (int32_t i = 0; i < 4; i++) {
            const float from = (float)((vm->fade_from >> (i * 8)) & 0xff);
            const float to = (float)((vm->fade_to >> (i * 8)) & 0xff);
            int32_t x = X87_Ftol((to - from) * alpha + from);
            if (x < 0) {
                x = 0;
            }
            const uint32_t byte = x >= 0x100 ? 0xffu : (uint32_t)x;
            col |= byte << (i * 8);
        }
        vm->color = col;
        if (vm->fade_timer.current >= vm->fade_time) {
            vm->fade_time = 0;
        }
    }

    if (vm->move_time != 0) {
        float t = (float)vm->move_timer.current / (float)vm->move_time;
        if (t >= 1.0f) {
            t = 1.0f;
        }
        switch (vm->move_mode) {
        case ANM_MOVE_OUT:
            t = 1.0f - t;
            t = t * t;
            t = 1.0f - t;
            break;
        case ANM_MOVE_OUT4:
            t = 1.0f - t;
            t = t * t * t * t;
            t = 1.0f - t;
            break;
        case ANM_MOVE_LINEAR:
            break;
        }
        VEC3 *const target = vm->use_pos_offset ? &vm->pos_offset : &vm->pos;
        target->x = t * vm->move_to.x + (1.0f - t) * vm->move_from.x;
        target->y = t * vm->move_to.y + (1.0f - t) * vm->move_from.y;
        target->z = t * vm->move_to.z + (1.0f - t) * vm->move_from.z;
        if (vm->move_timer.current >= vm->move_time) {
            vm->move_time = 0;
        }
        Timer_Tick(&vm->move_timer);
    }

    vm->pc = pc;
    Timer_Tick(&vm->script_time);
    return false;
}
