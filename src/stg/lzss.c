#include "stg/lzss.h"

#include "core/base/memory.h"

#define M_WINDOW_SIZE 0x2000
#define M_WINDOW_MASK (M_WINDOW_SIZE - 1)
#define M_MAX_MATCH 0x12
#define M_MIN_MATCH 3
#define M_OFFSET_BITS 13
#define M_LENGTH_BITS 4
// Indexes the tree's root, one past the window positions. Links are window
// positions, and 0 is no link.
#define M_ROOT M_WINDOW_SIZE

typedef struct {
    READER *reader;
    uint32_t byte;
    int32_t bits_left;
    uint32_t checksum;
} BIT_READER;

typedef struct {
    WRITER *writer;
    uint8_t byte;
    uint8_t mask;
} BIT_WRITER;

typedef struct {
    int32_t parent;
    int32_t smaller_child;
    int32_t larger_child;
} TREE_NODE;

typedef struct {
    TREE_NODE tree[M_WINDOW_SIZE + 1];
    uint8_t window[M_WINDOW_SIZE];
} ENCODER;

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

static void M_WriteBits(
    BIT_WRITER *const bw, const uint32_t value, const int32_t count)
{
    for (uint32_t bit = 1u << (count - 1); bit != 0; bit >>= 1) {
        if ((value & bit) != 0) {
            bw->byte |= bw->mask;
        }
        bw->mask >>= 1;
        if (bw->mask == 0) {
            Writer_WriteU8(bw->writer, bw->byte);
            bw->byte = 0;
            bw->mask = 0x80;
        }
    }
}

static void M_InitTree(ENCODER *const enc, const int32_t pos)
{
    enc->tree[M_ROOT].larger_child = pos;
    enc->tree[pos].parent = M_ROOT;
    enc->tree[pos].larger_child = 0;
    enc->tree[pos].smaller_child = 0;
}

static void M_ReplaceChild(
    ENCODER *const enc, const int32_t old_node, const int32_t new_node)
{
    TREE_NODE *const tree = enc->tree;
    const int32_t parent = tree[old_node].parent;
    if (tree[parent].smaller_child == old_node) {
        tree[parent].smaller_child = new_node;
    } else {
        tree[parent].larger_child = new_node;
    }
    tree[new_node] = tree[old_node];
    tree[tree[new_node].smaller_child].parent = new_node;
    tree[tree[new_node].larger_child].parent = new_node;
    tree[old_node].parent = 0;
}

// Unlinks old_node, which has at most one child, new_node, and links the
// child to its parent. A new_node of 0 is no child, but node 0 is also
// window position 0, so its parent is set too, and M_DeleteNode then unlinks
// node 0 when the window reaches it, as the original does.
static void M_ReplaceNode(
    ENCODER *const enc, const int32_t old_node, const int32_t new_node)
{
    TREE_NODE *const tree = enc->tree;
    tree[new_node].parent = tree[old_node].parent;
    if (tree[tree[old_node].parent].larger_child == old_node) {
        tree[tree[old_node].parent].larger_child = new_node;
    } else {
        tree[tree[old_node].parent].smaller_child = new_node;
    }
    tree[old_node].parent = 0;
}

static int32_t M_Predecessor(const ENCODER *const enc, const int32_t node)
{
    int32_t next = enc->tree[node].smaller_child;
    while (enc->tree[next].larger_child != 0) {
        next = enc->tree[next].larger_child;
    }
    return next;
}

static void M_DeleteNode(ENCODER *const enc, const int32_t pos)
{
    const TREE_NODE *const node = &enc->tree[pos];
    if (node->parent == 0) {
        return;
    }
    if (node->larger_child == 0) {
        M_ReplaceNode(enc, pos, node->smaller_child);
    } else if (node->smaller_child == 0) {
        M_ReplaceNode(enc, pos, node->larger_child);
    } else {
        const int32_t replacement = M_Predecessor(enc, pos);
        M_DeleteNode(enc, replacement);
        M_ReplaceChild(enc, pos, replacement);
    }
}

// Adds the string at window position pos to the tree. Returns the length of
// the longest match and sets *match_pos to its position. Of equal lengths,
// the match found last on the search path wins.
static int32_t M_InsertNode(
    ENCODER *const enc, const int32_t pos, int32_t *const match_pos)
{
    if (pos == 0) {
        return 0;
    }
    TREE_NODE *const tree = enc->tree;
    const uint8_t *const window = enc->window;
    int32_t test = tree[M_ROOT].larger_child;
    int32_t best_length = 0;
    for (;;) {
        int32_t delta = 0;
        int32_t i;
        for (i = 0; i < M_MAX_MATCH; i++) {
            delta = window[(pos + i) & M_WINDOW_MASK]
                - window[(test + i) & M_WINDOW_MASK];
            if (delta != 0) {
                break;
            }
        }
        if (i >= best_length) {
            best_length = i;
            *match_pos = test;
            if (best_length >= M_MAX_MATCH) {
                M_ReplaceChild(enc, test, pos);
                return best_length;
            }
        }
        int32_t *const child =
            delta >= 0 ? &tree[test].larger_child : &tree[test].smaller_child;
        if (*child == 0) {
            *child = pos;
            tree[pos].parent = test;
            tree[pos].larger_child = 0;
            tree[pos].smaller_child = 0;
            return best_length;
        }
        test = *child;
    }
}

RESULT Lzss_Decode(
    READER *const input, uint8_t *const output, const size_t size,
    size_t *const decoded, uint32_t *const checksum)
{
    BIT_READER br = { .reader = input };
    uint8_t window[M_WINDOW_SIZE] = { 0 };
    uint32_t window_pos = 1;
    size_t count = 0;
    RESULT result = OK;

    while (count < size) {
        if (M_ReadBits(&br, 1) != 0) {
            const uint8_t byte = (uint8_t)M_ReadBits(&br, 8);
            output[count++] = byte;
            window[window_pos] = byte;
            window_pos = (window_pos + 1) & M_WINDOW_MASK;
            continue;
        }
        const uint32_t offset = M_ReadBits(&br, M_OFFSET_BITS);
        if (offset == 0) {
            break;
        }
        const uint32_t length = M_ReadBits(&br, M_LENGTH_BITS) + M_MIN_MATCH;
        if (length > size - count) {
            result = FAIL("match runs past the end of the output");
            break;
        }
        for (uint32_t i = 0; i < length; i++) {
            const uint8_t byte = window[(offset + i) & M_WINDOW_MASK];
            output[count++] = byte;
            window[window_pos] = byte;
            window_pos = (window_pos + 1) & M_WINDOW_MASK;
        }
    }

    *decoded = count;
    if (checksum != nullptr) {
        *checksum = br.checksum;
    }
    return result;
}

void Lzss_Encode(
    const uint8_t *const data, const size_t size, WRITER *const output)
{
    ENCODER *const enc = Mem_AllocZ(sizeof(*enc));
    BIT_WRITER bw = { .writer = output, .byte = 0, .mask = 0x80 };
    size_t in = 0;

    int32_t window_pos = 1;
    int32_t lookahead;
    for (lookahead = 0; lookahead < M_MAX_MATCH && in < size; lookahead++) {
        enc->window[window_pos + lookahead] = data[in++];
    }
    M_InitTree(enc, window_pos);
    int32_t length = 0;
    int32_t match_pos = 0;

    while (lookahead > 0) {
        if (length > lookahead) {
            length = lookahead;
        }
        int32_t replace_length;
        if (length < M_MIN_MATCH) {
            replace_length = 1;
            M_WriteBits(&bw, 1, 1);
            M_WriteBits(&bw, enc->window[window_pos], 8);
        } else {
            M_WriteBits(&bw, 0, 1);
            M_WriteBits(&bw, (uint32_t)match_pos, M_OFFSET_BITS);
            M_WriteBits(&bw, (uint32_t)(length - M_MIN_MATCH), M_LENGTH_BITS);
            replace_length = length;
        }
        for (int32_t i = 0; i < replace_length; i++) {
            const int32_t incoming = (window_pos + M_MAX_MATCH) & M_WINDOW_MASK;
            M_DeleteNode(enc, incoming);
            if (in < size) {
                enc->window[incoming] = data[in++];
            } else {
                lookahead--;
            }
            window_pos = (window_pos + 1) & M_WINDOW_MASK;
            if (lookahead != 0) {
                length = M_InsertNode(enc, window_pos, &match_pos);
            }
        }
    }
    M_WriteBits(&bw, 0, 1);
    M_WriteBits(&bw, 0, M_OFFSET_BITS);

    Mem_Free(enc);
}
