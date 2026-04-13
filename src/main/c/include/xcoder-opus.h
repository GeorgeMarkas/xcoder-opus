#pragma once

#include <jni.h>

JNIEXPORT jint JNICALL
Java_io_github_georgemarkas_xcoder_XcoderOpus_transcodeToOpus(JNIEnv *env,
        __attribute__((unused)) jobject obj, jstring j_input_file,
        jstring j_output_file, jint j_output_bit_rate);
