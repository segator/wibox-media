#!/bin/sh
set -eu

SDK_DIR="${SDK_DIR:-$(pwd)/third_party/gk710x-sdk-min}"
BUILD_IMAGE="${BUILD_IMAGE:-wibox-build-tool:latest}"
WIBOX_VERSION="${WIBOX_VERSION:-dev-$(date -u +%Y%m%d%H%M%S)}"
WIBOX_COMMIT="${WIBOX_COMMIT:-$(git rev-parse --short=12 HEAD 2>/dev/null || echo unknown)}"
WIBOX_BUILD_TIMESTAMP="${WIBOX_BUILD_TIMESTAMP:-$(date -u +%Y-%m-%dT%H:%M:%SZ)}"

docker run --rm \
  -v "$(pwd)":/workspace \
  -v "$SDK_DIR":/sdk \
  -w /workspace/src/sip_media \
  -e WIBOX_VERSION="$WIBOX_VERSION" \
  -e WIBOX_COMMIT="$WIBOX_COMMIT" \
  -e WIBOX_BUILD_TIMESTAMP="$WIBOX_BUILD_TIMESTAMP" \
  "$BUILD_IMAGE" \
  sh -lc 'export PATH=/opt/4.6.1/usr/bin:$PATH; make clean PROGRAM=wibox-media-daemon && make PROGRAM=wibox-media-daemon WIBOX_VERSION="$WIBOX_VERSION" WIBOX_COMMIT="$WIBOX_COMMIT" WIBOX_BUILD_TIMESTAMP="$WIBOX_BUILD_TIMESTAMP"'

BUILD_IMAGE="$BUILD_IMAGE" scripts/build_firmware_update.sh

mkdir -p include/lib
cp "$SDK_DIR/install/arm11-gcc-uClibc-linux-GK710XS/lib/libap.so" include/lib/

mkdir -p include/bin
cp src/sip_media/wibox-media-daemon include/bin/

mkdir -p include/etc
cp src/sip_media/sip_media.conf.default include/etc/
cat > include/etc/wibox-release <<EOF
WIBOX_VERSION=$WIBOX_VERSION
WIBOX_COMMIT=$WIBOX_COMMIT
WIBOX_BUILD_TIMESTAMP=$WIBOX_BUILD_TIMESTAMP
EOF
