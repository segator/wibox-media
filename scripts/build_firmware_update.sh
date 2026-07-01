#!/bin/sh
set -eu

SDK_DIR="${SDK_DIR:-$(pwd)/third_party/gk710x-sdk-min}"
BUILD_IMAGE="${BUILD_IMAGE:-wibox-build-tool:latest}"
SSL_LIB_DIR="${SSL_LIB_DIR:-$(pwd)/cramfs/lib}"

if [ ! -f "${SSL_LIB_DIR}/libssl.so.1.1" ] || [ ! -f "${SSL_LIB_DIR}/libcrypto.so.1.1" ]; then
  echo "[!] Missing ARM OpenSSL runtime libraries in ${SSL_LIB_DIR}" >&2
  echo "    Run make prepare-base or make build." >&2
  exit 1
fi

docker run --rm \
  -v "$(pwd)":/workspace \
  -v "$SDK_DIR":/sdk \
  -v "$SSL_LIB_DIR":/openssl-libs:ro \
  -w /workspace \
  "$BUILD_IMAGE" \
  sh -lc '
    export PATH=/opt/4.6.1/usr/bin:$PATH
    arm-goke-linux-uclibcgnueabi-gcc \
      -O2 -g -std=gnu99 -D_GNU_SOURCE -Wno-deprecated-declarations \
      -o /workspace/include/bin/firmware_update /workspace/src/firmware_update.c \
      /openssl-libs/libssl.so.1.1 /openssl-libs/libcrypto.so.1.1 \
      -ldl -lpthread -lm
    chmod +x /workspace/include/bin/firmware_update
  '
