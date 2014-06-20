#!/bin/bash
make clean
#git reset --hard
patch -p1 <ffmpeg-dtp-android.patch
make -f makefile-android
