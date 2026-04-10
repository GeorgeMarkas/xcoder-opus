#pragma once

#include <stdint.h>

/**
 * Transcode an audio file to OGG Opus.
 * @param input_file  The audio file to transcode.
 * @param output_file The name of the output file. This does not include
 * the file extension, that will always be '.ogg' since we're using the
 * OGG container.
 * @param output_bit_rate The output bit rate.
 * @return Zero on success, a negative error code on failure.
 */
int transcode(const char *input_file, const char *output_file,
              int64_t output_bit_rate);
