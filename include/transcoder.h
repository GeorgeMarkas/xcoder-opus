#pragma once

#include "../include/transcoder.h"

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>

#define OUTPUT_BIT_RATE 256000    // 256 kbit/s
#define OUTPUT_SAMPLE_RATE 48000  // 48000 kHz
#define OUTPUT_CHANNELS 2

typedef struct Input_Context {
    AVFormatContext *fmt_ctx;
    AVCodecContext *codec_ctx;
} input_ctx;

typedef struct Output_Context {
    AVFormatContext *fmt_ctx;
    AVCodecContext *codec_ctx;
} output_ctx;