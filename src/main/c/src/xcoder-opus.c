#include "xcoder-opus_internal.h"
#include "../include/xcoder-opus.h"

int64_t pts = 0;

JNIEXPORT jint JNICALL
Java_io_github_georgemarkas_xcoder_XcoderOpus_transcodeToOpus(JNIEnv *env,
        __attribute__((unused)) jobject obj, jstring j_input_file,
        jstring j_output_file, jint j_output_bit_rate) {

    const char *input_file = (*env)->GetStringUTFChars(env, j_input_file, 0);
    const char *output_file = (*env)->GetStringUTFChars(env, j_output_file, 0);
    const int64_t output_bit_rate = j_output_bit_rate;

    input_ctx in = {.fmt_ctx = NULL, .codec_ctx = NULL};
    output_ctx out = {.fmt_ctx = NULL, .codec_ctx = NULL};
    SwrContext *swr = NULL;
    AVAudioFifo *fifo = NULL;
    int ret = AVERROR_EXIT;

    if (open_input_file(input_file, &in, env)) goto cleanup;

    if (open_output_file(output_file, &out, output_bit_rate, env)) goto cleanup;

    if (init_resampler(&in, &out, &swr, env)) goto cleanup;

    if (init_fifo(&fifo, out.codec_ctx, env)) goto cleanup;

    if (write_output_file_header(out.fmt_ctx, env)) goto cleanup;

    // Loop as long as we have input samples to read or output samples to write
    while (true) {
        const int output_frame_size = out.codec_ctx->frame_size;
        bool finished = false;

        // Ensure there is one frame's worth of samples in the FIFO buffer
        // so that the encoder can do its work. Since the decoder's and the
        // encoder's frame size may differ, we need the FIFO buffer to store
        // as many frames' worth of input samples that they make up at least
        // one frame's worth of output samples.
        while (av_audio_fifo_size(fifo) < output_frame_size) {
            if (read_decode_convert_and_store(fifo, &in, out.codec_ctx, swr,
                                              &finished, env))
                goto cleanup;

            // If we are at the end of the input file, we continue encoding
            // the remaining audio samples to the output file.
            if (finished) break;
        }

        // If we have enough samples for the encoder, we encode them.
        // At the end of the file, we pass the remaining samples to the
        // encoder.
        while (av_audio_fifo_size(fifo) >= output_frame_size ||
               (finished && av_audio_fifo_size(fifo) > 0)) {
            if (load_encode_and_write(fifo, &out, env)) goto cleanup;
        }

        // If we're at the end of the input file and have encoded
        // all the remaining samples, we can exit this loop and finish.
        if (finished) {
            bool data_written;

            // Flush the encoder as it may have delayed frames
            do {
                if (encode_audio_frame(NULL, &out, &data_written, env)) goto cleanup;
            } while (data_written);

            break;
        }
    }

    if (write_output_file_trailer(out.fmt_ctx, env)) goto cleanup;
    ret = 0;

cleanup:
    if (fifo) av_audio_fifo_free(fifo);
    swr_free(&swr);
    if (out.codec_ctx) avcodec_free_context(&out.codec_ctx);
    if (out.fmt_ctx) {
        avio_closep(&out.fmt_ctx->pb);
        avformat_free_context(out.fmt_ctx);
    }
    if (in.codec_ctx) avcodec_free_context(&in.codec_ctx);
    if (in.fmt_ctx) avformat_close_input(&in.fmt_ctx);
    (*env)->ReleaseStringUTFChars(env, j_input_file, input_file);
    (*env)->ReleaseStringUTFChars(env, j_output_file, output_file);
    return ret;
}
