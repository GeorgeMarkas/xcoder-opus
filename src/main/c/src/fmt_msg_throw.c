#include "xcoder-opus_internal.h"

#define EXCEPTION_CLASS "io/github/georgemarkas/xcoder/TranscodingFailedException"

static void throw_exception(JNIEnv *env, const char* exc_msg);

void fmt_msg_throw(JNIEnv *env, const char *error_msg, ...) {
    va_list args;

    va_start(args, error_msg);
    const int len = vsnprintf(NULL, 0, error_msg, args);
    va_end(args);

    if (len < 0) {
        const char *fallback_msg = "Output error; vsnprintf() returned negative\n";
        throw_exception(env, fallback_msg);
        return;
    }

    char *exc_msg = malloc(len + 1);
    if (!exc_msg) {
        fprintf(stderr, "Failed to allocate exception message buffer\n");
        return;
    }

    va_start(args, error_msg);
    vsnprintf(exc_msg, len + 1, error_msg, args);
    va_end(args);

    throw_exception(env, exc_msg);

    free(exc_msg);
}

static void throw_exception(JNIEnv *env, const char* exc_msg) {
    (*env)->ExceptionClear(env);
    const jclass exc_class = (*env)->FindClass(env, EXCEPTION_CLASS);
    if (!exc_class) {
        fprintf(stderr,
                "Could not find exception class '%s'\n", EXCEPTION_CLASS);
        return;
    }

    (*env)->ThrowNew(env, exc_class, exc_msg);
}
