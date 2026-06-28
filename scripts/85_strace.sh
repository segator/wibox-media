#!/bin/sh
# Patch run-orig.sh: add dropbear SSH + sofia_trace ioctl tracer
# Only runs when /mnt/mtd/factory flag is set
if [ ! -f "${ROOTFS}/run-orig.sh" ]; then
  echo "[*] run-orig.sh not found, skipping"
  exit 0
fi

echo "[*] Adding dropbear + sofia_trace to run-orig.sh"

sed -i "/telnetd \&/a\\
/usr/sbin/dropbear -R 2>/dev/null \&" ${ROOTFS}/run-orig.sh

sed -i "s|interDebug /var/Sofia 9527|# IOCTL tracer: logs to /mnt/mtd/iotrace_boot.log\\
/usr/bin/sofia_trace /var/Sofia|" ${ROOTFS}/run-orig.sh

echo "[*] Tracer mode applied to run-orig.sh"
grep "sofia_trace\|dropbear" ${ROOTFS}/run-orig.sh