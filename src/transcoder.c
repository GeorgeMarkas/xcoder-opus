#include "../include/transcoder.h"

#include <stdbool.h>
#include <stdio.h>

#include <libswresample/swresample.h>
#include <libavutil/audio_fifo.h>

/**
 * Open an input file and the required decoder.
 */
static int open_input_file(const char *filename, input_ctx *in) {
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

/**
 * Open an output file and the Opus encoder.
 */
static int open_output_file(const char *filename, output_ctx *out) {
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
    out->codec_ctx->bit_rate = OUTPUT_BIT_RATE;

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

/**
 * Initialize one data packet for reading or writing.
 */
static int init_packet(AVPacket **packet) {
    if (!(*packet = av_packet_alloc())) {
        fprintf(stderr, "Failed to allocate packet");
        return AVERROR(ENOMEM);
    }

    return 0;
}

/**
 * Initialize one audio frame for reading from the input file.
 */
static int init_input_frame(AVFrame **frame) {
    if (!(*frame = av_frame_alloc())) {
        fprintf(stderr, "Failed to allocate input frame\n");
        return AVERROR(ENOMEM);
    }

    return 0;
}

/**
 * Initialize the audio resampler based on the input and output codec settings.
 */
static int init_resampler(const input_ctx *in, const output_ctx *out,
                          SwrContext **swr) {
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
        fprintf(stderr, "Failed to allocate resample context (error '%s')\n",
                av_err2str(ret));
        return ret;
    }

    // Initialize the resampler
    if ((ret = swr_init(*swr)) < 0) {
        fprintf(stderr, "Could not open resampler (error '%s')\n",
                av_err2str(ret));
        return ret;
    }

    return 0;
}

/**
 * Initialize a FIFO buffer for the audio samples to be encoded.
 */
static int init_fifo(AVAudioFifo **fifo,
                     const AVCodecContext *output_codec_ctx) {

    *fifo = av_audio_fifo_alloc(output_codec_ctx->sample_fmt,
                                output_codec_ctx->ch_layout.nb_channels, 1);
    if (!*fifo) {
        fprintf(stderr, "Failed to allocate FIFO\n");
        return AVERROR(ENOMEM);
    }

    return 0;
}

/**
 * Write the header of the output file container.
 */
static int write_output_file_header(AVFormatContext *output_format_ctx) {
    const int ret = avformat_write_header(output_format_ctx, NULL);
    if (ret < 0) {
        fprintf(stderr, "Could not write output file header (error '%s')\n",
                av_err2str(ret));
        return ret;
    }

    return 0;
}

/**
 * Decode one audio frame from the input file.
 */
static int decode_audio_frame(AVFrame *frame, const input_ctx *in,
                              bool *data_present,
                              bool *finished) {

    AVPacket *input_packet = NULL;
    int ret;

    if ((ret = init_packet(&input_packet)) < 0) return ret;

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
                fprintf(stderr, "Could not read frame (error '%s')\n",
                        av_err2str(ret));
                goto cleanup;
            }
            break;
        }
    } while (input_packet->stream_index != in->stream_idx);

    // Send the audio frame stored in the temporary packet to the decoder
    if ((ret = avcodec_send_packet(in->codec_ctx, input_packet)) < 0) {
        fprintf(stderr, "Failed to send packet to the decoder (error '%s')\n",
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
        fprintf(stderr, "Could not decode frame (error '%s')\n",
                av_err2str(ret));
        goto cleanup;
    }

    // Return encoded data
    *data_present = true;

cleanup:
    av_packet_free(&input_packet);

    return ret;
}

/**
 * Initialize a temporary storage for the specified number of audio samples.
 */
static int init_converted_samples(uint8_t ***converted_samples,
                                  const AVCodecContext *output_codec_ctx,
                                  const int frame_size) {
    // Allocate as many pointers as there are audio channels. Each pointer
    // will point to the audio samples of the corresponding channels.
    const int ret = av_samples_alloc_array_and_samples(
        converted_samples, NULL,
        output_codec_ctx->ch_layout.nb_channels,
        frame_size, output_codec_ctx->sample_fmt, 0);
    if (ret < 0) {
        fprintf(stderr, "Failed to allocate converted samples (error '%s')\n",
                av_err2str(ret));
        return ret;
    }

    return 0;
}

/**
 * Convert the input audio samples into the output sample format.
 */
static int convert_samples(const uint8_t **input_data,
                           uint8_t **converted_data,
                           const int frame_size, SwrContext *swr) {

    // Convert the samples using the resampler
    const int ret = swr_convert(swr, converted_data, frame_size, input_data,
                                frame_size);
    if (ret < 0) {
        fprintf(stderr, "Could not convert samples (error '%s')\n",
                av_err2str(ret));
        return ret;
    }

    return 0;
}

/**
 * Add converted audio samples to the FIFO buffer.
 */
static int add_samples_to_fifo(AVAudioFifo *fifo, uint8_t **converted_samples,
                               const int frame_size) {

    // Expand the FIFO buffer as to hold both the old and new samples
    const int ret = av_audio_fifo_realloc(fifo, av_audio_fifo_size(fifo)
                                                + frame_size);
    if (ret < 0) {
        fprintf(stderr, "Failed to reallocate FIFO (error '%s')\n",
                av_err2str(ret));
        return ret;
    }

    // Store the new samples in the FIFO buffer
    if (av_audio_fifo_write(fifo, (void **) converted_samples,
                            frame_size) < frame_size) {
        fprintf(stderr, "Could not write data to FIFO (error '%s')\n",
                av_err2str(ret));
        return AVERROR_EXIT;
    }

    return 0;
}

/**
 * Read one frame from the input file, decode it, convert it and store it.
 */
static int read_decode_convert_and_store(AVAudioFifo *fifo, const input_ctx *in,
                                         const AVCodecContext *output_codec_ctx,
                                         SwrContext *swr, bool *finished) {
    AVFrame *input_frame = NULL;
    uint8_t **converted_samples = NULL;
    bool data_present;
    int ret = AVERROR_EXIT;

    if (init_input_frame(&input_frame)) goto cleanup;

    // Decode one frame's worth of audio samples
    if (decode_audio_frame(input_frame, in, &data_present, finished))
        goto cleanup;

    // Fully flush the resampler. Keep calling swr_convert with NULL
    // until 0 is returned (nothing left to flush).
    if (*finished && !data_present) {
        int flushed;

        do {
            if (init_converted_samples(&converted_samples, output_codec_ctx,
                                       output_codec_ctx->frame_size))
                goto cleanup;

            flushed = swr_convert(swr, converted_samples,
                                  output_codec_ctx->frame_size, NULL, 0);
            if (flushed < 0) {
                ret = flushed;
                fprintf(stderr, "Could not flush resampler (error '%s')\n",
                        av_err2str(ret));
                goto cleanup;
            }

            if (flushed > 0) {
                if (add_samples_to_fifo(fifo, converted_samples, flushed))
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
                                   input_frame->nb_samples))
            goto cleanup;

        if (convert_samples((const uint8_t **) input_frame->extended_data,
                            converted_samples, input_frame->nb_samples, swr))
            goto cleanup;

        if (add_samples_to_fifo(fifo, converted_samples,
                                input_frame->nb_samples))
            goto cleanup;
    }

    ret = 0;

cleanup:
    if (converted_samples) av_freep(&converted_samples[0]);
    av_freep(&converted_samples);
    av_frame_free(&input_frame);

    return ret;
}

/**
 * Initialize one input frame for writing to the output file.
 */
static int init_output_frame(AVFrame **frame,
                             const AVCodecContext *output_codec_ctx,
                             const int frame_size) {
    int ret;

    if (!(*frame = av_frame_alloc())) {
        fprintf(stderr, "Failed to allocate output frame\n");
        return AVERROR_EXIT;
    }

    // Set the frame's parameters
    (*frame)->nb_samples = frame_size;
    av_channel_layout_copy(&(*frame)->ch_layout, &output_codec_ctx->ch_layout);
    (*frame)->format = output_codec_ctx->sample_fmt;
    (*frame)->sample_rate = output_codec_ctx->sample_rate;

    // Allocate the samples of the created frame
    if ((ret = av_frame_get_buffer(*frame, 0)) < 0) {
        fprintf(stderr, "Failed to allocate output frame samples (error '%s')",
                av_err2str(ret));
        av_frame_free(frame);
        return ret;
    }

    return 0;
}

static int64_t pts = 0; // Global timestamp for the audio frames

/**
 * Encode one frame's worth of audio to the output file.
 */
static int encode_audio_frame(AVFrame *frame, const output_ctx *out,
                              bool *data_present) {

    AVPacket *output_packet = NULL;
    int ret;

    if ((ret = init_packet(&output_packet))) return ret;

    // Set a timestamp based on the sample rate of the container
    if (frame) {
        frame->pts = pts;
        pts += frame->nb_samples;
    }

    *data_present = false;

    // Send the audio frame to the decoder
    ret = avcodec_send_frame(out->codec_ctx, frame);
    if (ret < 0 && ret != AVERROR_EOF) {
        fprintf(stderr, "Could not send packet for encoding (error '%s')\n",
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
        fprintf(stderr, "Could not decode frame (error '%s')\n",
                av_err2str(ret));
        goto cleanup;
    }

    // Return encoded data
    *data_present = true;

    // Write one audio frame to the output file
    ret = av_write_frame(out->fmt_ctx, output_packet);
    if (*data_present && ret < 0) {
        fprintf(stderr, "Could not write frame (error '%s')\n",
                av_err2str(ret));
    }

cleanup:
    av_packet_free(&output_packet);
    return ret;
}

/**
 * Load one audio frame from the FIFO buffer, encode and write it to the
 * output file.
 */
static int load_encode_and_write(AVAudioFifo *fifo, const output_ctx *out) {
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
                          out->codec_ctx->frame_size))
        return AVERROR_EXIT;

    // Pad the last "partial" frame with silence as to make sure it makes
    // up a full audio frame for the reason mentioned above.
    av_samples_set_silence(output_frame->data, 0,
                           out->codec_ctx->frame_size,
                           out->codec_ctx->ch_layout.nb_channels,
                           out->codec_ctx->sample_fmt);

    if (av_audio_fifo_read(fifo, (void **) output_frame->data, real_samples)
        < real_samples) {
        fprintf(stderr, "Could not read data from FIFO\n");
        av_frame_free(&output_frame);
        return AVERROR_EXIT;
    }

    // Encode one frame's worth of samples
    if (encode_audio_frame(output_frame, out, &data_written)) {
        av_frame_free(&output_frame);
        return AVERROR_EXIT;
    }

    av_frame_free(&output_frame);

    return 0;
}

/**
 * Write the trailer of the output file container
 */
static int write_output_file_trailer(AVFormatContext *output_format_ctx) {
    const int ret = av_write_trailer(output_format_ctx);
    if (ret < 0) {
        fprintf(stderr, "Could not write output file trailer (error '%s')\n",
                av_err2str(ret));
        return ret;
    }

    return 0;
}

int main(int argc, char *argv[]) {
    input_ctx in = {.fmt_ctx = NULL, .codec_ctx = NULL};
    output_ctx out = {.fmt_ctx = NULL, .codec_ctx = NULL};
    SwrContext *swr = NULL;
    AVAudioFifo *fifo = NULL;
    int ret = AVERROR_EXIT;

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <input file> <output file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    if (open_input_file(argv[1], &in)) goto cleanup;

    if (open_output_file(argv[2], &out)) goto cleanup;

    if (init_resampler(&in, &out, &swr)) goto cleanup;

    if (init_fifo(&fifo, out.codec_ctx)) goto cleanup;

    if (write_output_file_header(out.fmt_ctx)) goto cleanup;

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
                                              &finished))
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
            if (load_encode_and_write(fifo, &out)) goto cleanup;
        }

        // If we're at the end of the input file and have encoded
        // all the remaining samples, we can exit this loop and finish.
        if (finished) {
            bool data_written;

            // Flush the encoder as it may have delayed frames
            do {
                if (encode_audio_frame(NULL, &out, &data_written)) goto cleanup;
            } while (data_written);

            break;
        }
    }

    if (write_output_file_trailer(out.fmt_ctx)) goto cleanup;
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
    return ret;
}
