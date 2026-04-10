#include "xcoder-opus_internal.h"

int open_input_file(const char *filename, input_ctx *in) {
    const AVCodec *input_codec = NULL;
    const AVStream *stream = NULL;
    int ret;

    // Open input file for reading
    if ((ret = avformat_open_input(&in->fmt_ctx, filename, NULL, NULL)) < 0) {
        fprintf(stderr, "Could not open input file '%s' (error '%s')\n",
                filename, av_err2str(ret));
        return ret;
    }

    // Get information on the input file
    if ((ret = avformat_find_stream_info(in->fmt_ctx, NULL)) < 0) {
        fprintf(stderr, "Could not get stream info (error '%s')\n",
                av_err2str(ret));
        goto cleanup;
    }

    // Get the correct stream index
    in->stream_idx = av_find_best_stream(in->fmt_ctx, AVMEDIA_TYPE_AUDIO,
                                         -1, -1, &input_codec, 0);
    if (in->stream_idx < 0) {
        fprintf(stderr, "Could not find stream (error '%s')\n",
                av_err2str(in->stream_idx));
        goto cleanup;
    }

    stream = in->fmt_ctx->streams[in->stream_idx];

    // Find the corresponding codec for the stream
    if (!(input_codec = avcodec_find_decoder(stream->codecpar->codec_id))) {
        fprintf(stderr, "No suitable decoder found for '%s'\n",
                avcodec_get_name(stream->codecpar->codec_id));
        ret = AVERROR_DECODER_NOT_FOUND;
        goto cleanup;
    }

    // Create the decoding context
    if (!(in->codec_ctx = avcodec_alloc_context3(input_codec))) {
        fprintf(stderr, "Failed to allocate decoding context\n");
        ret = AVERROR(ENOMEM);
        goto cleanup;
    }

    // Pass the stream info to the codec
    ret = avcodec_parameters_to_context(in->codec_ctx, stream->codecpar);
    if (ret < 0) {
        fprintf(stderr,
                "Failed to copy codec parameters to codec (error '%s')\n",
                av_err2str(ret));
        goto cleanup;
    }

    // Initialize the codec
    if ((ret = avcodec_open2(in->codec_ctx, input_codec, NULL))) {
        fprintf(stderr, "Could not open input codec (error '%s')\n",
                av_err2str(ret));
        goto cleanup;
    }

    // Set the packet timebase for the decoder
    in->codec_ctx->pkt_timebase = stream->time_base;

    return 0;

cleanup:
    avcodec_free_context(&in->codec_ctx);
    avformat_close_input(&in->fmt_ctx);

    return ret < 0 ? ret : AVERROR_EXIT;
}