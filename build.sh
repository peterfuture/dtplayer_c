#!/bin/bash

usage()
{
    echo "usage: `basename $0` linux | android | all"
}

OPT=$1

if [ $# -ne 1 ]; then
    usage
    exit 1
fi

DEST=`pwd`/build/

make clean
if [ -d build ]; then
rm -rf build
fi

mkdir -p build
mkdir -p build/android

case $OPT in
     linux|Linux) echo "build for Linux platform.."
         make -f  makefile
         mkdir -p build/linux
         cp *.so  build/linux
         #cp *.a   build/linux
         #cp *.exe build/linux
         ;;
     android|Android) echo "build for Android platform.."
         make -f  makefile-android-armeabi-v7a
         mkdir -p build/android/armeabi-v7a
         cp *.so  build/android/armeabi-v7a
         #cp *.a   build/android/armeabi-v7a
         #cp *.exe build/android/armeabi-v7a
 
         make clean
         make -f  makefile-android-armeabi-v8a
         mkdir -p build/android/arm64-v8a
         cp *.so  build/android/arm64-v8a
         #cp *.a   build/android/arm64-v8a
         #cp *.exe build/android/arm64-v8a
         ;;
     all|All) echo "build for Linux & Android platform.."
         make -f  makefile
         mkdir -p build/linux
         cp *.so  build/linux
         #cp *.a   build/linux
         #cp *.exe build/linux
         
         make clean
 
         make -f  makefile-android-armeabi-v7a
         mkdir -p build/android/armeabi-v7a
         cp *.so  build/android/armeabi-v7a
         #cp *.a   build/android/armeabi-v7a
         #cp *.exe build/android/armeabi-v7a
 
         make clean
         make -f  makefile-android-armeabi-v8a
         mkdir -p build/android/arm64-v8a
         cp *.so  build/android/arm64-v8a
         #cp *.a   build/android/arm64-v8a
         #cp *.exe build/android/arm64-v8a
         ;;
     *)usage
         ;;
 esac
