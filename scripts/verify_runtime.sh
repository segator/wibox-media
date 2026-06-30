#!/bin/sh
set -eu

WIBOX_IP="${WIBOX_IP:-192.168.0.196}"
WIBOX_USER="${WIBOX_USER:-root}"
WIBOX_PASS="${WIBOX_PASS:-qv2008}"
LOCAL_BIN="${LOCAL_BIN:-include/bin/wibox-media-daemon}"
SSH_OPTS="-o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null"

if [ ! -x "${LOCAL_BIN}" ]; then
  echo "[!] Missing local daemon: ${LOCAL_BIN}" >&2
  exit 1
fi

LOCAL_MD5=$(md5sum "${LOCAL_BIN}" | cut -d" " -f1)

REMOTE=$(
  sshpass -p "${WIBOX_PASS}" ssh ${SSH_OPTS} "${WIBOX_USER}@${WIBOX_IP}" '
set -eu
PID=$(pidof wibox-media-daemon 2>/dev/null | awk "{print \$1}")
if [ -z "${PID}" ]; then
  echo "ERROR no wibox-media-daemon process"
  exit 10
fi
EXE=$(readlink "/proc/${PID}/exe" 2>/dev/null || true)
if [ -z "${EXE}" ]; then
  echo "ERROR cannot resolve /proc/${PID}/exe"
  exit 11
fi
MD5=$(md5sum "/proc/${PID}/exe" | cut -d" " -f1)
CMD=$(tr "\0" " " <"/proc/${PID}/cmdline" 2>/dev/null || true)
echo "PID=${PID}"
echo "EXE=${EXE}"
echo "MD5=${MD5}"
echo "CMD=${CMD}"
'
)

printf "%s\n" "${REMOTE}"
REMOTE_MD5=$(printf "%s\n" "${REMOTE}" | awk -F= '/^MD5=/{print $2}')

if [ "${REMOTE_MD5}" != "${LOCAL_MD5}" ]; then
  echo "[!] Active daemon checksum mismatch: local=${LOCAL_MD5} remote=${REMOTE_MD5}" >&2
  exit 2
fi

echo "[*] Active WiBox daemon matches ${LOCAL_BIN} (${LOCAL_MD5})"
