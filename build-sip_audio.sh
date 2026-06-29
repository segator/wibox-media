#!/bin/sh
set -eu

docker run --rm \
  -v "$(pwd)":/workspace \
  -w /workspace/src/sip_audio \
  wibox-build:latest \
  sh -lc 'export PATH=/opt/4.6.1/usr/bin:$PATH; make clean && make'

mkdir -p include/bin
cp src/sip_audio/sip_audio include/bin/
