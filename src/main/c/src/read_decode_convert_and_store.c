#include "xcoder-opus_internal.h"

/**
 * Initialize one audio frame for reading from the input file.
 */
static int init_input_frame(AVFrame **, JNIEnv *env);

/**
 * Decode one audio frame from the input file.
 */
static int decode_audio_frame(AVFrame *frame, const input_ctx *in,
                              bool *data_present, bool *finished, JNIEnv *env);

/**
 * Initialize a temporary storage for the specified number of audio samples.
 */
static int init_converted_samples(uint8_t ***converted_samples,
                                  const AVCodecContext *output_codec_ctx,
                                  int frame_size, JNIEnv *env);

/**
 * Add converted audio samples to the FIFO buffer.
 */
static int add_samples_to_fifo(AVAudioFifo *fifo, uint8_t **converted_samples,
                               int frame_size, JNIEnv *env);

/**
 * Convert the input audio samples into the output sample format.
 */
static int convert_samples(const uint8_t **input_data, uint8_t **converted_data,
                           int frame_size, SwrContext *swr, JNIEnv *env);

int read_decode_convert_and_store(AVAudioFifo *fifo, const input_ctx *in,
                                  const AVCodecContext *output_codec_ctx,
                                  SwrContext *swr, bool *finished,
                                  JNIEnv *env) {
    AVFrame *input_frame = NULL;
    uint8_t **converted_samples = NULL;
    bool data_present;
    int ret = AVERROR_EXIT;

    if (init_input_frame(&input_frame, env)) goto cleanup;

    // Decode one frame's worth of audio samples
    if (decode_audio_frame(input_frame, in, &data_present, finished, env))
        goto cleanup;

    // Fully flush the resampler. Keep calling swr_convert with NULL
    // until 0 is returned (nothing left to flush).
    if (*finished && !data_present) {
        int flushed;

        do {
            if (init_converted_samples(&converted_samples, output_codec_ctx,
                                       output_codec_ctx->frame_size, env))
                goto cleanup;

            flushed = swr_convert(swr, converted_samples,
                                  output_codec_ctx->frame_size, NULL, 0);
            if (flushed < 0) {
                ret = flushed;
                fmt_msg_throw(env, "Could not flush resampler (error '%s')\n",
                                av_err2str(ret));
                goto cleanup;
            }

            if (flushed > 0) {
                if (add_samples_to_fifo(fifo, converted_samples, flushed, env))
                    goto cleanup;
            }

            av_freep(&converted_samples[0]);
            av_freep(&converted_samples);
        } while (flushed > 0);

        ret = 0;
        goto cleanup;
    }

    // If there is decoded data, convert it and store it
    if (data_present) {
        if (init_converted_samples(&converted_samples, output_codec_ctx,
                                   input_frame->nb_samples, env))
            goto cleanup;

        if (convert_samples((const uint8_t **) input_frame->extended_data,
                            converted_samples, input_frame->nb_samples, swr,
                            env))
            goto cleanup;

        if (add_samples_to_fifo(fifo, converted_samples,
                                input_frame->nb_samples, env))
            goto cleanup;
    }

    ret = 0;

cleanup:
    if (converted_samples) av_freep(&converted_samples[0]);
    av_freep(&converted_samples);
    av_frame_free(&input_frame);

    return ret;
}

static int init_input_frame(AVFrame **frame, JNIEnv *env) {
    if (!(*frame = av_frame_alloc())) {
        fmt_msg_throw(env, "Failed to allocate input frame\n");
        return AVERROR(ENOMEM);
    }

    return 0;
}

static int decode_audio_frame(AVFrame *frame, const input_ctx *in,
                              bool *data_present, bool *finished, JNIEnv *env) {

    AVPacket *input_packet = NULL;
    int ret;

    if ((ret = init_packet(&input_packet, env)) < 0) return ret;

    *data_present = false;
    *finished = false;

    // The do-while loop is to skip packets that belong to a
    // video stream, in case there is embedded cover art present.
    // The unref is also needed to clean up after skipped packets.
    do {
        av_packet_unref(input_packet);

        if ((ret = av_read_frame(in->fmt_ctx, input_packet)) < 0) {
            // If end of file is reached, flush the decoder below
            if (ret == AVERROR_EOF) {
                *finished = true;
            } else {
                fmt_msg_throw(env, "Could not read frame (error '%s')\n",
                                av_err2str(ret));
                goto cleanup;
            }
            break;
        }
    } while (input_packet->stream_index != in->stream_idx);

    // Send the audio frame stored in the temporary packet to the decoder
    if ((ret = avcodec_send_packet(in->codec_ctx, input_packet)) < 0) {
        fmt_msg_throw(
            env, "Failed to send packet to the decoder (error '%s')\n",
            av_err2str(ret));
        goto cleanup;
    }

    // Receive one frame from the decoder
    ret = avcodec_receive_frame(in->codec_ctx, frame);

    // If the decoder asks for more data to be able to decode a frame,
    // return indicating there is no data present.
    if (ret == AVERROR(EAGAIN)) {
        ret = 0;
        goto cleanup;
    }

    // If the end of the input file is reached, stop decoding
    if (ret == AVERROR_EOF) {
        *finished = true;
        ret = 0;
        goto cleanup;
    }

    if (ret < 0) {
        fmt_msg_throw(env, "Could not decode frame (error '%s')\n",
                        av_err2str(ret));
        goto cleanup;
    }

    // Return encoded data
    *data_present = true;

cleanup:
    av_packet_free(&input_packet);

    return ret;
}

static int init_converted_samples(uint8_t ***converted_samples,
                                  const AVCodecContext *output_codec_ctx,
                                  const int frame_size, JNIEnv *env) {
    // Allocate as many pointers as there are audio channels. Each pointer
    // will point to the audio samples of the corresponding channels.
    const int ret = av_samples_alloc_array_and_samples(
        converted_samples, NULL,
        output_codec_ctx->ch_layout.nb_channels,
        frame_size, output_codec_ctx->sample_fmt, 0);
    if (ret < 0) {
        fmt_msg_throw(
            env, "Failed to allocate converted samples (error '%s')\n",
            av_err2str(ret));
        return ret;
    }

    return 0;
}

static int add_samples_to_fifo(AVAudioFifo *fifo, uint8_t **converted_samples,
                               const int frame_size, JNIEnv *env) {

    // Expand the FIFO buffer as to hold both the old and new samples
    const int ret = av_audio_fifo_realloc(fifo, av_audio_fifo_size(fifo)
                                                + frame_size);
    if (ret < 0) {
        fmt_msg_throw(env, "Failed to reallocate FIFO (error '%s')\n",
                        av_err2str(ret));
        return ret;
    }

    // Store the new samples in the FIFO buffer
    if (av_audio_fifo_write(fifo, (void **) converted_samples,
                            frame_size) < frame_size) {
        fmt_msg_throw(env, "Could not write data to FIFO (error '%s')\n",
                        av_err2str(ret));
        return AVERROR_EXIT;
    }

    return 0;
}

static int convert_samples(const uint8_t **input_data,
                           uint8_t **converted_data,
                           const int frame_size, SwrContext *swr,
                           JNIEnv *env) {

    // Convert the samples using the resampler
    const int ret = swr_convert(swr, converted_data, frame_size, input_data,
                                frame_size);
    if (ret < 0) {
        fmt_msg_throw(env, "Could not convert samples (error '%s')\n",
                        av_err2str(ret));
        return ret;
    }

    return 0;
}
