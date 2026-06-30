#!/bin/sh
set -eu

WIBOX_IP="${WIBOX_IP:-192.168.0.196}"
WIBOX_USER="${WIBOX_USER:-root}"
WIBOX_PASS="${WIBOX_PASS:-qv2008}"
SSH_OPTS="-o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null"

sshpass -p "${WIBOX_PASS}" ssh ${SSH_OPTS} "${WIBOX_USER}@${WIBOX_IP}" '
echo "== process =="
ps | grep -E "wibox-media-daemon|app_watchdog|Sofia" | grep -v grep || true
echo
echo "== executable =="
PID=$(pidof wibox-media-daemon 2>/dev/null | awk "{print \$1}")
if [ -n "${PID}" ]; then
  readlink "/proc/${PID}/exe" 2>/dev/null || true
  tr "\0" " " <"/proc/${PID}/cmdline" 2>/dev/null || true
  echo
fi
echo
echo "== mtd =="
cat /proc/mtd
echo
echo "== config =="
sed -n "1,120p" /mnt/mtd/sip_media.conf 2>/dev/null | sed "s/^mqtt_pass=.*/mqtt_pass=<redacted>/" || true
echo
echo "== log =="
tail -80 /var/log/wibox-media-daemon.log 2>/dev/null || true
'
