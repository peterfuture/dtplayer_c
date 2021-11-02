
# dtplayer 
dtplayer is an open-source project based on C , aimed to provide multimedia playing service base on ffmpeg(linux version2.0+).

At present, dtplayer have provided examples on stream-demuxer-decoder-render modules. User can easily understand how to add new element.

User can also remove ffmpeg dependence through modifing makefile (set DT_FFMPEG = no), Then you will get an aac player for now. 

For more [details](http://blog.csdn.net/dtplayer).


## Installation
### Mac
* install sdl2.0
* install [ffmpeg](https://github.com/FFmpeg/FFmpeg)
* make -f makefile-mac

### Linux
* install sdl2.0
* install [ffmpeg](https://github.com/FFmpeg/FFmpeg)
* make -j8

### Android
* [Install ToolChain]
* Install [ffmpeg-android]
* ./build.sh android

## Demos
* [dttv-desktop] - pc multimedia player
* [dttv-android]- android multimedia player

# Licence
GPL v3.0
