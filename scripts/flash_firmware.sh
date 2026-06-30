#!/bin/sh
set -eu

IMAGE="${1:-release/latest}"
WIBOX_IP="${WIBOX_IP:-192.168.0.196}"
WIBOX_USER="${WIBOX_USER:-root}"
WIBOX_PASS="${WIBOX_PASS:-qv2008}"
CONFIRM_FLASH="${CONFIRM_FLASH:-}"
FLASH_DRY_RUN="${FLASH_DRY_RUN:-0}"
SSH_OPTS="-o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null"

if [ "${CONFIRM_FLASH}" != "YES" ]; then
  echo "[!] Refusing to flash without CONFIRM_FLASH=YES" >&2
  echo "    Example: make flash CONFIRM_FLASH=YES" >&2
  exit 1
fi

if [ ! -f "${IMAGE}" ]; then
  echo "[!] Missing firmware image: ${IMAGE}" >&2
  exit 1
fi

SIZE=$(stat -L -c "%s" "${IMAGE}")
LOCAL_MD5=$(md5sum "${IMAGE}" | cut -d" " -f1)
echo "[*] Firmware: ${IMAGE} (${SIZE} bytes, md5 ${LOCAL_MD5})"

sshpass -p "${WIBOX_PASS}" ssh ${SSH_OPTS} "${WIBOX_USER}@${WIBOX_IP}" '
set -eu
if ! grep -q "mtd4: .* \"usr\"" /proc/mtd; then
  echo "[!] /proc/mtd does not show mtd4 as usr" >&2
  cat /proc/mtd >&2
  exit 2
fi
if ! mount | grep -q "/dev/mtdblock4 on /usr type cramfs"; then
  echo "[!] /usr is not mounted from /dev/mtdblock4 cramfs" >&2
  mount >&2
  exit 3
fi
'

REMOTE_MD5=$(sshpass -p "${WIBOX_PASS}" ssh ${SSH_OPTS} "${WIBOX_USER}@${WIBOX_IP}" "md5sum /tmp/update.img 2>/dev/null | cut -d' ' -f1" || true)
if [ "${REMOTE_MD5}" = "${LOCAL_MD5}" ]; then
  echo "[*] /tmp/update.img already matches local firmware"
else
  echo "[*] Uploading firmware to /tmp/update.img"
  base64 "${IMAGE}" | sshpass -p "${WIBOX_PASS}" ssh ${SSH_OPTS} "${WIBOX_USER}@${WIBOX_IP}" '
set -eu
base64 -d > /tmp/update.img
md5sum /tmp/update.img
'
fi

REMOTE_MD5=$(sshpass -p "${WIBOX_PASS}" ssh ${SSH_OPTS} "${WIBOX_USER}@${WIBOX_IP}" "md5sum /tmp/update.img | cut -d' ' -f1")
if [ "${REMOTE_MD5}" != "${LOCAL_MD5}" ]; then
  echo "[!] Uploaded image checksum mismatch: local=${LOCAL_MD5} remote=${REMOTE_MD5}" >&2
  exit 4
fi

if [ "${FLASH_DRY_RUN}" = "1" ]; then
  echo "[*] Dry run complete; /tmp/update.img is verified, mtd4 was not written."
  exit 0
fi

echo "[*] Running /usr/bin/update_firmware.sh"
sshpass -p "${WIBOX_PASS}" ssh ${SSH_OPTS} "${WIBOX_USER}@${WIBOX_IP}" '/usr/bin/update_firmware.sh'

echo "[*] Flash verified by updater. Reboot the WiBox to boot the new /usr image."
