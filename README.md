# dtplayer

dtplayer is an open-source project based on C , aimed to provide multimedia playing service base on ffmpeg(linux version2.0+).

At present, dtplayer have provided examples on stream-demuxer-decoder-render modules. User can easily understand how to add new element.

User can also remove ffmpeg dependence through modifing makefile (set DT_FFMPEG = no), Then you will get an aac player for now.


## Installation

### Linux

- install sdl1.0

- install ffmpeg & libav

- ./build.sh linux

### Android

- install ffmpeg for android (refer to https://github.com/peterfuture/ffmpeg_android)

- export DT_FFMPEG_ANDROID_DIR=/PATH/TO/FFmpeg-Android/build/ffmpeg/armv7

- ./build.sh android


## Usage

Usage: dtplayer [options] <url>

Options:

    -V  , --version                 output program version
    -h  , --help                    output help information
    -dw , --width <n>               specify destiny width
    -dh , --height <n>              specify destiny height
    -na , --disable_audio           disable audio
    -nv , --disable_video           disable video
    -ns , --disable_sub             disable sub
    -ast, --audio_index <n>         specify audio index
    -vst, --video_index <n>         specify video index
    -sst, --sub_index <n>           specify sub index
    -l  , --loop <n>                enable loop
    -nsy, --disable-sync            disable avsync


## Links

Demos build with `dtplayer`:

- [dttv-desktop](https://github.com/peterfuture/dttv-desktop) - pc multimedia player
- [dttv-android](https://github.com/peterfuture/dttv-android) - android multimedia player

## Author

peter_future@outlook.com

# Licence

GPL v3.0

# Build Status

[![Build Status](https://travis-ci.org/peterfuture/dtplayer_c.svg?branch=master)](https://travis-ci.org/peterfuture/dtplayer_c)
