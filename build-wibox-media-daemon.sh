#!/bin/sh
set -eu

docker run --rm \
  -v "$(pwd)":/workspace \
  -w /workspace/src/sip_media \
  wibox-build:latest \
  sh -lc 'export PATH=/opt/4.6.1/usr/bin:$PATH; make clean PROGRAM=wibox-media-daemon && make PROGRAM=wibox-media-daemon'

mkdir -p include/bin
cp src/sip_media/wibox-media-daemon include/bin/
ln -sf wibox-media-daemon include/bin/sip_media
