#include "xcoder-opus_internal.h"

int init_packet(AVPacket **packet) {
    if (!(*packet = av_packet_alloc())) {
        fprintf(stderr, "Failed to allocate packet");
        return AVERROR(ENOMEM);
    }

    return 0;
}