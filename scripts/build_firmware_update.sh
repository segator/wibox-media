#!/bin/sh
set -eu

SDK_DIR="${SDK_DIR:-$(pwd)/third_party/gk710x-sdk-min}"
BUILD_IMAGE="${BUILD_IMAGE:-wibox-build-tool:latest}"

docker run --rm \
  -v "$(pwd)":/workspace \
  -v "$SDK_DIR":/sdk \
  -w /workspace \
  "$BUILD_IMAGE" \
  sh -lc '
    export PATH=/opt/4.6.1/usr/bin:$PATH
    arm-goke-linux-uclibcgnueabi-gcc \
      -O2 -g -std=gnu99 -D_GNU_SOURCE -Wno-deprecated-declarations \
      -o /workspace/include/bin/firmware_update /workspace/src/firmware_update.c \
      /workspace/cramfs/lib/libssl.so.1.1 /workspace/cramfs/lib/libcrypto.so.1.1 \
      -ldl -lpthread -lm
    chmod +x /workspace/include/bin/firmware_update
  '
