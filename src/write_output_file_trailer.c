#include "xcoder-opus_internal.h"

int write_output_file_trailer(AVFormatContext *output_format_ctx) {
    const int ret = av_write_trailer(output_format_ctx);
    if (ret < 0) {
        fprintf(stderr, "Could not write output file trailer (error '%s')\n",
                av_err2str(ret));
        return ret;
    }

    return 0;
}