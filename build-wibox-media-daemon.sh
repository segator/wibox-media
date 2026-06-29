#!/bin/sh
set -eu

SDK_DIR="${SDK_DIR:-$HOME/config/GK710X_LinuxSDK_v2.0.0}"

docker run --rm \
  -v "$(pwd)":/workspace \
  -v "$SDK_DIR":/sdk \
  -w /workspace/src/sip_media \
  wibox-build:latest \
  sh -lc 'export PATH=/opt/4.6.1/usr/bin:$PATH; make clean PROGRAM=wibox-media-daemon && make PROGRAM=wibox-media-daemon'

mkdir -p include/lib
cp "$SDK_DIR/install/arm11-gcc-uClibc-linux-GK710XS/lib/libap.so" include/lib/

mkdir -p include/bin
cp src/sip_media/wibox-media-daemon include/bin/
ln -sf wibox-media-daemon include/bin/sip_media
