#include "core/input.h"

#include "core/base/assert.h"
#include "core/base/log.h"
#include "core/base/string.h"
#include "core/config.h"
#include "core/input/backend.h"
#include "core/input/codes.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define INPUT_MAX_BINDINGS_PER_DEVICE 8
#define INPUT_MAX_BINDINGS_PER_ACTION                                          \
    (INPUT_MAX_BINDINGS_PER_DEVICE * INPUT_DEVICE_COUNT)
#define INPUT_MAX_CONTROLLERS 16
#define INPUT_STICK_STEP (1.0f / 32767.0f)

// Maps a hat direction (see codes.h) to the bit position backend.h's
// INPUT_CONTROLLER_STATE.hats uses: bit 0 up, bit 1 right, bit 2 down, bit 3
// left.
static const int m_HatBitForDir[4] = { 0, 2, 3, 1 };

typedef struct {
    INPUT_BINDING bindings[INPUT_MAX_BINDINGS_PER_ACTION];
    // Holds whether each chord has started and none of its controls has been
    // released since.
    bool latched[INPUT_MAX_BINDINGS_PER_ACTION];
    // Holds whether each binding is held in the last Input_Update, after
    // suppression by chords.
    bool held[INPUT_MAX_BINDINGS_PER_ACTION];
    size_t count;
} ACTION_BINDINGS;

typedef struct {
    const INPUT_SCHEMA *schema;
    // [player][action], flattened as player * schema->count + action.
    ACTION_BINDINGS *bindings;
    // Holds every player's input struct from the last Input_Update.
    void *output;
} INPUT_SET;

typedef struct {
    char guid[33];
    int32_t player;
} CONTROLLER_OVERRIDE;

typedef struct {
    uint32_t id;
    INPUT_DEVICE device;
    char guid[33];
    int32_t player;
    bool connected;
} CONNECTED_CONTROLLER;

struct INPUT_STATE {
    const ALLOCATOR *allocator;
    INPUT_BACKEND *backend;
    // Holds the schema of Input_Create first, then those of Input_AddSchema.
    INPUT_SET sets[INPUT_MAX_SCHEMAS];
    size_t set_count;
    float stick_threshold;
    float stick_deadzone;
    CONTROLLER_OVERRIDE overrides[INPUT_MAX_CONTROLLERS];
    size_t override_count;
    CONNECTED_CONTROLLER connected[INPUT_MAX_CONTROLLERS];
    size_t connected_count;
    size_t next_connection_order;
    INPUT_DEVICES last_devices;
    // Holds the devices of the last Input_Update, to find when a trigger
    // goes down.
    uint8_t polled_keys[32];
    INPUT_CONTROLLER_STATE polled_controllers[INPUT_MAX_CONTROLLERS];
    size_t polled_controller_count;
    uint8_t prev_keys[32];
    uint8_t prev_gamepad_buttons[32];
    uint8_t prev_joy_buttons[32];
    uint8_t prev_joy_hats[4];
};

static const CONFIG_OPTION m_ConfigOptions[] = {
    CONFIG_FLOAT(
        INPUT_STATE, "input", "stick_threshold", stick_threshold, 0.5f, 0.0f,
        1.0f),
    CONFIG_FLOAT(
        INPUT_STATE, "input", "stick_deadzone", stick_deadzone, 0.15f, 0.0f,
        1.0f),
};
static const CONFIG_SCHEMA m_ConfigSchema =
    CONFIG_SCHEMA_OF(INPUT_STATE, m_ConfigOptions);

static const char *const m_DeviceSections[INPUT_DEVICE_COUNT] = {
    [INPUT_DEVICE_KEYBOARD] = "keyboard",
    [INPUT_DEVICE_GAMEPAD] = "gamepad",
    [INPUT_DEVICE_JOYSTICK] = "joystick",
};

static ACTION_BINDINGS *M_Bindings(
    const INPUT_SET *const set, const int32_t player, const size_t action)
{
    return &set->bindings[(size_t)player * set->schema->count + action];
}

static size_t M_BindingsSize(const INPUT_SCHEMA *const schema)
{
    return Mem_ArrSize(
        (size_t)schema->players * schema->count, sizeof(ACTION_BINDINGS));
}

static size_t M_OutputSize(const INPUT_SCHEMA *const schema)
{
    return Mem_ArrSize((size_t)schema->players, schema->size);
}

static const INPUT_SCHEMA *M_PlayerSchema(const INPUT_STATE *const input)
{
    return input->sets[0].schema;
}

static INPUT_SET *M_FindSet(
    const INPUT_STATE *const input, const INPUT_SCHEMA *const schema)
{
    for (size_t i = 0; i < input->set_count; i++) {
        if (input->sets[i].schema == schema) {
            return (INPUT_SET *)&input->sets[i];
        }
    }
    LOG_FATAL(
        "input: the schema was not given to Input_Create or Input_AddSchema");
}

static void *M_FieldAt(void *const out, const size_t offset)
{
    return (uint8_t *)out + offset;
}

static bool M_IsWholeStick(const INPUT_DEVICE device, const uint16_t code)
{
    switch (device) {
    case INPUT_DEVICE_GAMEPAD:
        return code == GAMEPAD_LEFT_STICK || code == GAMEPAD_RIGHT_STICK;
    case INPUT_DEVICE_JOYSTICK:
        return (code & JOY_TAG_MASK) == JOY_TAG_AXIS;
    default:
        return false;
    }
}

static bool M_ChordHasWholeStick(const INPUT_BINDING binding)
{
    if (binding.count < 2) {
        return false;
    }
    for (size_t i = 0; i < binding.count; i++) {
        if (M_IsWholeStick(binding.device, binding.codes[i])) {
            return true;
        }
    }
    return false;
}

static bool M_BindingEqual(const INPUT_BINDING a, const INPUT_BINDING b)
{
    if (a.device != b.device || a.count != b.count) {
        return false;
    }
    for (size_t i = 0; i < a.count; i++) {
        if (a.codes[i] != b.codes[i]) {
            return false;
        }
    }
    return true;
}

static void M_ClearHeld(ACTION_BINDINGS *const bindings)
{
    memset(bindings->latched, 0, sizeof(bindings->latched));
    memset(bindings->held, 0, sizeof(bindings->held));
}

// Parses a comma-separated list of binding names. A name that does not
// parse, or a chord with a whole stick in it, is fatal in the schema's
// defaults and a warning in the INI file.
static void M_ParseBindingList(
    const char *const text, INPUT_DEVICE device, const bool is_default,
    ACTION_BINDINGS *const out)
{
    *out = (ACTION_BINDINGS) {};
    if (text == nullptr) {
        return;
    }
    const char *p = text;
    while (*p != '\0') {
        while (*p == ' ' || *p == '\t' || *p == ',') {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        const char *start = p;
        while (*p != '\0' && *p != ',') {
            p++;
        }
        const char *end = p;
        while (end > start && (end[-1] == ' ' || end[-1] == '\t')) {
            end--;
        }
        char name[INPUT_BINDING_NAME_SIZE];
        const size_t length = MIN((size_t)(end - start), sizeof(name) - 1);
        memcpy(name, start, length);
        name[length] = '\0';
        if (name[0] == '\0') {
            continue;
        }
        INPUT_BINDING binding;
        const char *problem = nullptr;
        if (!Input_BindingFromName(name, &binding)
            || binding.device != device) {
            problem = "unknown binding";
        } else if (M_ChordHasWholeStick(binding)) {
            problem = "a chord with a whole stick";
        }
        if (problem == nullptr && out->count >= INPUT_MAX_BINDINGS_PER_DEVICE) {
            problem = "past the limit of bindings per device";
        }
        if (problem != nullptr && is_default) {
            LOG_FATAL("input: default \"%s\" is %s", name, problem);
        }
        if (problem != nullptr) {
            LOG_WARNING("input: \"%s\" is %s, skipped", name, problem);
        } else {
            out->bindings[out->count++] = binding;
        }
    }
}

static void M_SetDefaultsFor(
    INPUT_SET *const set, const int32_t player, const bool with_keyboard)
{
    for (size_t i = 0; i < set->schema->count; i++) {
        const INPUT_ACTION *const action = &set->schema->actions[i];
        for (INPUT_DEVICE device = 0; device < INPUT_DEVICE_COUNT; device++) {
            if (device == INPUT_DEVICE_KEYBOARD && !with_keyboard) {
                *M_Bindings(set, player, i) = (ACTION_BINDINGS) {};
                continue;
            }
            ACTION_BINDINGS parsed;
            M_ParseBindingList(action->defaults[device], device, true, &parsed);
            if (device == INPUT_DEVICE_KEYBOARD) {
                *M_Bindings(set, player, i) = parsed;
            } else {
                ACTION_BINDINGS *const bindings = M_Bindings(set, player, i);
                for (size_t j = 0; j < parsed.count
                     && bindings->count < INPUT_MAX_BINDINGS_PER_ACTION;
                     j++) {
                    bindings->bindings[bindings->count++] = parsed.bindings[j];
                }
            }
        }
    }
}

static void M_AddSchema(
    INPUT_STATE *const input, const INPUT_SCHEMA *const schema)
{
    ASSERT(schema->count <= INPUT_MAX_ACTIONS);
    ASSERT(schema->players >= 1 && schema->players <= INPUT_MAX_PLAYERS);
    ASSERT(input->set_count < INPUT_MAX_SCHEMAS);
    for (size_t i = 0; i < input->set_count; i++) {
        const INPUT_SCHEMA *const other = input->sets[i].schema;
        ASSERT(other != schema);
        for (size_t a = 0; a < schema->count; a++) {
            for (size_t b = 0; b < other->count; b++) {
                if (strcmp(schema->actions[a].name, other->actions[b].name)
                    == 0) {
                    LOG_FATAL(
                        "input: action \"%s\" is in two schemas",
                        schema->actions[a].name);
                }
            }
        }
    }
    INPUT_SET *const set = &input->sets[input->set_count++];
    set->schema = schema;
    set->bindings = Allocator_AllocZ(input->allocator, M_BindingsSize(schema));
    set->output = Allocator_AllocZ(input->allocator, M_OutputSize(schema));
    M_SetDefaultsFor(set, 0, true);
    if (schema->players == 2) {
        M_SetDefaultsFor(set, 1, false);
    }
}

INPUT_STATE *Input_Create(
    const INPUT_SCHEMA *const schema, const ALLOCATOR *const allocator)
{
    INPUT_STATE *const input = Allocator_AllocZ(allocator, sizeof(INPUT_STATE));
    input->allocator = allocator;
    input->backend = InputBackend_Create(allocator);
    input->stick_threshold = 0.5f;
    input->stick_deadzone = 0.15f;
    M_AddSchema(input, schema);
    return input;
}

void Input_Destroy(INPUT_STATE *const input)
{
    if (input == nullptr) {
        return;
    }
    InputBackend_Destroy(input->backend);
    for (size_t i = 0; i < input->set_count; i++) {
        const INPUT_SET *const set = &input->sets[i];
        Allocator_Free(
            input->allocator, set->bindings, M_BindingsSize(set->schema));
        Allocator_Free(
            input->allocator, set->output, M_OutputSize(set->schema));
    }
    Allocator_Free(input->allocator, input, sizeof(INPUT_STATE));
}

void Input_AddSchema(INPUT_STATE *const input, const INPUT_SCHEMA *const schema)
{
    ASSERT(schema->players == 1);
    M_AddSchema(input, schema);
}

void Input_Read(
    const INPUT_STATE *const input, const INPUT_SCHEMA *const schema,
    void *const out)
{
    const INPUT_SET *const set = M_FindSet(input, schema);
    memcpy(out, set->output, M_OutputSize(schema));
}

static void M_PlayerSection(
    const INPUT_SCHEMA *const schema, const int32_t player,
    const INPUT_DEVICE device, char *const out, const size_t size)
{
    if (player == 0) {
        snprintf(out, size, "%s.%s", "input", m_DeviceSections[device]);
    } else {
        snprintf(
            out, size, "%s.p%d.%s", "input", player + 1,
            m_DeviceSections[device]);
    }
}

static void M_LoadPlayerDevice(
    INPUT_SET *const set, const INI *const ini, const int32_t player,
    const INPUT_DEVICE device)
{
    char section[128];
    M_PlayerSection(set->schema, player, device, section, sizeof(section));
    for (size_t i = 0; i < set->schema->count; i++) {
        const char *const text =
            Ini_Get(ini, section, set->schema->actions[i].name);
        if (text == nullptr) {
            continue;
        }
        ACTION_BINDINGS *const bindings = M_Bindings(set, player, i);
        ACTION_BINDINGS parsed;
        M_ParseBindingList(text, device, false, &parsed);
        M_ClearHeld(bindings);
        size_t write = 0;
        for (size_t j = 0; j < bindings->count; j++) {
            if (bindings->bindings[j].device != device) {
                bindings->bindings[write++] = bindings->bindings[j];
            }
        }
        bindings->count = write;
        for (size_t j = 0; j < parsed.count
             && bindings->count < INPUT_MAX_BINDINGS_PER_ACTION;
             j++) {
            bindings->bindings[bindings->count++] = parsed.bindings[j];
        }
    }
}

void Input_Load(INPUT_STATE *const input, const INI *const ini)
{
    Config_Load(&m_ConfigSchema, ini, input);
    for (size_t i = 0; i < input->set_count; i++) {
        INPUT_SET *const set = &input->sets[i];
        for (int32_t player = 0; player < set->schema->players; player++) {
            for (INPUT_DEVICE device = 0; device < INPUT_DEVICE_COUNT;
                 device++) {
                M_LoadPlayerDevice(set, ini, player, device);
            }
        }
    }
    input->override_count = 0;
    for (int32_t player = 0; player < M_PlayerSchema(input)->players;
         player++) {
        char key[16];
        snprintf(key, sizeof(key), "p%d_controllers", player + 1);
        const char *const guids = Ini_Get(ini, "input", key);
        if (guids == nullptr) {
            continue;
        }
        const char *p = guids;
        while (*p != '\0') {
            while (*p == ' ' || *p == '\t' || *p == ',') {
                p++;
            }
            const char *start = p;
            while (*p != '\0' && *p != ',') {
                p++;
            }
            const char *end = p;
            while (end > start && (end[-1] == ' ' || end[-1] == '\t')) {
                end--;
            }
            if (end > start && input->override_count < INPUT_MAX_CONTROLLERS) {
                CONTROLLER_OVERRIDE *const o =
                    &input->overrides[input->override_count++];
                const size_t length = MIN((size_t)(end - start), 32u);
                memcpy(o->guid, start, length);
                o->guid[length] = '\0';
                o->player = player;
            }
        }
    }
}

static void M_SaveBindingList(
    INI *const ini, const char *const section, const char *const key,
    const ACTION_BINDINGS *const bindings, const INPUT_DEVICE device)
{
    char text[512] = "";
    size_t length = 0;
    for (size_t i = 0; i < bindings->count; i++) {
        if (bindings->bindings[i].device != device) {
            continue;
        }
        char name[INPUT_BINDING_NAME_SIZE];
        Input_BindingName(bindings->bindings[i], name);
        const size_t name_length = strlen(name);
        if (length + name_length + 2 >= sizeof(text)) {
            continue;
        }
        if (length > 0) {
            text[length++] = ',';
            text[length++] = ' ';
        }
        memcpy(text + length, name, name_length);
        length += name_length;
        text[length] = '\0';
    }
    Ini_Set(ini, section, key, text);
}

static void M_SavePlayerDevice(
    const INPUT_SET *const set, INI *const ini, const int32_t player,
    const INPUT_DEVICE device)
{
    char section[128];
    M_PlayerSection(set->schema, player, device, section, sizeof(section));
    for (size_t i = 0; i < set->schema->count; i++) {
        M_SaveBindingList(
            ini, section, set->schema->actions[i].name,
            M_Bindings(set, player, i), device);
    }
}

void Input_Save(const INPUT_STATE *const input, INI *const ini)
{
    Config_Save(&m_ConfigSchema, ini, input);
    for (size_t i = 0; i < input->set_count; i++) {
        const INPUT_SET *const set = &input->sets[i];
        for (int32_t player = 0; player < set->schema->players; player++) {
            for (INPUT_DEVICE device = 0; device < INPUT_DEVICE_COUNT;
                 device++) {
                M_SavePlayerDevice(set, ini, player, device);
            }
        }
    }
    for (int32_t player = 0; player < M_PlayerSchema(input)->players;
         player++) {
        char text[512] = "";
        size_t length = 0;
        for (size_t i = 0; i < input->override_count; i++) {
            if (input->overrides[i].player != player) {
                continue;
            }
            const size_t guid_length = strlen(input->overrides[i].guid);
            if (length + guid_length + 2 >= sizeof(text)) {
                continue;
            }
            if (length > 0) {
                text[length++] = ',';
                text[length++] = ' ';
            }
            memcpy(text + length, input->overrides[i].guid, guid_length);
            length += guid_length;
            text[length] = '\0';
        }
        char key[16];
        snprintf(key, sizeof(key), "p%d_controllers", player + 1);
        Ini_Set(ini, "input", key, text);
    }
}

static bool M_Bit(const uint8_t *const bits, const int index)
{
    return (bits[index / 8] & (1u << (index % 8))) != 0;
}

static void M_SetBit(uint8_t *const bits, const int index)
{
    bits[index / 8] |= (uint8_t)(1u << (index % 8));
}

static bool M_IsModifierKey(const uint16_t code)
{
    return code >= KEY_LEFT_MODIFIERS
        && code < KEY_RIGHT_MODIFIERS + KEY_MODIFIER_COUNT;
}

static uint16_t M_ModifierIndex(const uint16_t code)
{
    if (code >= KEY_GENERIC_MODIFIERS) {
        return code - KEY_GENERIC_MODIFIERS;
    }
    return (code - KEY_LEFT_MODIFIERS) % KEY_MODIFIER_COUNT;
}

static bool M_GenericModifierDown(
    const uint8_t *const keys, const uint16_t index)
{
    return M_Bit(keys, KEY_LEFT_MODIFIERS + index)
        || M_Bit(keys, KEY_RIGHT_MODIFIERS + index);
}

static bool M_KeyCodeDown(const uint8_t *const keys, const uint16_t code)
{
    if (code >= KEY_GENERIC_MODIFIERS) {
        return code < KEY_GENERIC_MODIFIERS + KEY_MODIFIER_COUNT
            && M_GenericModifierDown(keys, M_ModifierIndex(code));
    }
    return M_Bit(keys, code);
}

static bool M_GamepadButtonBit(
    const INPUT_CONTROLLER_STATE *const controller, const int bit)
{
    return (controller->buttons[bit / 8] & (1u << (bit % 8))) != 0;
}

static bool M_GamepadCodeDown(
    const INPUT_CONTROLLER_STATE *const controller, const uint16_t code,
    const float threshold)
{
    if (code <= GAMEPAD_DPAD_RIGHT) {
        return M_GamepadButtonBit(controller, code);
    }
    switch (code) {
    case GAMEPAD_LEFT_TRIGGER:
        return controller->axes[GAMEPAD_AXIS_LEFT_TRIGGER] >= threshold;
    case GAMEPAD_RIGHT_TRIGGER:
        return controller->axes[GAMEPAD_AXIS_RIGHT_TRIGGER] >= threshold;
    case GAMEPAD_LEFT_STICK_UP:
        return controller->axes[GAMEPAD_AXIS_LEFT_STICK_Y] <= -threshold;
    case GAMEPAD_LEFT_STICK_DOWN:
        return controller->axes[GAMEPAD_AXIS_LEFT_STICK_Y] >= threshold;
    case GAMEPAD_LEFT_STICK_LEFT:
        return controller->axes[GAMEPAD_AXIS_LEFT_STICK_X] <= -threshold;
    case GAMEPAD_LEFT_STICK_RIGHT:
        return controller->axes[GAMEPAD_AXIS_LEFT_STICK_X] >= threshold;
    case GAMEPAD_RIGHT_STICK_UP:
        return controller->axes[GAMEPAD_AXIS_RIGHT_STICK_Y] <= -threshold;
    case GAMEPAD_RIGHT_STICK_DOWN:
        return controller->axes[GAMEPAD_AXIS_RIGHT_STICK_Y] >= threshold;
    case GAMEPAD_RIGHT_STICK_LEFT:
        return controller->axes[GAMEPAD_AXIS_RIGHT_STICK_X] <= -threshold;
    case GAMEPAD_RIGHT_STICK_RIGHT:
        return controller->axes[GAMEPAD_AXIS_RIGHT_STICK_X] >= threshold;
    default:
        return false;
    }
}

static bool M_JoystickCodeDown(
    const INPUT_CONTROLLER_STATE *const controller, const uint16_t code,
    const float threshold)
{
    const uint16_t tag = code & JOY_TAG_MASK;
    const uint16_t index = code & JOY_INDEX_MASK;
    switch (tag) {
    case JOY_TAG_BUTTON:
        return M_GamepadButtonBit(controller, index);
    case JOY_TAG_HAT_DIR: {
        const int hat = index / 4;
        const int dir = index % 4;
        if (hat >= 4) {
            return false;
        }
        return (controller->hats[hat] & (1u << m_HatBitForDir[dir])) != 0;
    }
    case JOY_TAG_AXIS_DIR: {
        const int axis = index / 2;
        const int dir = index % 2;
        if (axis >= 8) {
            return false;
        }
        return dir == JOY_AXIS_DIR_POSITIVE
            ? controller->axes[axis] >= threshold
            : controller->axes[axis] <= -threshold;
    }
    default:
        return false;
    }
}

static int32_t M_PlayerOf(const INPUT_STATE *const input, const uint32_t id)
{
    for (size_t i = 0; i < input->connected_count; i++) {
        if (input->connected[i].id == id && input->connected[i].connected) {
            return input->connected[i].player;
        }
    }
    return -1;
}

static int32_t M_OverridePlayer(
    const INPUT_STATE *const input, const char *const guid)
{
    for (size_t i = 0; i < input->override_count; i++) {
        if (strcmp(input->overrides[i].guid, guid) == 0) {
            return input->overrides[i].player;
        }
    }
    return -1;
}

static void M_TrackControllers(
    INPUT_STATE *const input, const INPUT_DEVICES *const devices)
{
    for (size_t i = 0; i < input->connected_count; i++) {
        input->connected[i].connected = false;
    }
    for (size_t i = 0; i < devices->controller_count; i++) {
        const INPUT_CONTROLLER_STATE *const state = &devices->controllers[i];
        CONNECTED_CONTROLLER *entry = nullptr;
        for (size_t j = 0; j < input->connected_count; j++) {
            if (input->connected[j].id == state->id) {
                entry = &input->connected[j];
                break;
            }
        }
        if (entry == nullptr
            && input->connected_count < INPUT_MAX_CONTROLLERS) {
            entry = &input->connected[input->connected_count++];
            entry->id = state->id;
            entry->device = state->device;
            memcpy(entry->guid, state->guid, sizeof(entry->guid));
            const int32_t overridden = M_OverridePlayer(input, state->guid);
            if (overridden >= 0) {
                entry->player = overridden;
            } else if (M_PlayerSchema(input)->players == 1) {
                entry->player = 0;
            } else {
                entry->player =
                    (int32_t)(input->next_connection_order
                              % (size_t)M_PlayerSchema(input)->players);
                input->next_connection_order++;
            }
        }
        if (entry != nullptr) {
            entry->connected = true;
        }
    }
}

static bool M_ControlDown(
    const INPUT_STATE *const input, const INPUT_DEVICES *const devices,
    const INPUT_DEVICE device, const uint16_t code, const int32_t player)
{
    if (device == INPUT_DEVICE_KEYBOARD) {
        return M_KeyCodeDown(devices->keys, code);
    }
    for (size_t i = 0; i < devices->controller_count; i++) {
        const INPUT_CONTROLLER_STATE *const state = &devices->controllers[i];
        if (state->device != device || M_PlayerOf(input, state->id) != player) {
            continue;
        }
        const bool down = device == INPUT_DEVICE_GAMEPAD
            ? M_GamepadCodeDown(state, code, input->stick_threshold)
            : M_JoystickCodeDown(state, code, input->stick_threshold);
        if (down) {
            return true;
        }
    }
    return false;
}

static bool M_ChordDown(
    const INPUT_STATE *const input, const INPUT_DEVICES *const devices,
    const INPUT_DEVICES *const previous, const INPUT_BINDING binding,
    const bool latched, const int32_t player)
{
    for (size_t i = 0; i < binding.count; i++) {
        if (!M_ControlDown(
                input, devices, binding.device, binding.codes[i], player)) {
            return false;
        }
    }
    if (binding.count == 1 || latched) {
        return true;
    }
    return !M_ControlDown(
        input, previous, binding.device, binding.codes[binding.count - 1],
        player);
}

// Returns whether a control of a binding stands for a control of a chord:
// the same code, or a generic keyboard modifier and its left or right key.
static bool M_ControlMatches(
    const INPUT_DEVICE device, const uint16_t a, const uint16_t b)
{
    if (a == b) {
        return true;
    }
    if (device != INPUT_DEVICE_KEYBOARD) {
        return false;
    }
    const bool a_generic = a >= KEY_GENERIC_MODIFIERS;
    const bool b_generic = b >= KEY_GENERIC_MODIFIERS;
    if (a_generic == b_generic) {
        return false;
    }
    const uint16_t specific = a_generic ? b : a;
    return M_IsModifierKey(specific)
        && M_ModifierIndex(a) == M_ModifierIndex(b);
}

static bool M_InChord(const INPUT_BINDING binding, const INPUT_BINDING chord)
{
    if (binding.device != chord.device || binding.count >= chord.count) {
        return false;
    }
    for (size_t i = 0; i < binding.count; i++) {
        bool found = false;
        for (size_t j = 0; j < chord.count && !found; j++) {
            found = M_ControlMatches(
                binding.device, binding.codes[i], chord.codes[j]);
        }
        if (!found) {
            return false;
        }
    }
    return true;
}

static bool M_IsButton(const INPUT_SET *const set, const size_t action)
{
    return set->schema->actions[action].type == INPUT_ACTION_BUTTON;
}

static void M_SuppressInChord(
    INPUT_STATE *const input, const INPUT_BINDING chord, const int32_t player)
{
    for (size_t set_index = 0; set_index < input->set_count; set_index++) {
        INPUT_SET *const set = &input->sets[set_index];
        for (int32_t other = 0; other < set->schema->players; other++) {
            if (chord.device != INPUT_DEVICE_KEYBOARD && other != player) {
                continue;
            }
            for (size_t action = 0; action < set->schema->count; action++) {
                if (!M_IsButton(set, action)) {
                    continue;
                }
                ACTION_BINDINGS *const bindings =
                    M_Bindings(set, other, action);
                for (size_t i = 0; i < bindings->count; i++) {
                    if (M_InChord(bindings->bindings[i], chord)) {
                        bindings->held[i] = false;
                    }
                }
            }
        }
    }
}

// Sets whether every button binding of every set is held. Each chord is
// evaluated first, and every held chord then suppresses the bindings whose
// controls are all in it.
static void M_EvaluateBindings(
    INPUT_STATE *const input, const INPUT_DEVICES *const devices,
    const INPUT_DEVICES *const previous)
{
    for (size_t set_index = 0; set_index < input->set_count; set_index++) {
        INPUT_SET *const set = &input->sets[set_index];
        for (int32_t player = 0; player < set->schema->players; player++) {
            for (size_t action = 0; action < set->schema->count; action++) {
                if (!M_IsButton(set, action)) {
                    continue;
                }
                ACTION_BINDINGS *const bindings =
                    M_Bindings(set, player, action);
                for (size_t i = 0; i < bindings->count; i++) {
                    const bool down = M_ChordDown(
                        input, devices, previous, bindings->bindings[i],
                        bindings->latched[i], player);
                    bindings->latched[i] =
                        down && bindings->bindings[i].count > 1;
                    bindings->held[i] = down;
                }
            }
        }
    }
    for (size_t set_index = 0; set_index < input->set_count; set_index++) {
        const INPUT_SET *const set = &input->sets[set_index];
        for (int32_t player = 0; player < set->schema->players; player++) {
            for (size_t action = 0; action < set->schema->count; action++) {
                const ACTION_BINDINGS *const bindings =
                    M_Bindings(set, player, action);
                for (size_t i = 0; i < bindings->count; i++) {
                    if (bindings->latched[i]) {
                        M_SuppressInChord(input, bindings->bindings[i], player);
                    }
                }
            }
        }
    }
}

static void M_GamepadStickAxes(
    const uint16_t code, int *const x_axis, int *const y_axis)
{
    if (code == GAMEPAD_LEFT_STICK) {
        *x_axis = GAMEPAD_AXIS_LEFT_STICK_X;
        *y_axis = GAMEPAD_AXIS_LEFT_STICK_Y;
    } else {
        *x_axis = GAMEPAD_AXIS_RIGHT_STICK_X;
        *y_axis = GAMEPAD_AXIS_RIGHT_STICK_Y;
    }
}

static void M_StickValue(
    const INPUT_STATE *const input, const INPUT_DEVICES *const devices,
    const ACTION_BINDINGS *const bindings, const int32_t player,
    const INPUT_DEVICE filter, float *const out_x, float *const out_y)
{
    float best_x = 0.0f;
    float best_y = 0.0f;
    float best_mag = -1.0f;
    size_t i = 0;
    while (i < bindings->count) {
        const INPUT_BINDING binding = bindings->bindings[i];
        if (binding.count != 1) {
            i++;
        } else if (binding.device == INPUT_DEVICE_GAMEPAD) {
            if (filter == INPUT_DEVICE_COUNT
                || filter == INPUT_DEVICE_GAMEPAD) {
                int x_axis, y_axis;
                M_GamepadStickAxes(binding.codes[0], &x_axis, &y_axis);
                for (size_t j = 0; j < devices->controller_count; j++) {
                    const INPUT_CONTROLLER_STATE *const state =
                        &devices->controllers[j];
                    if (state->device != INPUT_DEVICE_GAMEPAD
                        || M_PlayerOf(input, state->id) != player) {
                        continue;
                    }
                    const float x = state->axes[x_axis];
                    const float y = state->axes[y_axis];
                    const float mag = x * x + y * y;
                    if (mag > best_mag) {
                        best_mag = mag;
                        best_x = x;
                        best_y = y;
                    }
                }
            }
            i++;
        } else if (
            binding.device == INPUT_DEVICE_JOYSTICK
            && i + 1 < bindings->count) {
            const uint16_t x_code = binding.codes[0];
            const uint16_t y_code = bindings->bindings[i + 1].codes[0];
            if (filter == INPUT_DEVICE_COUNT
                || filter == INPUT_DEVICE_JOYSTICK) {
                const int x_axis = (x_code & JOY_INDEX_MASK);
                const int y_axis = (y_code & JOY_INDEX_MASK);
                for (size_t j = 0; j < devices->controller_count; j++) {
                    const INPUT_CONTROLLER_STATE *const state =
                        &devices->controllers[j];
                    if (state->device != INPUT_DEVICE_JOYSTICK
                        || M_PlayerOf(input, state->id) != player) {
                        continue;
                    }
                    const float x = state->axes[x_axis];
                    const float y = state->axes[y_axis];
                    const float mag = x * x + y * y;
                    if (mag > best_mag) {
                        best_mag = mag;
                        best_x = x;
                        best_y = y;
                    }
                }
            }
            i += 2;
        } else {
            i++;
        }
    }
    const float deadzone = input->stick_deadzone;
    const float length = sqrtf(best_x * best_x + best_y * best_y);
    float x = 0.0f;
    float y = 0.0f;
    if (length > deadzone) {
        const float scale =
            MIN((length - deadzone) / (1.0f - deadzone), 1.0f) / length;
        x = best_x * scale;
        y = best_y * scale;
    }
    *out_x = roundf(x / INPUT_STICK_STEP) * INPUT_STICK_STEP;
    *out_y = roundf(y / INPUT_STICK_STEP) * INPUT_STICK_STEP;
}

static void M_UpdatePlayer(
    const INPUT_STATE *const input, const INPUT_SET *const set,
    const INPUT_DEVICES *const devices, const int32_t player,
    const INPUT_DEVICE filter, void *const out)
{
    for (size_t i = 0; i < set->schema->count; i++) {
        const INPUT_ACTION *const action = &set->schema->actions[i];
        const ACTION_BINDINGS *const bindings = M_Bindings(set, player, i);
        if (action->type == INPUT_ACTION_BUTTON) {
            bool down = false;
            for (size_t j = 0; j < bindings->count && !down; j++) {
                down = bindings->held[j]
                    && (filter == INPUT_DEVICE_COUNT
                        || bindings->bindings[j].device == filter);
            }
            *(bool *)M_FieldAt(out, action->offset) = down;
        } else {
            float x, y;
            M_StickValue(input, devices, bindings, player, filter, &x, &y);
            *(float *)M_FieldAt(out, action->offset) = x;
            *(float *)M_FieldAt(out, action->offset + sizeof(float)) = y;
        }
    }
}

void Input_Update(INPUT_STATE *const input, void *const out)
{
    INPUT_DEVICES devices;
    InputBackend_Poll(input->backend, &devices);
    M_TrackControllers(input, &devices);
    input->last_devices = devices;
    INPUT_DEVICES previous = {
        .controllers = input->polled_controllers,
        .controller_count = input->polled_controller_count,
    };
    memcpy(previous.keys, input->polled_keys, sizeof(previous.keys));
    M_EvaluateBindings(input, &devices, &previous);
    memcpy(input->polled_keys, devices.keys, sizeof(input->polled_keys));
    input->polled_controller_count =
        MIN(devices.controller_count, (size_t)INPUT_MAX_CONTROLLERS);
    memcpy(
        input->polled_controllers, devices.controllers,
        input->polled_controller_count * sizeof(INPUT_CONTROLLER_STATE));
    for (size_t i = 0; i < input->set_count; i++) {
        const INPUT_SET *const set = &input->sets[i];
        for (int32_t player = 0; player < set->schema->players; player++) {
            void *const player_out =
                (uint8_t *)set->output + (size_t)player * set->schema->size;
            M_UpdatePlayer(
                input, set, &devices, player, INPUT_DEVICE_COUNT, player_out);
        }
    }
    memcpy(out, input->sets[0].output, M_OutputSize(input->sets[0].schema));
}

void Input_GetDevice(
    const INPUT_STATE *const input, const INPUT_DEVICE device, void *const out)
{
    const INPUT_SET *const set = &input->sets[0];
    for (int32_t player = 0; player < set->schema->players; player++) {
        void *const player_out =
            (uint8_t *)out + (size_t)player * set->schema->size;
        M_UpdatePlayer(
            input, set, &input->last_devices, player, device, player_out);
    }
}

void Input_Debounce(
    const INPUT_SCHEMA *const schema, const void *const previous,
    const void *const current, void *const out)
{
    for (size_t i = 0; i < schema->count; i++) {
        const INPUT_ACTION *const action = &schema->actions[i];
        if (action->type == INPUT_ACTION_BUTTON) {
            const bool was =
                *(const bool *)((const uint8_t *)previous + action->offset);
            const bool is =
                *(const bool *)((const uint8_t *)current + action->offset);
            *(bool *)M_FieldAt(out, action->offset) = is && !was;
        } else {
            *(float *)M_FieldAt(out, action->offset) = 0.0f;
            *(float *)M_FieldAt(out, action->offset + sizeof(float)) = 0.0f;
        }
    }
}

size_t Input_ListControllers(
    const INPUT_STATE *const input, INPUT_CONTROLLER *const out,
    const size_t capacity)
{
    size_t count = 0;
    for (size_t i = 0; i < input->connected_count && count < capacity; i++) {
        if (!input->connected[i].connected) {
            continue;
        }
        const CONNECTED_CONTROLLER *const entry = &input->connected[i];
        const char *name = "";
        for (size_t j = 0; j < input->last_devices.controller_count; j++) {
            if (input->last_devices.controllers[j].id == entry->id) {
                name = input->last_devices.controllers[j].name;
                break;
            }
        }
        out[count++] = (INPUT_CONTROLLER) {
            .id = entry->id,
            .device = entry->device,
            .name = name,
            .player = entry->player,
        };
    }
    return count;
}

void Input_AssignController(
    INPUT_STATE *const input, const uint32_t id, const int32_t player)
{
    for (size_t i = 0; i < input->connected_count; i++) {
        if (input->connected[i].id != id) {
            continue;
        }
        input->connected[i].player = player;
        CONTROLLER_OVERRIDE *entry = nullptr;
        for (size_t j = 0; j < input->override_count; j++) {
            if (strcmp(input->overrides[j].guid, input->connected[i].guid)
                == 0) {
                entry = &input->overrides[j];
                break;
            }
        }
        if (entry == nullptr && input->override_count < INPUT_MAX_CONTROLLERS) {
            entry = &input->overrides[input->override_count++];
            memcpy(entry->guid, input->connected[i].guid, sizeof(entry->guid));
        }
        if (entry != nullptr) {
            entry->player = player;
        }
        return;
    }
}

size_t Input_GetBindings(
    const INPUT_STATE *const input, const INPUT_SCHEMA *const schema,
    const int32_t player, const size_t action, INPUT_BINDING *const out,
    const size_t capacity)
{
    const ACTION_BINDINGS *const bindings =
        M_Bindings(M_FindSet(input, schema), player, action);
    const size_t count = MIN(bindings->count, capacity);
    memcpy(out, bindings->bindings, count * sizeof(INPUT_BINDING));
    return bindings->count;
}

void Input_Bind(
    INPUT_STATE *const input, const INPUT_SCHEMA *const schema,
    const int32_t player, const size_t action, const INPUT_BINDING binding)
{
    ACTION_BINDINGS *const bindings =
        M_Bindings(M_FindSet(input, schema), player, action);
    for (size_t i = 0; i < bindings->count; i++) {
        if (M_BindingEqual(bindings->bindings[i], binding)) {
            return;
        }
    }
    size_t on_device = 0;
    for (size_t i = 0; i < bindings->count; i++) {
        on_device += bindings->bindings[i].device == binding.device;
    }
    if (on_device < INPUT_MAX_BINDINGS_PER_DEVICE) {
        bindings->bindings[bindings->count++] = binding;
        M_ClearHeld(bindings);
    }
}

void Input_Unbind(
    INPUT_STATE *const input, const INPUT_SCHEMA *const schema,
    const int32_t player, const size_t action, const INPUT_BINDING binding)
{
    ACTION_BINDINGS *const bindings =
        M_Bindings(M_FindSet(input, schema), player, action);
    size_t write = 0;
    for (size_t i = 0; i < bindings->count; i++) {
        if (!M_BindingEqual(bindings->bindings[i], binding)) {
            bindings->bindings[write++] = bindings->bindings[i];
        }
    }
    bindings->count = write;
    M_ClearHeld(bindings);
}

void Input_ResetBindings(
    INPUT_STATE *const input, const INPUT_SCHEMA *const schema,
    const int32_t player)
{
    M_SetDefaultsFor(M_FindSet(input, schema), player, player == 0);
}

static INPUT_BINDING M_KeyboardChord(
    const uint8_t *const keys, const uint16_t trigger)
{
    INPUT_BINDING chord = { .device = INPUT_DEVICE_KEYBOARD };
    for (uint16_t i = 0; i < KEY_MODIFIER_COUNT; i++) {
        if (M_IsModifierKey(trigger) && M_ModifierIndex(trigger) == i) {
            continue;
        }
        if (M_GenericModifierDown(keys, i)
            && chord.count < INPUT_CHORD_MAX - 1) {
            chord.codes[chord.count++] = KEY_GENERIC_MODIFIERS + i;
        }
    }
    chord.codes[chord.count++] = trigger;
    return chord;
}

static bool M_LastPressedKey(
    INPUT_STATE *const input, const INPUT_DEVICES *const devices,
    INPUT_BINDING *const out)
{
    int key = -1;
    int modifier = -1;
    for (int code = 0; code < 256; code++) {
        if (!M_Bit(devices->keys, code) || M_Bit(input->prev_keys, code)) {
            continue;
        }
        if (M_IsModifierKey((uint16_t)code)) {
            modifier = code;
        } else {
            key = code;
        }
    }
    memcpy(input->prev_keys, devices->keys, sizeof(input->prev_keys));
    const int trigger = key >= 0 ? key : modifier;
    if (trigger < 0) {
        return false;
    }
    *out = M_KeyboardChord(devices->keys, (uint16_t)trigger);
    return true;
}

// Builds the chord that ends with trigger and starts with the other held
// controls, which held lists in ascending code order.
static INPUT_BINDING M_ControllerChord(
    const INPUT_DEVICE device, const uint16_t *const held,
    const size_t held_count, const uint16_t trigger)
{
    INPUT_BINDING chord = { .device = device };
    for (size_t i = 0; i < held_count && chord.count < INPUT_CHORD_MAX - 1;
         i++) {
        if (held[i] != trigger) {
            chord.codes[chord.count++] = held[i];
        }
    }
    chord.codes[chord.count++] = trigger;
    return chord;
}

static bool M_LastPressedGamepad(
    INPUT_STATE *const input, const INPUT_DEVICES *const devices,
    INPUT_BINDING *const out)
{
    uint8_t buttons[32] = {};
    for (size_t i = 0; i < devices->controller_count; i++) {
        if (devices->controllers[i].device != INPUT_DEVICE_GAMEPAD) {
            continue;
        }
        for (int bit = 0; bit <= GAMEPAD_DPAD_RIGHT; bit++) {
            if (M_GamepadButtonBit(&devices->controllers[i], bit)) {
                M_SetBit(buttons, bit);
            }
        }
    }
    uint16_t held[GAMEPAD_DPAD_RIGHT + 1];
    size_t held_count = 0;
    int trigger = -1;
    for (int code = 0; code <= GAMEPAD_DPAD_RIGHT; code++) {
        if (!M_Bit(buttons, code)) {
            continue;
        }
        held[held_count++] = (uint16_t)code;
        if (!M_Bit(input->prev_gamepad_buttons, code)) {
            trigger = code;
        }
    }
    memcpy(
        input->prev_gamepad_buttons, buttons,
        sizeof(input->prev_gamepad_buttons));
    if (trigger < 0) {
        return false;
    }
    *out = M_ControllerChord(
        INPUT_DEVICE_GAMEPAD, held, held_count, (uint16_t)trigger);
    return true;
}

static bool M_LastPressedJoystick(
    INPUT_STATE *const input, const INPUT_DEVICES *const devices,
    INPUT_BINDING *const out)
{
    uint8_t buttons[32] = {};
    uint8_t hats[4] = {};
    for (size_t i = 0; i < devices->controller_count; i++) {
        if (devices->controllers[i].device != INPUT_DEVICE_JOYSTICK) {
            continue;
        }
        for (int bit = 0; bit < 256; bit++) {
            if (M_GamepadButtonBit(&devices->controllers[i], bit)) {
                M_SetBit(buttons, bit);
            }
        }
        for (int hat = 0; hat < 4; hat++) {
            hats[hat] |= devices->controllers[i].hats[hat];
        }
    }
    uint16_t held[256 + 16];
    size_t held_count = 0;
    bool found = false;
    uint16_t trigger = 0;
    for (int code = 0; code < 256; code++) {
        if (!M_Bit(buttons, code)) {
            continue;
        }
        held[held_count++] = Joy_Button(code);
        if (!M_Bit(input->prev_joy_buttons, code)) {
            found = true;
            trigger = Joy_Button(code);
        }
    }
    for (int hat = 0; hat < 4; hat++) {
        for (int dir = 0; dir < 4; dir++) {
            const int bit = m_HatBitForDir[dir];
            if ((hats[hat] & (1u << bit)) == 0) {
                continue;
            }
            held[held_count++] = Joy_HatDir(hat, dir);
            if ((input->prev_joy_hats[hat] & (1u << bit)) == 0) {
                found = true;
                trigger = Joy_HatDir(hat, dir);
            }
        }
    }
    memcpy(input->prev_joy_buttons, buttons, sizeof(input->prev_joy_buttons));
    memcpy(input->prev_joy_hats, hats, sizeof(input->prev_joy_hats));
    if (!found) {
        return false;
    }
    *out = M_ControllerChord(INPUT_DEVICE_JOYSTICK, held, held_count, trigger);
    return true;
}

bool Input_LastPressed(
    INPUT_STATE *const input, const INPUT_DEVICE device,
    INPUT_BINDING *const out)
{
    INPUT_DEVICES devices;
    InputBackend_Poll(input->backend, &devices);
    M_TrackControllers(input, &devices);
    switch (device) {
    case INPUT_DEVICE_KEYBOARD:
        return M_LastPressedKey(input, &devices, out);
    case INPUT_DEVICE_GAMEPAD:
        return M_LastPressedGamepad(input, &devices, out);
    case INPUT_DEVICE_JOYSTICK:
        return M_LastPressedJoystick(input, &devices, out);
    case INPUT_DEVICE_COUNT:
        break;
    }
    return false;
}
