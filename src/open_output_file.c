#include "xcoder-opus_internal.h"

int open_output_file(const char *filename, output_ctx *out,
                     const int64_t output_bit_rate) {

    AVIOContext *output_io_ctx = NULL;
    const AVCodec *output_codec = NULL;
    AVStream *stream = NULL;
    int ret;

    // Open the output file for writing
    if ((ret = avio_open(&output_io_ctx, filename, AVIO_FLAG_WRITE)) < 0) {
        fprintf(stderr, "Could not open output file '%s' (error '%s')\n",
                filename, av_err2str(ret));
        return ret;
    }

    // Create the output format context
    ret = avformat_alloc_output_context2(&out->fmt_ctx, NULL, NULL, filename);
    if (ret < 0) {
        fprintf(stderr,
                "Failed to allocate output format context (error '%s')\n",
                av_err2str(ret));
        goto cleanup;
    }

    // Associate the output file with the container format context
    out->fmt_ctx->pb = output_io_ctx;

    // Find a codec for Opus
    if (!(output_codec = avcodec_find_encoder(AV_CODEC_ID_OPUS))) {
        fprintf(stderr, "No suitable Opus encoder found\n");
        ret = AVERROR_ENCODER_NOT_FOUND;
        goto cleanup;
    }

    // Create a new audio stream in the output file container
    if (!(stream = avformat_new_stream(out->fmt_ctx, NULL))) {
        fprintf(stderr, "Failed to create new audio stream\n");
        ret = AVERROR(ENOMEM);
        goto cleanup;
    }

    // Create the encoding context
    if (!(out->codec_ctx = avcodec_alloc_context3(output_codec))) {
        fprintf(stderr, "Failed to allocate encoding context\n");
        ret = AVERROR(ENOMEM);
        goto cleanup;
    }

    // Set encoder parameters
    // TODO: Should probably copy this from the input context
    av_channel_layout_default(&out->codec_ctx->ch_layout, OUTPUT_CHANNELS);
    out->codec_ctx->sample_fmt = output_codec->sample_fmts[0];
    out->codec_ctx->sample_rate = OUTPUT_SAMPLE_RATE;
    out->codec_ctx->bit_rate = output_bit_rate;

    // Set the sample rate for the container
    stream->time_base.num = 1;
    stream->time_base.den = OUTPUT_SAMPLE_RATE;

    // Have the encoder play nice with container formats that
    // require global headers (e.g. MP4).
    if (out->fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        out->codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    // Initialize the codec
    if ((ret = avcodec_open2(out->codec_ctx, output_codec, NULL)) < 0) {
        fprintf(stderr, "Could not open output codec (error '%s')\n",
                av_err2str(ret));
        goto cleanup;
    }

    ret = avcodec_parameters_from_context(stream->codecpar, out->codec_ctx);
    if (ret < 0) {
        fprintf(stderr,
                "Failed to initialize stream parameters (error '%s')\n",
                av_err2str(ret));
        goto cleanup;
    }

    return 0;

cleanup:
    avcodec_free_context(&out->codec_ctx);
    avformat_free_context(out->fmt_ctx);
    avio_closep(&out->fmt_ctx->pb);
    out->fmt_ctx = NULL;

    return ret < 0 ? ret : AVERROR_EXIT;
}
