#include "xcoder-opus_internal.h"

int init_fifo(AVAudioFifo **fifo, const AVCodecContext *output_codec_ctx,
              JNIEnv *env) {
    *fifo = av_audio_fifo_alloc(output_codec_ctx->sample_fmt,
                                output_codec_ctx->ch_layout.nb_channels, 1);
    if (!*fifo) {
        fmt_msg_throw(env, "Failed to allocate FIFO\n");
        return AVERROR(ENOMEM);
    }

    return 0;
}
