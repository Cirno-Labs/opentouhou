#pragma once

#include "core/base/types.h"

typedef enum {
    JSON_NULL,
    JSON_BOOL,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT,
} JSON_TYPE;

typedef struct JSON_VALUE JSON_VALUE;

// Holds one member of an object: its key, a NUL-terminated UTF-8 string,
// and its value. A \u0000 escape in a key ends the key early.
typedef struct {
    const char *key;
    JSON_VALUE *value;
} JSON_MEMBER;

// Holds one value. type selects the member of the union that is valid.
struct JSON_VALUE {
    JSON_TYPE type;
    union {
        bool boolean;
        // value holds every number. is_integer is true when the number has
        // no fraction and no exponent and fits in int64_t, in decimal or in
        // hexadecimal. integer then holds it exactly, also above 2^53, where
        // value is rounded.
        struct {
            double value;
            int64_t integer;
            bool is_integer;
        } number;
        // data is UTF-8 and NUL-terminated. size counts the bytes before the
        // terminator, including any NUL bytes from \u0000 escapes.
        struct {
            const char *data;
            size_t size;
        } string;
        struct {
            JSON_VALUE *items;
            int32_t count;
        } array;
        // members are in the order of the text, duplicate keys included.
        struct {
            JSON_MEMBER *members;
            int32_t count;
        } object;
    };
};
