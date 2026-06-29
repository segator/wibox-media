#!/bin/sh
set -eu

docker run --rm \
  -v "$(pwd)":/workspace \
  -w /workspace/src/audio_bridge \
  wibox-build:latest \
  sh -lc 'export PATH=/opt/4.6.1/usr/bin:$PATH; make clean && make'

mkdir -p include/bin include/lib
cp src/audio_bridge/audio_bridge include/bin/
cp src/audio_bridge/libadi.so include/lib/ 2>/dev/null || true
cp src/audio_bridge/libap.so include/lib/ 2>/dev/null || true
