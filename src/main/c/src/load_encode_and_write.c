#include "xcoder-opus_internal.h"

/**
 * Initialize one input frame for writing to the output file.
 */
static int init_output_frame(AVFrame **frame,
                             const AVCodecContext *output_codec_ctx,
                             int frame_size, JNIEnv *env);

int load_encode_and_write(AVAudioFifo *fifo, const output_ctx *out,
                          JNIEnv *env) {
    AVFrame *output_frame = NULL;

    // We always use the maximum number of possible samples per frame.
    // If there is less than the maximum possible frame size in the FIFO
    // buffer, use this number instead. That being said, Opus expects
    // the total length to be a multiple of the audio frames, so the
    // last frame will most likely need to be padded with silence to
    // make up a full frame, hence we're always allocating space for
    // a full encoder frame.
    const int fifo_size = av_audio_fifo_size(fifo);
    const int real_samples = FFMIN(fifo_size, out->codec_ctx->frame_size);

    bool data_written;

    if (init_output_frame(&output_frame, out->codec_ctx,
                          out->codec_ctx->frame_size, env))
        return AVERROR_EXIT;

    // Pad the last "partial" frame with silence as to make sure it makes
    // up a full audio frame for the reason mentioned above.
    av_samples_set_silence(output_frame->data, 0,
                           out->codec_ctx->frame_size,
                           out->codec_ctx->ch_layout.nb_channels,
                           out->codec_ctx->sample_fmt);

    if (av_audio_fifo_read(fifo, (void **) output_frame->data, real_samples)
        < real_samples) {
        fmt_msg_throw(env, "Could not read data from FIFO\n");
        av_frame_free(&output_frame);
        return AVERROR_EXIT;
    }

    // Encode one frame's worth of samples
    if (encode_audio_frame(output_frame, out, &data_written, env)) {
        av_frame_free(&output_frame);
        return AVERROR_EXIT;
    }

    av_frame_free(&output_frame);

    return 0;
}

static int init_output_frame(AVFrame **frame,
                             const AVCodecContext *output_codec_ctx,
                             const int frame_size, JNIEnv *env) {
    int ret;

    if (!(*frame = av_frame_alloc())) {
        fmt_msg_throw(env, "Failed to allocate output frame\n");
        return AVERROR_EXIT;
    }

    // Set the frame's parameters
    (*frame)->nb_samples = frame_size;
    av_channel_layout_copy(&(*frame)->ch_layout, &output_codec_ctx->ch_layout);
    (*frame)->format = output_codec_ctx->sample_fmt;
    (*frame)->sample_rate = output_codec_ctx->sample_rate;

    // Allocate the samples of the created frame
    if ((ret = av_frame_get_buffer(*frame, 0)) < 0) {
        fmt_msg_throw(
            env, "Failed to allocate output frame samples (error '%s')",
            av_err2str(ret));
        av_frame_free(frame);
        return ret;
    }

    return 0;
}
