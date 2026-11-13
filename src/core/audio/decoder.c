// SPDX-License-Identifier: GPL-3.0-only

#include "core/audio/decoder.h"

#include "core/base/memory.h"
#include "core/base/utils.h"

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/error.h>
#include <libswresample/swresample.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>

#define AUDIO_IO_BUFFER_SIZE 32768
#define AUDIO_OUTPUT_CHANNELS 2

struct AUDIO_DECODER {
    READER *reader;
    AVFormatContext *format;
    AVIOContext *io;
    AVCodecContext *codec;
    AVPacket *packet;
    AVFrame *frame;
    SwrContext *resampler;
    int stream;
    AUDIO_INFO info;
    float *buffer;
    uint32_t capacity;
    uint32_t pending_offset;
    uint32_t pending_frames;
    bool flushing;
    bool finished;
};

static RESULT M_Error(const char *const operation, const int error)
{
    char message[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(error, message, sizeof(message));
    return FAIL("%s: %s", operation, message);
}

static int M_Read(void *const opaque, uint8_t *const buffer, const int size)
{
    READER *const reader = opaque;
    if (Reader_Failed(reader)) {
        return AVERROR(EIO);
    }
    const size_t count = MIN(Reader_Remaining(reader), (size_t)size);
    if (count == 0) {
        return AVERROR_EOF;
    }
    Reader_ReadBytes(reader, buffer, count);
    return Reader_Failed(reader) ? AVERROR(EIO) : (int)count;
}

static int64_t M_Seek(void *const opaque, const int64_t offset, int whence)
{
    READER *const reader = opaque;
    whence &= ~AVSEEK_FORCE;
    if (Reader_Size(reader) > INT64_MAX) {
        return AVERROR(EINVAL);
    }
    const int64_t size = (int64_t)Reader_Size(reader);
    if (whence == AVSEEK_SIZE) {
        return size;
    }
    int64_t base;
    switch (whence) {
    case SEEK_SET:
        base = 0;
        break;
    case SEEK_CUR:
        base = (int64_t)Reader_Tell(reader);
        break;
    case SEEK_END:
        base = size;
        break;
    default:
        return AVERROR(EINVAL);
    }
    if (offset < -base || offset > size - base) {
        return AVERROR(EINVAL);
    }
    Reader_Seek(reader, (size_t)(base + offset));
    return Reader_Failed(reader) ? AVERROR(EIO) : base + offset;
}

static void M_Close(AUDIO_DECODER *const decoder)
{
    swr_free(&decoder->resampler);
    av_frame_free(&decoder->frame);
    av_packet_free(&decoder->packet);
    avcodec_free_context(&decoder->codec);
    avformat_close_input(&decoder->format);
    if (decoder->io != nullptr) {
        av_freep(&decoder->io->buffer);
        avio_context_free(&decoder->io);
    }
    Mem_Free(decoder->buffer);
}

static RESULT M_Open(AUDIO_DECODER *const decoder)
{
    decoder->format = avformat_alloc_context();
    uint8_t *const buffer = av_malloc(AUDIO_IO_BUFFER_SIZE);
    if (decoder->format == nullptr || buffer == nullptr) {
        av_free(buffer);
        return FAIL("cannot allocate the audio demuxer");
    }
    decoder->io = avio_alloc_context(
        buffer, AUDIO_IO_BUFFER_SIZE, 0, decoder->reader, M_Read, nullptr,
        M_Seek);
    if (decoder->io == nullptr) {
        av_free(buffer);
        return FAIL("cannot allocate the audio input buffer");
    }
    decoder->format->pb = decoder->io;
    decoder->format->flags |= AVFMT_FLAG_CUSTOM_IO;
    int error =
        avformat_open_input(&decoder->format, nullptr, nullptr, nullptr);
    if (error < 0) {
        return M_Error("cannot open audio", error);
    }
    error = avformat_find_stream_info(decoder->format, nullptr);
    if (error < 0) {
        return M_Error("cannot read audio metadata", error);
    }
    const AVCodec *codec = nullptr;
    decoder->stream = av_find_best_stream(
        decoder->format, AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0);
    if (decoder->stream < 0) {
        return M_Error("cannot find an audio stream", decoder->stream);
    }
    const AVStream *const stream = decoder->format->streams[decoder->stream];
    decoder->codec = avcodec_alloc_context3(codec);
    decoder->packet = av_packet_alloc();
    decoder->frame = av_frame_alloc();
    if (decoder->codec == nullptr || decoder->packet == nullptr
        || decoder->frame == nullptr) {
        return FAIL("cannot allocate the audio decoder");
    }
    error = avcodec_parameters_to_context(decoder->codec, stream->codecpar);
    if (error < 0) {
        return M_Error("cannot configure the audio decoder", error);
    }
    decoder->codec->pkt_timebase = stream->time_base;
    error = avcodec_open2(decoder->codec, codec, nullptr);
    if (error < 0) {
        return M_Error("cannot start the audio decoder", error);
    }
    if (decoder->codec->sample_rate <= 0
        || decoder->codec->ch_layout.nb_channels <= 0) {
        return FAIL("audio has no sample rate or channels");
    }
    decoder->info = (AUDIO_INFO) {
        .sample_rate = (uint32_t)decoder->codec->sample_rate,
        .channels = (uint32_t)decoder->codec->ch_layout.nb_channels,
    };
    const AVCodecDescriptor *const description =
        avcodec_descriptor_get(codec->id);
    const bool pcm =
        description != nullptr && strncmp(description->name, "pcm_", 4) == 0;
    if ((pcm || codec->id == AV_CODEC_ID_FLAC)
        && stream->duration != AV_NOPTS_VALUE && stream->duration >= 0) {
        const int64_t length = av_rescale_q(
            stream->duration, stream->time_base,
            (AVRational) { 1, decoder->codec->sample_rate });
        if (length >= 0) {
            decoder->info.length_known = true;
            decoder->info.length = (uint64_t)length;
        }
    }
    AVChannelLayout stereo = AV_CHANNEL_LAYOUT_STEREO;
    error = swr_alloc_set_opts2(
        &decoder->resampler, &stereo, AV_SAMPLE_FMT_FLT,
        decoder->codec->sample_rate, &decoder->codec->ch_layout,
        decoder->codec->sample_fmt, decoder->codec->sample_rate, 0, nullptr);
    if (error < 0) {
        return M_Error("cannot configure audio conversion", error);
    }
    if (decoder->info.channels == 1) {
        const double matrix[2] = { 1.0, 1.0 };
        error = swr_set_matrix(decoder->resampler, matrix, 1);
        if (error < 0) {
            return M_Error("cannot configure mono audio conversion", error);
        }
    }
    error = swr_init(decoder->resampler);
    return error < 0 ? M_Error("cannot start audio conversion", error) : OK;
}

static RESULT M_Convert(
    AUDIO_DECODER *const decoder, const uint8_t **const input, const int frames,
    const float **const out, uint32_t *const out_frames)
{
    const int capacity = swr_get_out_samples(decoder->resampler, frames);
    if (capacity < 0) {
        return M_Error("cannot size audio conversion", capacity);
    }
    if ((uint32_t)capacity > decoder->capacity) {
        decoder->buffer = Mem_ReallocArr(
            decoder->buffer, (size_t)capacity,
            AUDIO_OUTPUT_CHANNELS * sizeof(float));
        decoder->capacity = (uint32_t)capacity;
    }
    uint8_t *buffer = (uint8_t *)decoder->buffer;
    const int count =
        swr_convert(decoder->resampler, &buffer, capacity, input, frames);
    if (count < 0) {
        return M_Error("cannot convert audio", count);
    }
    *out = decoder->buffer;
    *out_frames = (uint32_t)count;
    return OK;
}

RESULT AudioDecoder_Open(READER *const reader, AUDIO_DECODER **const out)
{
    *out = nullptr;
    FAIL_IF(Reader_Failed(reader), "audio reader has failed");
    FAIL_IF(Reader_Size(reader) > INT64_MAX, "audio input is too large");
    Reader_Seek(reader, 0);
    AUDIO_DECODER *const decoder = Mem_AllocZ(sizeof(*decoder));
    decoder->reader = reader;
    RESULT result = M_Open(decoder);
    if (!IS_OK(result)) {
        M_Close(decoder);
        Mem_Free(decoder);
        return result;
    }
    *out = decoder;
    return OK;
}

void AudioDecoder_Free(AUDIO_DECODER *const decoder)
{
    if (decoder != nullptr) {
        M_Close(decoder);
        Mem_Free(decoder);
    }
}

AUDIO_INFO AudioDecoder_GetInfo(const AUDIO_DECODER *const decoder)
{
    return decoder->info;
}

RESULT AudioDecoder_Read(
    AUDIO_DECODER *const decoder, const float **const out,
    uint32_t *const frames)
{
    *out = nullptr;
    *frames = 0;
    if (decoder->pending_frames != 0) {
        *out =
            decoder->buffer + AUDIO_OUTPUT_CHANNELS * decoder->pending_offset;
        *frames = decoder->pending_frames;
        decoder->pending_frames = 0;
        return OK;
    }
    while (!decoder->finished) {
        int error = avcodec_receive_frame(decoder->codec, decoder->frame);
        if (error == 0) {
            if (decoder->frame->sample_rate != (int)decoder->info.sample_rate
                || decoder->frame->ch_layout.nb_channels
                    != (int)decoder->info.channels) {
                return FAIL("audio format changes during playback");
            }
            RESULT result = M_Convert(
                decoder, (const uint8_t **)decoder->frame->extended_data,
                decoder->frame->nb_samples, out, frames);
            av_frame_unref(decoder->frame);
            if (!IS_OK(result) || *frames != 0) {
                return result;
            }
        } else if (error == AVERROR_EOF) {
            MUST(M_Convert(decoder, nullptr, 0, out, frames));
            decoder->finished = *frames == 0;
            return OK;
        } else if (error != AVERROR(EAGAIN)) {
            return M_Error("cannot decode audio", error);
        } else {
            if (decoder->flushing) {
                return FAIL("audio decoder did not finish after end of input");
            }
            do {
                av_packet_unref(decoder->packet);
                error = av_read_frame(decoder->format, decoder->packet);
            } while (error >= 0
                     && decoder->packet->stream_index != decoder->stream);
            if (error == AVERROR_EOF) {
                if (Reader_Failed(decoder->reader)) {
                    return Reader_Result(decoder->reader);
                }
                decoder->flushing = true;
                error = avcodec_send_packet(decoder->codec, nullptr);
            } else if (error >= 0) {
                error = avcodec_send_packet(decoder->codec, decoder->packet);
            }
            av_packet_unref(decoder->packet);
            if (error < 0) {
                return M_Error("cannot read audio packet", error);
            }
        }
    }
    return OK;
}

RESULT AudioDecoder_Seek(AUDIO_DECODER *const decoder, uint64_t position)
{
    FAIL_IF(
        decoder->info.length_known && position > decoder->info.length,
        "audio position is past the source end");
    const AVStream *const stream = decoder->format->streams[decoder->stream];
    const AVCodecDescriptor *const description =
        avcodec_descriptor_get(decoder->codec->codec_id);
    const bool pcm =
        description != nullptr && strncmp(description->name, "pcm_", 4) == 0;
    if (pcm && strcmp(decoder->format->iformat->name, "wav") == 0
        && stream->time_base.num == 1
        && stream->time_base.den == (int)decoder->info.sample_rate
        && position <= INT64_MAX) {
        const int error = av_seek_frame(
            decoder->format, decoder->stream, (int64_t)position,
            AVSEEK_FLAG_BACKWARD);
        if (error >= 0) {
            avcodec_flush_buffers(decoder->codec);
            swr_close(decoder->resampler);
            const int convert_error = swr_init(decoder->resampler);
            if (convert_error < 0) {
                return M_Error("cannot reset audio conversion", convert_error);
            }
            decoder->pending_frames = 0;
            decoder->flushing = false;
            decoder->finished = false;
            return OK;
        }
    }
    // Decode from the beginning to preserve codec delay and sample trimming.
    READER *const reader = decoder->reader;
    Reader_Seek(reader, 0);
    AUDIO_DECODER *replacement = nullptr;
    MUST(AudioDecoder_Open(reader, &replacement));
    M_Close(decoder);
    *decoder = *replacement;
    Mem_Free(replacement);
    while (position != 0) {
        const float *data;
        uint32_t frames;
        MUST(AudioDecoder_Read(decoder, &data, &frames));
        FAIL_IF(frames == 0, "audio position is past the source end");
        if (position < frames) {
            decoder->pending_offset = (uint32_t)position;
            decoder->pending_frames = frames - (uint32_t)position;
            return OK;
        }
        position -= frames;
    }
    return OK;
}
