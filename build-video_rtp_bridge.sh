#!/bin/sh
set -eu

SDK_DIR="${SDK_DIR:-$HOME/config/GK710X_LinuxSDK_v2.0.0}"

docker run --rm \
  -v "$(pwd)":/work \
  -v "$SDK_DIR":/sdk \
  wibox-build:latest \
  arm-goke-linux-uclibcgnueabi-gcc -static -std=gnu99 -Os \
    -I/work/include/adi \
    -I/sdk/adi/include \
    -I/sdk/install/arm11-gcc-uClibc-linux-GK710XS/include \
    -o /work/src/video_rtp_bridge/video_rtp_bridge \
    /work/src/video_rtp_bridge/video_rtp_bridge.c \
    /sdk/install/arm11-gcc-uClibc-linux-GK710XS/lib/libadi.a \
    -lpthread -lm

mkdir -p include/bin
cp src/video_rtp_bridge/video_rtp_bridge include/bin/
