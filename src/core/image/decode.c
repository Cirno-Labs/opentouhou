#include "core/image.h"

#include "core/base/memory.h"
#include "core/image/codec.h"

#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>

#include <string.h>

// Returns the codec the data's magic bytes name, or AV_CODEC_ID_NONE when
// none of the three known ones matches.
static enum AVCodecID M_DetectCodec(
    const uint8_t *const data, const size_t size)
{
    static const uint8_t PNG_MAGIC[] = { 0x89, 'P',  'N',  'G',
                                         '\r', '\n', 0x1a, '\n' };
    if (size >= sizeof(PNG_MAGIC)
        && memcmp(data, PNG_MAGIC, sizeof(PNG_MAGIC)) == 0) {
        return AV_CODEC_ID_PNG;
    }
    if (size >= 2 && data[0] == 0xff && data[1] == 0xd8) {
        return AV_CODEC_ID_MJPEG;
    }
    if (size >= 2 && data[0] == 'B' && data[1] == 'M') {
        return AV_CODEC_ID_BMP;
    }
    return AV_CODEC_ID_NONE;
}

// Fills out with an empty image. Image_Free on it does nothing.
static void M_Empty(IMAGE *const out, const ALLOCATOR *const allocator)
{
    *out = (IMAGE) { .allocator = allocator };
}

RESULT Image_Decode(
    READER *const reader, const ALLOCATOR *const allocator, IMAGE *const out)
{
    M_Empty(out, allocator);

    const size_t size = Reader_Remaining(reader);
    uint8_t *const data = Mem_AllocZ(size + AV_INPUT_BUFFER_PADDING_SIZE);
    Reader_ReadBytes(reader, data, size);
    if (Reader_Failed(reader)) {
        const RESULT result = Reader_Result(reader);
        Mem_Free(data);
        return result;
    }

    const enum AVCodecID codec_id = M_DetectCodec(data, size);
    if (codec_id == AV_CODEC_ID_NONE) {
        Mem_Free(data);
        return FAIL("unrecognized image format");
    }
    const AVCodec *const codec = avcodec_find_decoder(codec_id);
    AVCodecContext *context = avcodec_alloc_context3(codec);
    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    struct SwsContext *sws = nullptr;
    RESULT result = OK;

    if (codec == nullptr || avcodec_open2(context, codec, nullptr) < 0) {
        result = FAIL("failed to open the decoder");
        goto done;
    }
    packet->data = data;
    packet->size = (int)size;
    if (avcodec_send_packet(context, packet) < 0
        || avcodec_receive_frame(context, frame) < 0) {
        result = FAIL("failed to decode the image");
        goto done;
    }
    if (frame->width <= 0 || frame->height <= 0 || frame->width > IMAGE_MAX_SIZE
        || frame->height > IMAGE_MAX_SIZE) {
        result = FAIL("image is %dx%d, too large", frame->width, frame->height);
        goto done;
    }

    sws = sws_getContext(
        frame->width, frame->height, (enum AVPixelFormat)frame->format,
        frame->width, frame->height, AV_PIX_FMT_RGBA, IMAGE_SWS_FLAGS, nullptr,
        nullptr, nullptr);
    if (sws == nullptr) {
        result = FAIL("failed to convert the image to RGBA");
        goto done;
    }

    *out = Image_Create(frame->width, frame->height, allocator);
    uint8_t *dst_planes[1] = { out->pixels };
    int dst_strides[1] = { frame->width * 4 };
    sws_scale(
        sws, (const uint8_t *const *)frame->data, frame->linesize, 0,
        frame->height, dst_planes, dst_strides);

done:
    if (sws != nullptr) {
        sws_freeContext(sws);
    }
    av_frame_free(&frame);
    av_packet_free(&packet);
    avcodec_free_context(&context);
    Mem_Free(data);
    if (!IS_OK(result)) {
        Image_Free(out);
        M_Empty(out, allocator);
    }
    return result;
}
