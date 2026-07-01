#!/bin/sh
set -eu

IMAGE="${IMAGE:-release/latest}"
LOCAL_BIN="${LOCAL_BIN:-include/bin/wibox-media-daemon}"
ROOT=".verify-image-root"
DOCKER_IMAGE="${DOCKER_IMAGE:-wibox-build-tool:latest}"
HOST_UID=$(id -u)
HOST_GID=$(id -g)

docker_root_sh() {
  docker run --rm --entrypoint /bin/bash -v "$(pwd)":/build "${DOCKER_IMAGE}" \
    --noprofile --norc -lc "$1"
}

if [ ! -f "${IMAGE}" ]; then
  echo "[!] Missing firmware image: ${IMAGE}" >&2
  echo "    Run make build first." >&2
  exit 1
fi
if [ ! -x "${LOCAL_BIN}" ]; then
  echo "[!] Missing local daemon: ${LOCAL_BIN}" >&2
  exit 1
fi

docker_root_sh "rm -rf '/build/${ROOT}'"

echo "[*] Extracting ${IMAGE}"
docker_root_sh "cd /build && cramfsck -x '${ROOT}' '${IMAGE}' >/tmp/verify-image-cramfsck.log && chown -R ${HOST_UID}:${HOST_GID} '${ROOT}'"

require_file() {
  path="$1"
  if [ ! -e "${ROOT}/${path}" ]; then
    echo "[!] Missing ${path} in ${IMAGE}" >&2
    exit 2
  fi
}

require_absent() {
  pattern="$1"
  if find "${ROOT}" -path "${ROOT}/${pattern}" -print -quit | grep -q .; then
    echo "[!] Unexpected legacy artifact in ${IMAGE}: ${pattern}" >&2
    find "${ROOT}" -path "${ROOT}/${pattern}" -print >&2
    exit 3
  fi
}

require_file "bin/wibox-media-daemon"
require_file "bin/firmware_update"
require_file "bin/app_watchdog.sh"
require_file "etc/sip_media.conf.default"
require_file "etc/wibox-release"
require_file "run.sh"
require_file "lib/libap.so"
require_file "lib/libadi.so"

LOCAL_MD5=$(md5sum "${LOCAL_BIN}" | cut -d" " -f1)
IMAGE_MD5=$(md5sum "${ROOT}/bin/wibox-media-daemon" | cut -d" " -f1)
if [ "${LOCAL_MD5}" != "${IMAGE_MD5}" ]; then
  echo "[!] Firmware daemon checksum mismatch: local=${LOCAL_MD5} image=${IMAGE_MD5}" >&2
  exit 4
fi

if ! grep -q "wibox-media-daemon" "${ROOT}/run.sh"; then
  echo "[!] run.sh does not start wibox-media-daemon" >&2
  exit 7
fi
if ! grep -q "Sofia_temp.sh" "${ROOT}/run.sh"; then
  echo "[!] run.sh does not contain Sofia warmup" >&2
  exit 8
fi
if ! grep -q "^video_enabled=1" "${ROOT}/etc/sip_media.conf.default"; then
  echo "[!] default config does not enable video by default" >&2
  exit 9
fi
if ! grep -q "^prometheus_enabled=1" "${ROOT}/etc/sip_media.conf.default"; then
  echo "[!] default config does not enable Prometheus by default" >&2
  exit 9
fi
if ! grep -q "^prometheus_port=9617" "${ROOT}/etc/sip_media.conf.default"; then
  echo "[!] default config does not set Prometheus port 9617" >&2
  exit 9
fi
for key in WIBOX_VERSION WIBOX_COMMIT WIBOX_BUILD_TIMESTAMP; do
  if ! grep -q "^${key}=." "${ROOT}/etc/wibox-release"; then
    echo "[!] ${ROOT}/etc/wibox-release missing ${key}" >&2
    exit 10
  fi
done

require_absent "bin/listener*.sh"
require_absent "bin/mqtt_*.sh"
require_absent "bin/heartbeat_mqtt.sh"
require_absent "bin/mosquitto_*"
require_absent "bin/ipctool"
require_absent "bin/dbclient"
require_absent "bin/scp"
require_absent "bin/audio_bridge"
require_absent "bin/video_rtp_bridge"
require_absent "bin/sip_media"
require_absent "bin/sofia_trace"
require_absent "bin/firmware_update.sh"
require_absent "bin/update_firmware.sh"
require_absent "sbin/dropbearconvert"

echo "[*] Firmware image OK: ${IMAGE}"
echo "    daemon_md5=${IMAGE_MD5}"
