#include "xcoder-opus_internal.h"

int write_output_file_trailer(AVFormatContext *output_format_ctx, JNIEnv *env) {
    const int ret = av_write_trailer(output_format_ctx);
    if (ret < 0) {
        fmt_msg_throw(env, "Could not write output file trailer (error '%s')\n",
                av_err2str(ret));
        return ret;
    }

    return 0;
}