#!/bin/sh
set -eu

WIBOX_IP="${WIBOX_IP:-192.168.0.196}"
WIBOX_USER="${WIBOX_USER:-root}"
WIBOX_PASS="${WIBOX_PASS:-qv2008}"
BACKUP_DIR="${BACKUP_DIR:-backups}"
SSH_OPTS="-o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null"

mkdir -p "${BACKUP_DIR}"
STAMP=$(date +%y%m%d-%H%M%S)
OUT="${BACKUP_DIR}/mtd4-${WIBOX_IP}-${STAMP}.img"

INFO=$(sshpass -p "${WIBOX_PASS}" ssh ${SSH_OPTS} "${WIBOX_USER}@${WIBOX_IP}" '
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
SIZE_DEC=$(wc -c < /dev/mtdblock4 | tr -d " ")
MD5=$(md5sum /dev/mtdblock4 | cut -d" " -f1)
echo "SIZE=${SIZE_DEC}"
echo "MD5=${MD5}"
')

REMOTE_SIZE=$(printf "%s\n" "${INFO}" | awk -F= '/^SIZE=/{print $2}')
REMOTE_MD5=$(printf "%s\n" "${INFO}" | awk -F= '/^MD5=/{print $2}')

echo "[*] Backing up /dev/mtdblock4 from ${WIBOX_IP}"
sshpass -p "${WIBOX_PASS}" ssh ${SSH_OPTS} "${WIBOX_USER}@${WIBOX_IP}" \
  'dd if=/dev/mtdblock4 bs=65536 2>/dev/null' > "${OUT}.tmp"

LOCAL_SIZE=$(stat -c "%s" "${OUT}.tmp")
LOCAL_MD5=$(md5sum "${OUT}.tmp" | cut -d" " -f1)

if [ "${LOCAL_SIZE}" != "${REMOTE_SIZE}" ]; then
  echo "[!] Backup size mismatch: local=${LOCAL_SIZE} remote=${REMOTE_SIZE}" >&2
  rm -f "${OUT}.tmp"
  exit 4
fi
if [ "${LOCAL_MD5}" != "${REMOTE_MD5}" ]; then
  echo "[!] Backup checksum mismatch: local=${LOCAL_MD5} remote=${REMOTE_MD5}" >&2
  rm -f "${OUT}.tmp"
  exit 5
fi

mv "${OUT}.tmp" "${OUT}"
echo "[*] Backup OK: ${OUT}"
echo "    size=${LOCAL_SIZE} md5=${LOCAL_MD5}"
