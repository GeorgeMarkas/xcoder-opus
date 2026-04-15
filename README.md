# xcoder-opus
This is a single purpose, audio transcoder, outputting Opus encapsulated in compatible container formats.
It uses [libav](https://trac.ffmpeg.org/wiki/Using%20libav*) and is adapted from
[this example](https://github.com/FFmpeg/FFmpeg/blob/master/doc/examples/transcode_aac.c).

## Compatability
- This project has been tested on JDK 21, though it most likely works with older versions too.
- Only Linux x86-64 is supported.
- The following libraries are required (since the native code dynamically links to them)[^1].
    - libavcodec
    - libavformat
    - libavutil
    - libswresample

[^1]: This project was compiled linking against libav 6.1.1, your mileage may vary
depending on what versions of above dependencies you use.

## Install
Add the following to your `pom.xml`:
```xml
<dependency>
    <groupId>io.github.georgemarkas</groupId>
    <artifactId>xcoder-opus</artifactId>
    <version>1.0.0</version>
</dependency>
```

## Usage
```java
import io.github.georgemarkas.xcoder.XcoderOpus;

public class Main {
    private static final String inputFile = "/path/to/input/file.wav";
    private static final String outputFile = "/path/to/output/file.ogg";
    private static final int outputBitRate = 128000;
    
    public static void main(String[] args) {
        XcoderOpus.transcodeToOpus(inputFile, outputFile, outputBitRate);
    }
}
```

### NOTICE
This project uses libav (libavformat, libavcodec, libavutil, libswresample),
which are licensed under the GNU Lesser General Public License v2.1 or later.
See https://ffmpeg.org/legal.html for more information.