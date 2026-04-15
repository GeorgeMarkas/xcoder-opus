#include "xcoder-opus_internal.h"

int encode_audio_frame(AVFrame *frame, const output_ctx *out,
                       bool *data_present, JNIEnv *env) {
    AVPacket *output_packet = NULL;
    int ret;

    if ((ret = init_packet(&output_packet, env))) return ret;

    // Set a timestamp based on the sample rate of the container
    if (frame) {
        frame->pts = pts;
        pts += frame->nb_samples;
    }

    *data_present = false;

    // Send the audio frame to the decoder
    ret = avcodec_send_frame(out->codec_ctx, frame);
    if (ret < 0 && ret != AVERROR_EOF) {
        fmt_msg_throw(env, "Could not send packet for encoding (error '%s')\n",
                av_err2str(ret));
        goto cleanup;
    }

    // Receive one encoded frame from the encoder
    ret = avcodec_receive_packet(out->codec_ctx, output_packet);
    if (ret == AVERROR(EAGAIN)) {
        ret = 0;
        goto cleanup;
    }

    // If the last frame has been encoded, stop encoding
    if (ret == AVERROR_EOF) {
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

    // Write one audio frame to the output file
    ret = av_write_frame(out->fmt_ctx, output_packet);
    if (*data_present && ret < 0) {
        fmt_msg_throw(env, "Could not write frame (error '%s')\n",
                av_err2str(ret));
    }

cleanup:
    av_packet_free(&output_packet);
    return ret;
}
