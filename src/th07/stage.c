#include "th07/stage.h"

#include "th07/effect.h"
#include "th07/game.h"
#include "th07/game_manager.h"
#include "th07/gui.h"

#include "shell/game.h"
#include "core/vfs.h"
#include "core/base/color.h"
#include "core/base/cp932.h"
#include "core/base/memory.h"
#include "core/io/reader.h"
#include "core/math/x87.h"

#include "stg/chain.h"

#include <stdio.h>
#include <string.h>

#define M_FIRST_ANM_SLOT 5
#define M_ANM_SLOT_COUNT 5
#define M_FIRST_SPRITE 0x300
#define M_CALC_PRIORITY 7
#define M_DRAW_PRIORITY 3
#define M_DRAW2_PRIORITY 4
#define M_NAME_SIZE 0x80
#define M_OBJECTS_OFFSET 0x490
#define M_QUAD_SIZE 28
#define M_INSTANCE_SIZE 16
#define M_INSTR_SIZE 20
#define M_FOV 0.5235988f
#define M_FIXED_FOV 0.31415927f

STAGE g_Stage;

static CHAIN_ELEM m_CalcElem;
static CHAIN_ELEM m_DrawElem;
static CHAIN_ELEM m_Draw2Elem;
// The fog that the first draw sets up for all four layers.
static GFX_FOG m_Fog;

static VEC3 M_ReadVec3(READER *const reader)
{
    const float x = Reader_ReadF32(reader);
    const float y = Reader_ReadF32(reader);
    const float z = Reader_ReadF32(reader);
    return (VEC3) { x, y, z };
}

static int32_t M_Bits(const float value)
{
    int32_t bits;
    memcpy(&bits, &value, 4);
    return bits;
}

static RESULT M_ReadName(READER *const reader, char **const out)
{
    char bytes[M_NAME_SIZE];
    Reader_ReadBytes(reader, bytes, sizeof(bytes));
    FAIL_IF(Reader_Failed(reader), "names run past the file");
    const char *const end = memchr(bytes, '\0', sizeof(bytes));
    const size_t length =
        end != nullptr ? (size_t)(end - bytes) : sizeof(bytes);
    return Cp932_DecodeText(bytes, length, out);
}

static RESULT M_ReadObject(
    READER *const reader, const size_t offset, STAGE_OBJECT *const object)
{
    Reader_Seek(reader, offset + 2);
    object->layer = Reader_ReadI8(reader);
    Reader_Skip(reader, 1);
    object->pos = M_ReadVec3(reader);
    object->size = M_ReadVec3(reader);
    const size_t quads = Reader_Tell(reader);
    for (;;) {
        const int16_t type = Reader_ReadI16(reader);
        const int16_t size = Reader_ReadI16(reader);
        FAIL_IF(Reader_Failed(reader), "a quad list runs past the file");
        if (type < 0) {
            break;
        }
        FAIL_IF(size < M_QUAD_SIZE, "a quad is %d bytes", size);
        Reader_Skip(reader, (size_t)size - 4);
        object->quad_count++;
    }
    object->quads =
        Mem_AllocArrZ((size_t)object->quad_count, sizeof(STAGE_QUAD));
    Reader_Seek(reader, quads);
    for (int32_t i = 0; i < object->quad_count; i++) {
        const size_t start = Reader_Tell(reader);
        STAGE_QUAD *const quad = &object->quads[i];
        quad->type = Reader_ReadI16(reader);
        const int16_t size = Reader_ReadI16(reader);
        quad->script = Reader_ReadI16(reader);
        Reader_Skip(reader, 2);
        quad->pos = M_ReadVec3(reader);
        quad->width = Reader_ReadF32(reader);
        quad->height = Reader_ReadF32(reader);
        Reader_Seek(reader, start + (size_t)size);
    }
    return OK;
}

// Checks that the script stops on its last instruction, that every jump
// lands in it and that every keyframe has a keyframe after it.
static RESULT M_CheckScript(void)
{
    const STAGE_INSTR *const script = g_Stage.data.script;
    const int32_t count = g_Stage.data.script_count;
    FAIL_IF(count == 0, "the script is empty");
    const STAGE_INSTR *const last = &script[count - 1];
    FAIL_IF(
        last->frame != -1 && last->opcode != STAGE_OP_JUMP,
        "the script runs past its end");
    bool keyframe_after = false;
    for (int32_t i = count - 1; i >= 0; i--) {
        const STAGE_INSTR *const instr = &script[i];
        if (instr->opcode == STAGE_OP_JUMP && instr->frame != -1) {
            const int32_t target = M_Bits(instr->args.x);
            FAIL_IF(
                target < 0 || target >= count, "a jump at %d goes to %d", i,
                target);
        }
        if (instr->opcode == STAGE_OP_KEYFRAME) {
            FAIL_IF(
                instr->frame != -1 && !keyframe_after,
                "the keyframe at %d has none after it", instr->frame);
            keyframe_after = true;
        }
    }
    return OK;
}

static RESULT M_Parse(READER *const reader)
{
    const int32_t object_count = Reader_ReadI16(reader);
    Reader_Skip(reader, 2);
    const size_t instances = (size_t)Reader_ReadI32(reader);
    const size_t script = (size_t)Reader_ReadI32(reader);
    Reader_Skip(reader, 4);
    FAIL_IF(Reader_Failed(reader) || object_count < 0, "bad header");
    MUST(M_ReadName(reader, &g_Stage.data.name));
    for (int32_t i = 0; i < STAGE_SONG_COUNT; i++) {
        MUST(M_ReadName(reader, &g_Stage.data.song_names[i]));
    }
    for (int32_t i = 0; i < STAGE_SONG_COUNT; i++) {
        MUST(M_ReadName(reader, &g_Stage.data.song_paths[i]));
    }

    g_Stage.data.objects =
        Mem_AllocArrZ((size_t)object_count, sizeof(STAGE_OBJECT));
    g_Stage.data.object_count = object_count;
    for (int32_t i = 0; i < object_count; i++) {
        Reader_Seek(reader, M_OBJECTS_OFFSET + 4 * (size_t)i);
        const size_t offset = (size_t)Reader_ReadI32(reader);
        FAIL_IF(Reader_Failed(reader), "object %d runs past the file", i);
        MUST(
            M_ReadObject(reader, offset, &g_Stage.data.objects[i]), "object %d",
            i);
        g_Stage.data.sprite_count += g_Stage.data.objects[i].quad_count;
    }

    Reader_Seek(reader, instances);
    while (Reader_ReadI16(reader) >= 0) {
        Reader_Skip(reader, M_INSTANCE_SIZE - 2);
        g_Stage.data.instance_count++;
    }
    FAIL_IF(Reader_Failed(reader), "the instances run past the file");
    g_Stage.data.instances = Mem_AllocArrZ(
        (size_t)g_Stage.data.instance_count, sizeof(STAGE_INSTANCE));
    Reader_Seek(reader, instances);
    for (int32_t i = 0; i < g_Stage.data.instance_count; i++) {
        STAGE_INSTANCE *const instance = &g_Stage.data.instances[i];
        instance->object = Reader_ReadI16(reader);
        Reader_Skip(reader, 2);
        instance->pos = M_ReadVec3(reader);
        FAIL_IF(
            instance->object >= object_count, "instance %d places object %d", i,
            instance->object);
    }

    FAIL_IF(script > Reader_Size(reader), "the script starts past the file");
    g_Stage.data.script_count =
        (int32_t)((Reader_Size(reader) - script) / M_INSTR_SIZE);
    g_Stage.data.script =
        Mem_AllocArrZ((size_t)g_Stage.data.script_count, sizeof(STAGE_INSTR));
    Reader_Seek(reader, script);
    for (int32_t i = 0; i < g_Stage.data.script_count; i++) {
        STAGE_INSTR *const instr = &g_Stage.data.script[i];
        instr->frame = Reader_ReadI32(reader);
        instr->opcode = Reader_ReadI16(reader);
        Reader_Skip(reader, 2);
        instr->args = M_ReadVec3(reader);
    }
    return M_CheckScript();
}

// Starts a VM for each quad, at its script, and points the quad at it.
static void M_StartSprites(void)
{
    g_Stage.data.sprites =
        Mem_AllocArrZ((size_t)g_Stage.data.sprite_count, sizeof(ANM_VM));
    int16_t count = 0;
    for (int32_t i = 0; i < g_Stage.data.object_count; i++) {
        STAGE_OBJECT *const object = &g_Stage.data.objects[i];
        object->animating = true;
        for (int32_t j = 0; j < object->quad_count; j++) {
            STAGE_QUAD *const quad = &object->quads[j];
            Anm_StartScriptAtOrigin(
                &g_Stage.data.sprites[count], quad->script + M_FIRST_SPRITE);
            quad->sprite = count++;
        }
    }
}

static RESULT M_LoadAnms(const int32_t stage)
{
    static const char *const paths[] = {
        "data/stg4bg.anm",  "data/stg4bg2.anm", "data/stg4bg3.anm",
        "data/stg4bg4.anm", "data/stg4bg5.anm",
    };
    if (stage < 1) {
        return OK;
    }
    if (stage != 4) {
        char path[32];
        snprintf(path, sizeof(path), "data/stg%dbg.anm", stage);
        return Anm_Load(M_FIRST_ANM_SLOT, path, M_FIRST_SPRITE);
    }
    for (int32_t i = 0; i < M_ANM_SLOT_COUNT; i++) {
        MUST(Anm_Load(
            M_FIRST_ANM_SLOT + i, paths[i], M_FIRST_SPRITE + 0x10 * i));
    }
    return OK;
}

static RESULT M_Load(const int32_t stage)
{
    MUST(M_LoadAnms(stage));
    char path[32] = "dummy";
    if (stage >= 1) {
        snprintf(path, sizeof(path), "data/stage%d.std", stage);
    }
    READER reader;
    MUST(Vfs_Open(Shell_GetFiles(), path, VFS_IN_MEMORY, &reader), "%s", path);
    const RESULT result = M_Parse(&reader);
    Reader_Close(&reader);
    MUST(result, "%s", path);
    M_StartSprites();
    return OK;
}

static void M_Release(void)
{
    for (int32_t i = 0; i < M_ANM_SLOT_COUNT; i++) {
        Anm_Release(M_FIRST_ANM_SLOT + i);
    }
    Mem_Free(g_Stage.data.name);
    for (int32_t i = 0; i < STAGE_SONG_COUNT; i++) {
        Mem_Free(g_Stage.data.song_names[i]);
        Mem_Free(g_Stage.data.song_paths[i]);
    }
    for (int32_t i = 0; i < g_Stage.data.object_count; i++) {
        Mem_Free(g_Stage.data.objects[i].quads);
    }
    Mem_Free(g_Stage.data.objects);
    Mem_Free(g_Stage.data.instances);
    Mem_Free(g_Stage.data.script);
    Mem_Free(g_Stage.data.sprites);
    g_Stage.data = (typeof(g_Stage.data)) {};
}

static void M_StepObjects(void)
{
    for (int32_t i = 0; i < g_Stage.data.object_count; i++) {
        STAGE_OBJECT *const object = &g_Stage.data.objects[i];
        if (!object->animating) {
            continue;
        }
        int32_t busy = 0;
        for (int32_t j = 0; j < object->quad_count; j++) {
            const STAGE_QUAD *const quad = &object->quads[j];
            ANM_VM *const vm = &g_Stage.data.sprites[quad->sprite];
            if (quad->type == 0 || quad->type == 1) {
                Anm_Step(vm);
            }
            if (vm->script != nullptr && vm->pc >= 0) {
                busy++;
            }
        }
        if (busy == 0) {
            object->animating = false;
        }
    }
}

// Restarts the script after the label that script.label names. A label
// the script lacks stays set, so that the next wait passes.
static void M_GoToLabel(void)
{
    for (int32_t i = 0; i < g_Stage.data.script_count; i++) {
        const STAGE_INSTR *const instr = &g_Stage.data.script[i];
        if (instr->frame == -1) {
            return;
        }
        if (instr->opcode == STAGE_OP_LABEL
            && M_Bits(instr->args.x) == g_Stage.script.label) {
            g_Stage.script.index = i + 1;
            Timer_Set(&g_Stage.script.time, instr->frame);
            g_Stage.script.label = 0;
            return;
        }
    }
}

static void M_StartFade(
    const STAGE_FADE fade, const STAGE_INSTR *const instr,
    const STAGE_EASE ease)
{
    g_Stage.camera.frames[fade] = M_Bits(instr->args.x);
    Timer_Set(&g_Stage.camera.time[fade], 0);
    g_Stage.camera.ease[fade] = ease;
}

static void M_StartLayer(ANM_VM *const vm, const STAGE_INSTR *const instr)
{
    const int32_t script = M_Bits(instr->args.x);
    if (script >= 0) {
        Anm_StartScriptAtOrigin(vm, script + M_FIRST_SPRITE);
    } else {
        // Retail hides layer 0 for both layers' opcodes.
        g_Stage.layers[0].sprite = -1;
    }
}

// Runs the instruction at script.index, which is due. Returns false for a
// wait that holds.
static bool M_RunInstr(const STAGE_INSTR *const instr)
{
    STAGE_CAMERA *const current = &g_Stage.camera.current;
    STAGE_CAMERA *const end = &g_Stage.camera.end;
    STAGE_CAMERA *const start = &g_Stage.camera.start;
    const int32_t *const frames = g_Stage.camera.frames;
    switch (instr->opcode) {
    case STAGE_OP_KEYFRAME: {
        g_Stage.camera.pos = instr->args;
        g_Stage.camera.key_pos = instr->args;
        g_Stage.camera.key_frame = instr->frame;
        const STAGE_INSTR *next = instr + 1;
        while (next->opcode != STAGE_OP_KEYFRAME) {
            next++;
        }
        g_Stage.camera.next_key_frame = next->frame;
        g_Stage.camera.next_key_pos = next->args;
        break;
    }
    case STAGE_OP_FOG:
        memcpy(&g_Stage.fog.current.color, &instr->args.x, 4);
        g_Stage.fog.current.near = instr->args.y;
        g_Stage.fog.current.far = instr->args.z;
        g_Stage.fog.to = g_Stage.fog.current;
        break;
    case STAGE_OP_FOG_FADE:
        g_Stage.fog.from = g_Stage.fog.current;
        g_Stage.fog.frames = M_Bits(instr->args.x);
        Timer_Set(&g_Stage.fog.time, 0);
        break;
    case STAGE_OP_WAIT:
        if (g_Stage.script.label == 0) {
            return false;
        }
        g_Stage.script.label = 0;
        break;
    case STAGE_OP_EYE:
        if (g_Stage.camera.jumped) {
            Effect_OffsetPetals(Vec3_Sub(instr->args, end->eye));
            g_Stage.camera.jumped = false;
        }
        start->eye = end->eye;
        end->eye = instr->args;
        if (frames[STAGE_FADE_EYE] == 0) {
            current->eye = instr->args;
        }
        break;
    case STAGE_OP_EYE_FADE:
        M_StartFade(STAGE_FADE_EYE, instr, (STAGE_EASE)M_Bits(instr->args.y));
        break;
    case STAGE_OP_AT:
        start->at = end->at;
        end->at = instr->args;
        if (frames[STAGE_FADE_AT] == 0) {
            current->at = instr->args;
        }
        break;
    case STAGE_OP_AT_FADE:
        M_StartFade(STAGE_FADE_AT, instr, (STAGE_EASE)M_Bits(instr->args.y));
        break;
    case STAGE_OP_UP:
        start->up = end->up;
        end->up = instr->args;
        if (frames[STAGE_FADE_UP] == 0) {
            current->up = instr->args;
        }
        break;
    case STAGE_OP_UP_FADE:
        M_StartFade(STAGE_FADE_UP, instr, (STAGE_EASE)M_Bits(instr->args.y));
        break;
    case STAGE_OP_FOV:
        start->fov = end->fov;
        end->fov = instr->args.x;
        if (frames[STAGE_FADE_FOV] == 0) {
            current->fov = instr->args.x;
        }
        break;
    case STAGE_OP_FOV_FADE:
        M_StartFade(STAGE_FADE_FOV, instr, (STAGE_EASE)M_Bits(instr->args.y));
        break;
    case STAGE_OP_CLEAR_COLOR:
        memcpy(&g_Stage.clear_color, &instr->args.x, 4);
        break;
    case STAGE_OP_EYE_START:
        start->eye = instr->args;
        break;
    case STAGE_OP_EYE_END:
        end->eye = instr->args;
        break;
    case STAGE_OP_EYE_START_TANGENT:
        g_Stage.camera.start_tangent.eye = instr->args;
        break;
    case STAGE_OP_EYE_END_TANGENT:
        g_Stage.camera.end_tangent.eye = instr->args;
        break;
    case STAGE_OP_EYE_HERMITE:
        M_StartFade(STAGE_FADE_EYE, instr, STAGE_EASE_HERMITE);
        break;
    case STAGE_OP_AT_START:
        start->at = instr->args;
        break;
    case STAGE_OP_AT_END:
        end->at = instr->args;
        break;
    case STAGE_OP_AT_START_TANGENT:
        g_Stage.camera.start_tangent.at = instr->args;
        break;
    case STAGE_OP_AT_END_TANGENT:
        g_Stage.camera.end_tangent.at = instr->args;
        break;
    case STAGE_OP_AT_HERMITE:
        M_StartFade(STAGE_FADE_AT, instr, STAGE_EASE_HERMITE);
        break;
    case STAGE_OP_UP_START:
        start->up = instr->args;
        break;
    case STAGE_OP_UP_END:
        end->up = instr->args;
        break;
    case STAGE_OP_UP_START_TANGENT:
        g_Stage.camera.start_tangent.up = instr->args;
        break;
    case STAGE_OP_UP_END_TANGENT:
        g_Stage.camera.end_tangent.up = instr->args;
        break;
    case STAGE_OP_UP_HERMITE:
        M_StartFade(STAGE_FADE_UP, instr, STAGE_EASE_HERMITE);
        break;
    case STAGE_OP_LAYER0:
        M_StartLayer(&g_Stage.layers[0], instr);
        break;
    case STAGE_OP_LAYER1:
        M_StartLayer(&g_Stage.layers[1], instr);
        break;
    default:
        break;
    }
    return true;
}

// Runs the instructions due at the current frame. Returns the one it
// stopped at: the clock holds while that one is a wait, due or not.
static const STAGE_INSTR *M_RunScript(void)
{
    for (;;) {
        const STAGE_INSTR *const instr =
            &g_Stage.data.script[g_Stage.script.index];
        if (g_Stage.script.time.current < instr->frame || instr->frame == -1) {
            return instr;
        }
        if (instr->opcode == STAGE_OP_JUMP) {
            g_Stage.script.index = M_Bits(instr->args.x);
            Timer_Set(&g_Stage.script.time, M_Bits(instr->args.y));
            g_Stage.camera.frames[STAGE_FADE_EYE] = 0;
            g_Stage.camera.jumped = true;
            continue;
        }
        if (!M_RunInstr(instr)) {
            return instr;
        }
        g_Stage.script.index++;
    }
}

// Advances fade on its timer and returns its eased fraction. The fade
// ends on the update after its timer reaches its frames.
static float M_FadeFraction(const STAGE_FADE fade)
{
    TIMER *const time = &g_Stage.camera.time[fade];
    int32_t *const frames = &g_Stage.camera.frames[fade];
    float t;
    if (time->current < *frames) {
        Timer_TickAt(time, g_GameSpeed);
        t = Timer_AsFloat(time) / (float)*frames;
    } else {
        Timer_Set(time, *frames);
        t = 1.0f;
        *frames = 0;
    }
    float u;
    switch (g_Stage.camera.ease[fade]) {
    case STAGE_EASE_OUT2:
        u = 1.0f - t;
        t = 1.0f - u * u;
        break;
    case STAGE_EASE_OUT3:
        u = 1.0f - t;
        t = 1.0f - u * u * u;
        break;
    case STAGE_EASE_OUT4:
        u = 1.0f - t;
        t = 1.0f - u * u * u * u;
        break;
    case STAGE_EASE_IN2:
        t = t * t;
        break;
    case STAGE_EASE_IN3:
        t = t * t * t;
        break;
    case STAGE_EASE_IN4:
        t = t * t * t * t;
        break;
    default:
        break;
    }
    return t;
}

static float M_Hermite(
    const float p0, const float p1, const float m0, const float m1,
    const float t)
{
    const float f00 = (t - 1.0f) * (t - 1.0f) * (2.0f * t + 1.0f);
    const float f01 = t * t * (3.0f - 2.0f * t);
    const float f10 = (1.0f - t) * (1.0f - t) * t;
    const float f11 = (t - 1.0f) * t * t;
    return f00 * p0 + f01 * p1 + f10 * m0 + f11 * m1;
}

static VEC3 M_FadeVec3(
    const STAGE_FADE fade, const VEC3 start, const VEC3 end, const VEC3 m0,
    const VEC3 m1)
{
    const float t = M_FadeFraction(fade);
    if (g_Stage.camera.ease[fade] != STAGE_EASE_HERMITE) {
        return (VEC3) { t * (end.x - start.x) + start.x,
                        t * (end.y - start.y) + start.y,
                        t * (end.z - start.z) + start.z };
    }
    return (VEC3) { M_Hermite(start.x, end.x, m0.x, m1.x, t),
                    M_Hermite(start.y, end.y, m0.y, m1.y, t),
                    M_Hermite(start.z, end.z, m0.z, m1.z, t) };
}

static void M_FadeCamera(void)
{
    STAGE_CAMERA *const current = &g_Stage.camera.current;
    const STAGE_CAMERA *const start = &g_Stage.camera.start;
    const STAGE_CAMERA *const end = &g_Stage.camera.end;
    const STAGE_CAMERA *const m0 = &g_Stage.camera.start_tangent;
    const STAGE_CAMERA *const m1 = &g_Stage.camera.end_tangent;
    if (g_Stage.camera.frames[STAGE_FADE_EYE] != 0) {
        current->eye =
            M_FadeVec3(STAGE_FADE_EYE, start->eye, end->eye, m0->eye, m1->eye);
    }
    if (g_Stage.camera.frames[STAGE_FADE_AT] != 0) {
        current->at =
            M_FadeVec3(STAGE_FADE_AT, start->at, end->at, m0->at, m1->at);
    }
    if (g_Stage.camera.frames[STAGE_FADE_UP] != 0) {
        current->up =
            M_FadeVec3(STAGE_FADE_UP, start->up, end->up, m0->up, m1->up);
    }
    if (g_Stage.camera.frames[STAGE_FADE_FOV] != 0) {
        const float t = M_FadeFraction(STAGE_FADE_FOV);
        current->fov = (end->fov - start->fov) * t + start->fov;
    }
    current->dir = Vec3_Normalize(current->at);
}

static void M_FadeFog(void)
{
    Timer_TickAt(&g_Stage.fog.time, g_GameSpeed);
    float t = Timer_AsFloat(&g_Stage.fog.time) / (float)g_Stage.fog.frames;
    if (t >= 1.0f) {
        t = 1.0f;
    }
    const STAGE_FOG *const from = &g_Stage.fog.from;
    const STAGE_FOG *const to = &g_Stage.fog.to;
    uint32_t color = 0;
    for (int32_t i = 0; i < 4; i++) {
        const float a = (float)((from->color >> (i * 8)) & 0xff);
        const float b = (float)((to->color >> (i * 8)) & 0xff);
        color |= (uint32_t)(uint8_t)X87_Ftol((b - a) * t + a) << (i * 8);
    }
    g_Stage.fog.current.color = color;
    g_Stage.fog.current.near = (to->near - from->near) * t + from->near;
    g_Stage.fog.current.far = (to->far - from->far) * t + from->far;
    if (g_Stage.fog.time.current >= g_Stage.fog.frames) {
        g_Stage.fog.frames = 0;
    }
}

static CHAIN_STATUS M_Update(void *const arg)
{
    if (g_Stage.data.script == nullptr) {
        return CHAIN_CONTINUE;
    }
    if (g_Stage.script.label != 0) {
        M_GoToLabel();
    }
    const STAGE_INSTR *const instr = M_RunScript();
    M_FadeCamera();
    if (g_Stage.fog.frames != 0) {
        M_FadeFog();
    }
    if (instr->opcode != STAGE_OP_WAIT) {
        Timer_TickAt(&g_Stage.script.time, g_GameSpeed);
    }
    M_StepObjects();
    if (g_Stage.spell_card.state >= 1) {
        if (g_Stage.spell_card.frames == 60) {
            g_Stage.spell_card.state++;
        }
        g_Stage.spell_card.frames++;
        for (int32_t i = 0; i < g_Stage.spell_background.count; i++) {
            Anm_Step(&g_Stage.spell_background.sprites[i]);
        }
    }
    for (int32_t i = 0; i < 2; i++) {
        if (g_Stage.layers[i].sprite > 0) {
            Anm_Step(&g_Stage.layers[i]);
        }
    }
    return CHAIN_CONTINUE;
}

static GFX_VIEW M_FixedView(void)
{
    const VIEWPORT *const viewport = &g_GameManager.camera.viewport;
    const float mid_x = viewport->width / 2.0f;
    const float half_height = viewport->height / 2.0f;
    const float aspect = viewport->width / viewport->height;
    const float distance = half_height / X87_Tan(M_FIXED_FOV / 2.0f);
    const VEC3 up = { 0.0f, -1.0f, 0.0f };
    const VEC3 at = { mid_x, half_height, 0.0f };
    const VEC3 eye = { mid_x, half_height, distance };
    return (GFX_VIEW) {
        .view = Mat4_LookAtLH(eye, at, up),
        .projection =
            Mat4_PerspectiveFovLH(M_FIXED_FOV, aspect, 1.0f, 10000.0f),
        .viewport = *viewport,
        .id = ANM_VIEW_CAMERA,
    };
}

// Builds the stage camera's view and sets its right vector.
static GFX_VIEW M_StageView(void)
{
    STAGE_CAMERA *const camera = &g_Stage.camera.current;
    const VIEWPORT *const viewport = &g_GameManager.camera.viewport;
    const VEC3 at = Vec3_Add(camera->at, camera->eye);
    const GFX_VIEW view = {
        .view = Mat4_LookAtLH(camera->eye, at, camera->up),
        .projection = Mat4_PerspectiveFovLH(
            camera->fov, viewport->width / viewport->height, 30.0f, 1800.0f),
        .viewport = *viewport,
        .id = ANM_VIEW_STAGE,
    };
    camera->right = Vec3_Normalize(Vec3_Cross(camera->at, camera->up));
    return view;
}

static GFX_COLOR M_Rgba(const uint32_t color)
{
    return (GFX_COLOR) { (uint8_t)(color >> 16), (uint8_t)(color >> 8),
                         (uint8_t)color, (uint8_t)(color >> 24) };
}

// Returns whether the box of object at instance lies in front of the
// camera and near enough.
static bool M_ObjectVisible(
    const STAGE_OBJECT *const object, const STAGE_INSTANCE *const instance)
{
    const STAGE_CAMERA *const camera = &g_Stage.camera.current;
    const VEC3 *const pos = &g_Stage.camera.pos;
    VEC3 center = {
        object->pos.x + instance->pos.x - pos->x + object->size.x / 2.0f,
        object->pos.y + instance->pos.y - pos->y + object->size.y / 2.0f,
        object->pos.z + instance->pos.z - pos->z + object->size.z / 2.0f,
    };
    center = Vec3_Sub(center, camera->eye);
    if (Vec3_Dot(center, center) > 1690000.0f) {
        return false;
    }
    const float distance = Vec3_Dot(center, camera->dir);
    const float range = Vec3_Length(object->size) / 2.0f + 880.0f;
    return distance <= range && distance >= 60.0f;
}

// Draws vm at its position projected through the stage camera, sized
// along the view's x axis and faded by hand into the fog.
static void M_DrawProjected(
    ANM_VM *const vm, const STAGE_QUAD *const quad, const GFX_VIEW *const view,
    const uint32_t part)
{
    MAT4 matrix = Mat4_Identity();
    matrix.m[3][0] = vm->pos.x;
    matrix.m[3][1] = vm->pos.y;
    matrix.m[3][2] = vm->pos.z;
    const VEC3 center = Vec3_Project(
        (VEC3) {}, &view->viewport, &view->projection, &view->view, &matrix);
    const VEC3 right = Vec3_Normalize(
        (VEC3) { view->view.m[0][0], view->view.m[0][1], view->view.m[0][2] });
    const float width =
        quad->width != 0.0f ? quad->width : vm->sprite_data->width;
    matrix.m[3][0] += right.x * width * vm->scale_x;
    matrix.m[3][1] += right.y * width * vm->scale_x;
    matrix.m[3][2] += right.z * width * vm->scale_x;
    const VEC3 edge = Vec3_Project(
        (VEC3) {}, &view->viewport, &view->projection, &view->view, &matrix);
    vm->scale_x = Vec3_Length(Vec3_Sub(edge, center)) / width;
    vm->scale_y = vm->scale_x;
    float distance = Vec3_Length(Vec3_Sub(vm->pos, g_Stage.camera.current.eye));
    const uint32_t color = vm->color;
    const STAGE_FOG *const fog = &g_Stage.fog.current;
    if (fog->near < distance) {
        distance = (fog->near - distance) / (fog->near - fog->far);
        if (distance >= 1.0f) {
            return;
        }
        uint32_t faded = 0;
        for (int32_t i = 0; i < 3; i++) {
            const int32_t c = (int32_t)((color >> (i * 8)) & 0xff);
            const int32_t f = (int32_t)((fog->color >> (i * 8)) & 0xff);
            const uint8_t step = (uint8_t)X87_Ftol((float)(c - f) * distance);
            faded |= (uint32_t)(uint8_t)(c - step) << (i * 8);
        }
        const uint8_t alpha =
            (uint8_t)X87_Ftol((float)(color >> 24) * (1.0f - distance));
        vm->color = faded | (uint32_t)alpha << 24;
    }
    vm->pos = center;
    if (vm->pos.z >= 0.0f && vm->pos.z <= 1.0f) {
        ANM_DRAW draw = GameManager_PlayAreaDraw();
        draw.depth_test = GFX_DEPTH_LESS_EQUAL;
        draw.part = part;
        Anm_DrawNoRound(vm, &draw);
    }
    vm->color = color;
}

static void M_DrawLayer(const int32_t layer)
{
    GFX_VIEW fixed = M_FixedView();
    GFX_VIEW stage = M_StageView();
    fixed.fog = m_Fog;
    stage.fog = m_Fog;
    Anm_SetCameras(&fixed, &stage, g_Stage.camera.current.right);
    Anm_SetCamera(ANM_CAMERA_STAGE);
    Anm_SetLastCamera(ANM_CAMERA_STAGE);
    ANM_DRAW draw = Anm_ScreenDraw();
    draw.view = stage;
    draw.depth_test = GFX_DEPTH_LESS_EQUAL;
    draw.interpolate = true;
    const VEC3 camera = g_Stage.camera.pos;
    for (int32_t i = 0; i < g_Stage.data.instance_count; i++) {
        const STAGE_INSTANCE *const instance = &g_Stage.data.instances[i];
        const STAGE_OBJECT *const object =
            &g_Stage.data.objects[instance->object];
        if (object->layer != layer || !M_ObjectVisible(object, instance)) {
            continue;
        }
        for (int32_t j = 0; j < object->quad_count; j++) {
            draw.part = (uint32_t)i << 16 | (uint32_t)j;
            const STAGE_QUAD *const quad = &object->quads[j];
            ANM_VM *const vm = &g_Stage.data.sprites[quad->sprite];
            if (quad->type != 0) {
                continue;
            }
            vm->pos = (VEC3) {
                vm->pos_offset.x + quad->pos.x + instance->pos.x - camera.x,
                vm->pos_offset.y + quad->pos.y + instance->pos.y - camera.y,
                vm->pos_offset.z + quad->pos.z + instance->pos.z - camera.z,
            };
            if (vm->sprite_data == nullptr) {
                continue;
            }
            if (quad->width != 0.0f) {
                vm->scale_x = quad->width / vm->sprite_data->width;
            }
            if (quad->height != 0.0f) {
                vm->scale_y = quad->height / vm->sprite_data->height;
            }
            if (vm->auto_rotate == 2) {
                M_DrawProjected(vm, quad, &stage, draw.part);
            } else {
                Anm_Draw3D(vm, &draw);
            }
        }
    }
}

static bool M_LayersShown(void)
{
    return g_Stage.spell_card.state <= 1 && !Gui_IsLoadingShown();
}

// Returns the fog color modulated by tint: red, green and blue only,
// though the sprites' modulation scales alpha too.
static uint32_t M_ModulateFog(const uint32_t color, const uint32_t tint)
{
    uint32_t modulated = color & 0xff000000u;
    for (int32_t shift = 0; shift < 24; shift += 8) {
        uint32_t channel =
            ((color >> shift) & 0xffu) * ((tint >> shift) & 0xffu) >> 7;
        if (channel >= 0x100) {
            channel = 0xff;
        }
        modulated |= channel << shift;
    }
    return modulated;
}

static CHAIN_STATUS M_Draw(void *const arg)
{
    Anm_SetLastCamera(-1);
    if (g_Stage.clear_once) {
        GFX_VIEW view = GfxView_Screen();
        view.viewport = (VIEWPORT) { 32.0f, 16.0f, 384.0f, 448.0f, 0.0f, 1.0f };
        Gfx_SetView(&view);
        Gfx_Clear(GFX_CLEAR_COLOR, M_Rgba(COLOR_ARGB(0xff, 0x00, 0x00, 0x00)));
        g_Stage.clear_once = false;
    }
    const uint32_t tint = g_Stage.tint.color;
    const bool modulate = (tint >> 24) != 0;
    if (modulate) {
        Anm_SetColorModulation(tint);
    }
    g_Stage.tint.color = COLOR_ARGB(0x00, 0x80, 0x80, 0x80);
    if (M_LayersShown()) {
        const ANM_DRAW draw = GameManager_PlayAreaDraw();
        for (int32_t i = 0; i < 2; i++) {
            if (g_Stage.layers[i].sprite > 0) {
                Anm_Draw(&g_Stage.layers[i], &draw);
            }
        }
    }
    GFX_VIEW view =
        Anm_ScreenView(g_GameManager.camera.viewport, ANM_VIEW_PLAY_AREA);
    Gfx_SetView(&view);
    Gfx_Clear(
        g_Stage.clear_color != 0 ? GFX_CLEAR_COLOR | GFX_CLEAR_DEPTH
                                 : GFX_CLEAR_DEPTH,
        M_Rgba(g_Stage.clear_color));
    const uint32_t color = g_Stage.fog.current.color;
    memset(&m_Fog, 0, sizeof(m_Fog));
    m_Fog.enabled = true;
    m_Fog.begin = g_Stage.fog.current.near;
    m_Fog.end = g_Stage.fog.current.far;
    m_Fog.color = M_Rgba(modulate ? M_ModulateFog(color, tint) : color);
    if (M_LayersShown()) {
        M_DrawLayer(0);
        M_DrawLayer(1);
    }
    return CHAIN_CONTINUE;
}

static CHAIN_STATUS M_Draw2(void *const arg)
{
    if (M_LayersShown()) {
        M_DrawLayer(2);
        M_DrawLayer(3);
        Effect_DrawPetals();
        if (g_Stage.spell_card.state == 1) {
            Anm_DrawRect(
                (GFX_RECT) { 32, 16, 384, 448 },
                (uint32_t)(uint8_t)(g_Stage.spell_card.frames * 0xff / 60)
                    << 24);
        }
    }
    if (g_Stage.spell_card.state >= 1) {
        ANM_DRAW draw = GameManager_PlayAreaDraw();
        for (int32_t i = 0; i < g_Stage.spell_background.count; i++) {
            draw.part = (uint32_t)i;
            Anm_Draw(&g_Stage.spell_background.sprites[i], &draw);
        }
    }
    const GFX_VIEW fixed = M_FixedView();
    const GFX_VIEW stage = M_StageView();
    Anm_SetCameras(&fixed, &stage, g_Stage.camera.current.right);
    Anm_SetCamera(ANM_CAMERA_FIXED);
    Anm_SetLastCamera(ANM_CAMERA_FIXED);
    if (!g_Stage.tint.keep) {
        Anm_ResetColorModulation();
    }
    g_Stage.tint.keep = false;
    return CHAIN_CONTINUE;
}

static RESULT M_Added(void *const arg)
{
    const int32_t stage = g_GameManager.run.stage;
    FAIL_IF(stage < 0 || stage > 8, "no stage %d", stage);
    Timer_Set(&g_Stage.script.time, 0);
    g_Stage.script.index = 0;
    g_Stage.camera.pos = (VEC3) {};
    g_Stage.spell_card.state = 0;
    g_Stage.fog.frames = 0;
    const RESULT result = M_Load(stage);
    if (!IS_OK(result)) {
        M_Release();
        return result;
    }
    g_Stage.fog.current = (STAGE_FOG) {
        .near = 200.0f,
        .far = 500.0f,
        .color = COLOR_ARGB(0xff, 0x00, 0x00, 0x00),
    };
    STAGE_CAMERA *const camera = &g_Stage.camera.current;
    camera->eye = (VEC3) { 0.0f, 0.0f, 1000.0f };
    camera->at = (VEC3) { 0.0f, 0.0f, 0.0f };
    camera->up = (VEC3) { 0.0f, 1.0f, 0.0f };
    camera->fov = M_FOV;
    g_Stage.camera.start = *camera;
    g_Stage.camera.end = *camera;
    for (int32_t i = 0; i < STAGE_FADE_COUNT; i++) {
        g_Stage.camera.frames[i] = 0;
        Timer_Set(&g_Stage.camera.time[i], 0);
    }
    g_Stage.script.label = 0;
    return OK;
}

static void M_Deleted(void *const arg)
{
    M_Release();
}

RESULT Stage_Register(void)
{
    memset(&g_Stage, 0, sizeof(g_Stage));
    Chain_InitElem(&m_CalcElem, M_Update, nullptr);
    m_CalcElem.added = M_Added;
    m_CalcElem.deleted = M_Deleted;
    MUST(Chain_AddCalc(&m_CalcElem, M_CALC_PRIORITY));
    Chain_InitElem(&m_DrawElem, M_Draw, nullptr);
    MUST(Chain_AddDraw(&m_DrawElem, M_DRAW_PRIORITY));
    Chain_InitElem(&m_Draw2Elem, M_Draw2, nullptr);
    return Chain_AddDraw(&m_Draw2Elem, M_DRAW2_PRIORITY);
}

void Stage_Remove(void)
{
    Chain_Remove(&m_CalcElem);
    Chain_Remove(&m_DrawElem);
    Chain_Remove(&m_Draw2Elem);
}

void Stage_SetTint(const uint32_t color)
{
    if ((g_Stage.tint.color >> 24) == 0) {
        g_Stage.tint.color = color;
        return;
    }
    uint32_t average = 0;
    for (int32_t shift = 0; shift < 32; shift += 8) {
        const uint32_t a = (color >> shift) & 0xffu;
        const uint32_t b = (g_Stage.tint.color >> shift) & 0xffu;
        average |= ((a + b) >> 1) << shift;
    }
    g_Stage.tint.color = average;
}
