package io.github.georgemarkas.xcoder;

/**
 * Signals that the transcoding failed.
 */
public class TranscodingFailedException extends RuntimeException {
    public TranscodingFailedException(String message) {
        super(message);
    }
}
