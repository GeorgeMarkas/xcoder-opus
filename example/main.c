/*
 * Demo CLI program to showcase the transcoder
 */

#include "../include/xcoder-opus.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libavutil/error.h>

#define OUTPUT_BIT_RATE 256000 // 256 kbit/s

int main(int argc, char *argv[]) {
    const size_t argv2_len = strlen(argv[2]);
    char *output_file = malloc(argv2_len + 5); // ".ogg"
    snprintf(output_file, argv2_len + 5, "%s.ogg", argv[2]);

    if (argc != 3) {
        fprintf(stderr,
                "Usage: xcoder-opus <input>.<extension> <output>.ogg\n");
        exit(EXIT_FAILURE);
    }

    const int ret = transcode(argv[1], output_file, OUTPUT_BIT_RATE);
    if (ret < 0) {
        fprintf(stderr, "\x1B[1;31mTranscoding failed (error '%s')\x1B[0m\n",
                av_err2str(ret));
        goto skip_success_msg;
    }

    printf("\x1B[1;32mDONE\x1B[0m -> '%s'\n", output_file);

skip_success_msg:
    free(output_file);
    output_file = NULL;
    return ret;
}
