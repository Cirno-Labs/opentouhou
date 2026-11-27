#include "core/image/jpeg.h"

#include "core/image/codec.h"

#include <jpeglib.h>
#include <limits.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    struct jpeg_error_mgr manager;
    jmp_buf jump;
    char message[JMSG_LENGTH_MAX];
} M_ERROR;

typedef struct {
    struct jpeg_decompress_struct decoder;
    M_ERROR error;
    IMAGE image;
    uint8_t *row;
} M_STATE;

static void M_Error(j_common_ptr common)
{
    M_ERROR *const error = (M_ERROR *)common->err;
    common->err->format_message(common, error->message);
    longjmp(error->jump, 1);
}

static void M_Quiet(j_common_ptr common)
{
}

static uint8_t M_Clamp(const int32_t value)
{
    return value < 0 ? 0 : value > 255 ? 255 : (uint8_t)value;
}

static void M_ConvertRow(
    const uint8_t *const row, uint8_t *const pixels, const int32_t width,
    const J_COLOR_SPACE color)
{
    for (int32_t x = 0; x < width; x++) {
        uint8_t *const pixel = pixels + (size_t)x * 4;
        if (color == JCS_GRAYSCALE) {
            pixel[0] = pixel[1] = pixel[2] = row[x];
        } else if (color == JCS_RGB) {
            pixel[0] = row[x * 3];
            pixel[1] = row[x * 3 + 1];
            pixel[2] = row[x * 3 + 2];
        } else {
            const int32_t luma = row[x * 3];
            const int32_t cb = row[x * 3 + 1] - 128;
            const int32_t cr = row[x * 3 + 2] - 128;
            // Direct3D truncates eight-pixel groups and rounds the tail.
            if (x < width / 8 * 8) {
                pixel[0] = M_Clamp((luma * 256 + 359 * cr) / 256);
                pixel[1] = M_Clamp((luma * 256 - 88 * cb - 183 * cr) / 256);
                pixel[2] = M_Clamp((luma * 256 + 454 * cb) / 256);
            } else {
                pixel[0] = M_Clamp((luma * 65536 + 91881 * cr + 32768) / 65536);
                pixel[1] = M_Clamp(
                    (luma * 65536 - 22554 * cb - 46802 * cr + 32768) / 65536);
                pixel[2] =
                    M_Clamp((luma * 65536 + 116130 * cb + 32768) / 65536);
            }
        }
        pixel[3] = 255;
    }
}

static void M_Free(M_STATE *const state)
{
    jpeg_destroy_decompress(&state->decoder);
    Image_Free(&state->image);
    Mem_Free(state->row);
    Mem_Free(state);
}

RESULT Image_DecodeJpeg(
    const uint8_t *const data, const size_t size,
    const ALLOCATOR *const allocator, IMAGE *const out, bool *const handled)
{
    *handled = true;
    FAIL_IF(size > ULONG_MAX, "JPEG data is too large");
    M_STATE *const state = Mem_AllocZ(sizeof(*state));
    state->decoder.err = jpeg_std_error(&state->error.manager);
    state->error.manager.error_exit = M_Error;
    state->error.manager.output_message = M_Quiet;
    // Keep mutable decoder state on the heap across libjpeg's longjmp.
    if (setjmp(state->error.jump) != 0) {
        const RESULT result =
            FAIL("failed to decode JPEG: %s", state->error.message);
        M_Free(state);
        return result;
    }
    jpeg_create_decompress(&state->decoder);
    jpeg_mem_src(&state->decoder, data, (unsigned long)size);
    jpeg_read_header(&state->decoder, TRUE);
    const J_COLOR_SPACE color = state->decoder.jpeg_color_space;
    if (state->decoder.data_precision != 8
        || (color != JCS_YCbCr && color != JCS_GRAYSCALE && color != JCS_RGB)) {
        *handled = false;
        M_Free(state);
        return OK;
    }
    const JDIMENSION width = state->decoder.image_width;
    const JDIMENSION height = state->decoder.image_height;
    if (width == 0 || height == 0 || width > IMAGE_MAX_SIZE
        || height > IMAGE_MAX_SIZE) {
        M_Free(state);
        return FAIL(
            "JPEG image is %ux%u, too large", (unsigned)width,
            (unsigned)height);
    }
    state->decoder.out_color_space = color;
    state->decoder.dct_method = JDCT_ISLOW;
    state->decoder.do_fancy_upsampling = TRUE;
    jpeg_start_decompress(&state->decoder);
    state->image = Image_Create((int32_t)width, (int32_t)height, allocator);
    state->row = Mem_Alloc((size_t)width * state->decoder.output_components);
    while (state->decoder.output_scanline < height) {
        const JDIMENSION y = state->decoder.output_scanline;
        if (jpeg_read_scanlines(&state->decoder, &state->row, 1) != 1) {
            M_Free(state);
            return FAIL("failed to read a JPEG row");
        }
        M_ConvertRow(
            state->row, state->image.pixels + (size_t)y * width * 4,
            (int32_t)width, color);
    }
    jpeg_finish_decompress(&state->decoder);
    *out = state->image;
    memset(&state->image, 0, sizeof(state->image));
    M_Free(state);
    return OK;
}
