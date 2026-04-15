#include "xcoder-opus_internal.h"

int init_resampler(const input_ctx *in, const output_ctx *out,
                   SwrContext **swr, JNIEnv *env) {
    // Set conversion parameters for the resampler
    int ret = swr_alloc_set_opts2(swr,
                                  &out->codec_ctx->ch_layout,
                                  out->codec_ctx->sample_fmt,
                                  out->codec_ctx->sample_rate,
                                  &in->codec_ctx->ch_layout,
                                  in->codec_ctx->sample_fmt,
                                  in->codec_ctx->sample_rate,
                                  0, NULL);
    if (ret < 0) {
        fmt_msg_throw(env, "Failed to allocate resample context (error '%s')\n",
                av_err2str(ret));
        return ret;
    }

    // Initialize the resampler
    if ((ret = swr_init(*swr)) < 0) {
        fmt_msg_throw(env, "Could not open resampler (error '%s')\n",
                av_err2str(ret));
        return ret;
    }

    return 0;
}
