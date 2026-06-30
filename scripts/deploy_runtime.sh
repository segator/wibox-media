#!/bin/sh
set -eu

WIBOX_IP="${WIBOX_IP:-192.168.0.196}"
WIBOX_USER="${WIBOX_USER:-root}"
WIBOX_PASS="${WIBOX_PASS:-qv2008}"
LOCAL_BIN="${LOCAL_BIN:-include/bin/wibox-media-daemon}"
REMOTE_DIR="${REMOTE_DIR:-/tmp/wibox-media-test}"
SSH_OPTS="-o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null"

if [ ! -x "${LOCAL_BIN}" ]; then
  echo "[!] Missing local daemon: ${LOCAL_BIN}" >&2
  exit 1
fi

LOCAL_MD5=$(md5sum "${LOCAL_BIN}" | cut -d" " -f1)
echo "[*] Uploading ${LOCAL_BIN} to ${WIBOX_USER}@${WIBOX_IP}:${REMOTE_DIR}"

base64 "${LOCAL_BIN}" | sshpass -p "${WIBOX_PASS}" ssh ${SSH_OPTS} "${WIBOX_USER}@${WIBOX_IP}" "
set -eu
mkdir -p '${REMOTE_DIR}/bin'
base64 -d > '${REMOTE_DIR}/bin/wibox-media-daemon.new'
chmod +x '${REMOTE_DIR}/bin/wibox-media-daemon.new'
mv '${REMOTE_DIR}/bin/wibox-media-daemon.new' '${REMOTE_DIR}/bin/wibox-media-daemon'
md5sum '${REMOTE_DIR}/bin/wibox-media-daemon'
"

REMOTE_MD5=$(sshpass -p "${WIBOX_PASS}" ssh ${SSH_OPTS} "${WIBOX_USER}@${WIBOX_IP}" "md5sum '${REMOTE_DIR}/bin/wibox-media-daemon' | cut -d' ' -f1")
if [ "${REMOTE_MD5}" != "${LOCAL_MD5}" ]; then
  echo "[!] Checksum mismatch: local=${LOCAL_MD5} remote=${REMOTE_MD5}" >&2
  exit 2
fi

echo "[*] Restarting temporary daemon"
sshpass -p "${WIBOX_PASS}" ssh ${SSH_OPTS} "${WIBOX_USER}@${WIBOX_IP}" "
set -eu
killall wibox-media-daemon 2>/dev/null || true
sleep 2
cd '${REMOTE_DIR}'
export LD_LIBRARY_PATH='${REMOTE_DIR}/lib:/usr/lib:/lib'
mkdir -p /var/log
nohup ./bin/wibox-media-daemon /mnt/mtd/sip_media.conf >/var/log/wibox-media-daemon.log 2>&1 &
sleep 5
ps | grep -E 'wibox-media-daemon' | grep -v grep
tail -60 /var/log/wibox-media-daemon.log
"
