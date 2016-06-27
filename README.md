
# dtplayer 
[![Build Status](https://travis-ci.org/peterfuture/dtplayer_c.svg?branch=master)](https://travis-ci.org/peterfuture/dtplayer_c)

dtplayer is an open-source project based on C , aimed to provide multimedia playing service base on ffmpeg(linux version2.0+).

At present, dtplayer have provided examples on stream-demuxer-decoder-render modules. User can easily understand how to add new element.

User can also remove ffmpeg dependence through modifing makefile (set DT_FFMPEG = no), Then you will get an aac player for now. 

For more [details](http://blog.csdn.net/dtplayer).


## Installation
### Linux
* install sdl1.0
* install [ffmpeg](https://github.com/FFmpeg/FFmpeg)
* make -j8

### Android
* [Install ToolChain](https://github.com/peterfuture/dttv-android/wiki/1-%E5%AE%89%E8%A3%85android-arm%E4%BA%A4%E5%8F%89%E7%BC%96%E8%AF%91%E9%93%BE)
* Install [ffmpeg-android](https://github.com/peterfuture/ffmpeg_android)
* make -f makefile-android -j8

## Demos
* [dttv-desktop](https://github.com/peterfuture/dttv-desktop) - pc multimedia player
* [dttv-android](https://github.com/peterfuture/dttv-android) - android multimedia player

## Author
peter_future@outlook.com

# Licence
GPL v3.0
