#!/bin/sh
set -eu

WIBOX_IP="${WIBOX_IP:-192.168.0.196}"
WIBOX_USER="${WIBOX_USER:-root}"
WIBOX_PASS="${WIBOX_PASS:-qv2008}"
SSH_OPTS="-o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null"

CONFIG=$(
  sshpass -p "${WIBOX_PASS}" ssh ${SSH_OPTS} "${WIBOX_USER}@${WIBOX_IP}" '
set -eu
HOSTNAME=$(cat /proc/sys/kernel/hostname 2>/dev/null || echo "")
echo "hostname=${HOSTNAME}"
if [ -f /mnt/mtd/sip_media.conf ]; then
  sed -n "s/^\(mqtt_[A-Za-z0-9_]*\)=\(.*\)$/\1=\2/p" /mnt/mtd/sip_media.conf
fi
'
)

get_cfg() {
  key="$1"
  printf "%s\n" "${CONFIG}" | awk -F= -v k="${key}" '$1 == k {print substr($0, length($1) + 2); exit}'
}

HOSTNAME=$(get_cfg hostname)
MQTT_ENABLED=$(get_cfg mqtt_enabled)
MQTT_HOST=$(get_cfg mqtt_host)
MQTT_USER=$(get_cfg mqtt_user)
MQTT_PASS_REMOTE=$(get_cfg mqtt_pass)
MQTT_HA_PREFIX=$(get_cfg mqtt_homeassistant_prefix)
MQTT_BASE_TOPIC=$(get_cfg mqtt_base_topic)
MQTT_DEVICE_ID=$(get_cfg mqtt_device_id)

if [ "${MQTT_ENABLED:-1}" = "0" ]; then
  echo "[!] MQTT is disabled in /mnt/mtd/sip_media.conf" >&2
  exit 1
fi
if [ -z "${MQTT_HOST}" ]; then
  echo "[!] mqtt_host is empty in /mnt/mtd/sip_media.conf" >&2
  exit 1
fi
if [ -z "${MQTT_HA_PREFIX}" ]; then
  MQTT_HA_PREFIX="homeassistant"
fi
if [ -z "${MQTT_DEVICE_ID}" ]; then
  MQTT_DEVICE_ID="${HOSTNAME}"
fi
if [ -z "${MQTT_BASE_TOPIC}" ]; then
  MQTT_BASE_TOPIC="wibox/${HOSTNAME}"
fi

echo "[*] Verifying active runtime on ${WIBOX_IP}"
WIBOX_IP="${WIBOX_IP}" WIBOX_USER="${WIBOX_USER}" WIBOX_PASS="${WIBOX_PASS}" \
  scripts/verify_runtime.sh

echo "[*] Verifying MQTT discovery/state from WiBox config"
echo "    MQTT_HOST=${MQTT_HOST}"
echo "    MQTT_USER=${MQTT_USER}"
echo "    MQTT_HA_PREFIX=${MQTT_HA_PREFIX}"
echo "    MQTT_DEVICE=${MQTT_DEVICE_ID}"
echo "    MQTT_BASE_TOPIC=${MQTT_BASE_TOPIC}"

MQTT_HOST="${MQTT_HOST}" \
MQTT_USER="${MQTT_USER}" \
MQTT_PASS="${MQTT_PASS_REMOTE}" \
MQTT_HA_PREFIX="${MQTT_HA_PREFIX}" \
MQTT_DEVICE="${MQTT_DEVICE_ID}" \
MQTT_BASE_TOPIC="${MQTT_BASE_TOPIC}" \
  scripts/verify_mqtt.py
