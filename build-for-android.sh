#!/bin/bash

patch -p1 <ffmpeg-dtp-android.patch
make -f makefile-android
