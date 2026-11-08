#include "core/image.h"

#include "core/base/assert.h"
#include "core/base/log.h"
#include "core/image/codec.h"

#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>

// Encodes image into a packet with codec_id, converting to pix_fmt through
// swscale first, and writes the packets the encoder produces to writer. A
// failure anywhere in here is fatal: there is no RESULT in Image_EncodePng
// and Image_EncodeJpeg, and the only possible causes are out of memory or a
// bug in how the encoder is driven, never bad image data.
static void M_Encode(
    const IMAGE *const image, const enum AVCodecID codec_id,
    const enum AVPixelFormat pix_fmt, const int global_quality,
    WRITER *const writer)
{
    const AVCodec *const codec = avcodec_find_encoder(codec_id);
    if (codec == nullptr) {
        LOG_FATAL("no encoder for codec %d", codec_id);
    }
    AVCodecContext *context = avcodec_alloc_context3(codec);
    context->width = image->width;
    context->height = image->height;
    context->pix_fmt = pix_fmt;
    context->time_base = (AVRational) { 1, 1 };
    if (global_quality >= 0) {
        context->flags |= AV_CODEC_FLAG_QSCALE;
        context->global_quality = global_quality;
    }
    if (avcodec_open2(context, codec, nullptr) < 0) {
        LOG_FATAL("failed to open the encoder");
    }

    struct SwsContext *const sws = sws_getContext(
        image->width, image->height, AV_PIX_FMT_RGBA, image->width,
        image->height, pix_fmt, IMAGE_SWS_FLAGS, nullptr, nullptr, nullptr);
    if (sws == nullptr) {
        LOG_FATAL("failed to convert the image from RGBA");
    }

    AVFrame *frame = av_frame_alloc();
    frame->format = pix_fmt;
    frame->width = image->width;
    frame->height = image->height;
    if (av_frame_get_buffer(frame, 0) < 0) {
        LOG_FATAL("failed to allocate the encoder's frame");
    }
    const uint8_t *const src_planes[1] = { image->pixels };
    const int src_strides[1] = { image->width * 4 };
    sws_scale(
        sws, src_planes, src_strides, 0, image->height, frame->data,
        frame->linesize);

    AVPacket *packet = av_packet_alloc();
    if (avcodec_send_frame(context, frame) < 0
        || avcodec_send_frame(context, nullptr) < 0) {
        LOG_FATAL("failed to encode the image");
    }
    for (;;) {
        const int ret = avcodec_receive_packet(context, packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        }
        if (ret < 0) {
            LOG_FATAL("failed to encode the image");
        }
        Writer_WriteBytes(writer, packet->data, (size_t)packet->size);
        av_packet_unref(packet);
    }

    av_packet_free(&packet);
    av_frame_free(&frame);
    sws_freeContext(sws);
    avcodec_free_context(&context);
}

void Image_EncodePng(const IMAGE *const image, WRITER *const writer)
{
    M_Encode(image, AV_CODEC_ID_PNG, AV_PIX_FMT_RGBA, -1, writer);
}

void Image_EncodeJpeg(
    const IMAGE *const image, const int32_t quality, WRITER *const writer)
{
    ASSERT(quality >= 1 && quality <= 100);
    // FFmpeg's quantiser scale runs from 1 (best) to 31 (worst), the
    // opposite way around from the quality argument, and FF_QP2LAMBDA
    // converts a quantiser step into the lambda unit global_quality uses.
    const int qscale = 1 + (100 - quality) * 30 / 99;
    M_Encode(
        image, AV_CODEC_ID_MJPEG, AV_PIX_FMT_YUVJ420P, qscale * FF_QP2LAMBDA,
        writer);
}
