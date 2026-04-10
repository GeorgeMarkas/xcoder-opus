#pragma once

#include <stdbool.h>

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/audio_fifo.h>
#include <libswresample/swresample.h>

#define OUTPUT_SAMPLE_RATE 48000  // 48000 kHz
#define OUTPUT_CHANNELS 2

typedef struct Input_Context {
    AVFormatContext *fmt_ctx;
    AVCodecContext *codec_ctx;
    int stream_idx;
} input_ctx;

typedef struct Output_Context {
    AVFormatContext *fmt_ctx;
    AVCodecContext *codec_ctx;
} output_ctx;

/**
 * Global timestamp for the audio frames
 */
extern int64_t pts;

/**
 * Open an input file and the required decoder.
 */
int open_input_file(const char *filename, input_ctx *in);

/**
 * Open an output file and the Opus encoder.
 */
int open_output_file(const char *filename, output_ctx *out, int64_t output_bit_rate);

/**
 * Initialize the audio resampler based on the input and output codec settings.
 */
int init_resampler(const input_ctx *in, const output_ctx *out,
                   SwrContext **swr);

/**
 * Initialize a FIFO buffer for the audio samples to be encoded.
 */
int init_fifo(AVAudioFifo **fifo, const AVCodecContext *output_codec_ctx);


/**
 * Write the header of the output file container.
 */
int write_output_file_header(AVFormatContext *output_format_ctx);

/**
 * Initialize one data packet for reading or writing.
 */
int init_packet(AVPacket **packet);

/**
 * Read one frame from the input file, decode it, convert it and store it.
 */
int read_decode_convert_and_store(AVAudioFifo *fifo, const input_ctx *in,
                                  const AVCodecContext *output_codec_ctx,
                                  SwrContext *swr, bool *finished);

/**
 * Load one audio frame from the FIFO buffer, encode and write it to the
 * output file.
 */
int load_encode_and_write(AVAudioFifo *fifo, const output_ctx *out);

/**
 * Encode one frame's worth of audio to the output file.
 */
int encode_audio_frame(AVFrame *frame, const output_ctx *out,
                       bool *data_present);

/**
 * Write the trailer of the output file container
 */
int write_output_file_trailer(AVFormatContext *output_format_ctx);
