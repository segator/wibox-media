#!/bin/sh
set -eu

SDK_DIR="${SDK_DIR:-$(pwd)/third_party/gk710x-sdk-min}"
BUILD_IMAGE="${BUILD_IMAGE:-wibox-build-tool:latest}"

docker run --rm \
  -v "$(pwd)":/workspace \
  -v "$SDK_DIR":/sdk \
  -w /workspace/src/sip_media \
  "$BUILD_IMAGE" \
  sh -lc 'export PATH=/opt/4.6.1/usr/bin:$PATH; make clean PROGRAM=wibox-media-daemon && make PROGRAM=wibox-media-daemon'

mkdir -p include/lib
cp "$SDK_DIR/install/arm11-gcc-uClibc-linux-GK710XS/lib/libap.so" include/lib/

mkdir -p include/bin
cp src/sip_media/wibox-media-daemon include/bin/
