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

case $OPT in
     linux|Linux) echo "build for Linux platform.."
         make -f  makefile
         mkdir -p build/linux
         cp *.so  build/linux
         cp *.a   build/linux
         cp *.exe build/linux
         ;;
     android|Android) echo "build for Android platform.."
         make -f  makefile-android
         mkdir -p build/android
         cp *.so  build/android
         cp *.a   build/android
         cp *.exe build/android
         ;;
     all|All) echo "build for Linux & Android platform.."
        make -f  makefile
         mkdir -p build/linux
         cp *.so  build/linux
         cp *.a   build/linux
         cp *.exe build/linux
         
         make clean
         
         make -f  makefile-android
         mkdir -p build/android
         cp *.so  build/android
         cp *.a   build/android
         cp *.exe build/android
         ;;
     *)usage
         ;;
 esac
