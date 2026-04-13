# xcoder-opus
This is a single purpose, audio transcoder, outputting Opus encapsulated in compatible container formats.
It uses [libav](https://trac.ffmpeg.org/wiki/Using%20libav*) and is adapted from
[this example](https://github.com/FFmpeg/FFmpeg/blob/master/doc/examples/transcode_aac.c).

## Compatability
- This project has been tested on JDK 21, though it most likely works with older versions too.
- Only Linux x86-64 is supported for the time being.
- The following libraries are required[^1].
    - libavcodec
    - libavformat
    - libavutil
    - libswresample

[^1]: The project was compiled on Ubuntu 24.04 so your mileage may vary based on available package versions.

## Install
Add the following to your `pom.xml`:
```xml
<distributionManagement>
    <repository>
        <id>github</id>
        <name>GitHub Packages</name>
        <url>https://maven.pkg.github.com/George-Markas/xcoder-opus</url>
    </repository>
</distributionManagement>

<dependencies>
    <dependency>
        <groupId>io.github.georgemarkas</groupId>
        <artifactId>xcoder-opus</artifactId>
        <version>0.0.3</version>
    </dependency>
</dependencies>
```

> [!IMPORTANT]
> Due to GitHub Packages not supporting unauthenticated reads, even for public repositories,
> you will need to set your credentials in `~/.m2/settings.xml` like so:
> ```xml
> <settings xmlns="http://maven.apache.org/SETTINGS/1.0.0">
>     <servers>
>         <server>
>             <id>github</id>
>             <username>YOUR_USERNAME</username>
>             <password>YOUR_PERSONAL_ACCESS_TOKEN_WITH_read:packages_SCOPE</password>
>         </server>
>     </servers>
> </settings>
> ```

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