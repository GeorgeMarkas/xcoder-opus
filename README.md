# xcoder-opus
This is an audio transcoder, outputting Opus using the OGG container.
It uses libav and is adapted
from [this](https://github.com/FFmpeg/FFmpeg/blob/master/doc/examples/transcode_aac.c)
example provided by FFmpeg.

## Usage
The provided Makefile generates a `build` directory containing
`libxcoder-opus.a` which you can link against. Whatever program
consumes the static library will also need to link against its libav
dependencies since the generated archive does not embed said
dependencies. An example for this is provided [here](example)
and can be built quite easily like so:

```sh
git clone https://github.com/George-Markas/xcoder-opus.git
cd example
make
```

### NOTICES
This project uses libav (libavformat, libavcodec, libavutil, libswresample),
which are licensed under the GNU Lesser General Public License v2.1 or later.
See https://ffmpeg.org/legal.html for more information.