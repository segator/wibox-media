#!/bin/sh

echo "[*] Applying patches"
if [ ! -d patches ] || ! ls patches/* >/dev/null 2>&1; then
  echo "[*] No patches found, skipping."
  exit 0
fi

for PATCHFILE in patches/*; do
  patch -p1 --no-backup-if-mismatch -r /dev/null -d "${ROOTFS}" < "${PATCHFILE}"
done
