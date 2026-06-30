#!/bin/sh
set -eu

CONF_FILE="${FIRMWARE_UPDATE_CONF:-/mnt/mtd/sip_media.conf}"
DEFAULT_CONF="/etc/sip_media.conf.default"
REPO="${FIRMWARE_UPDATE_REPO:-}"
UPDATE_FILE="${FIRMWARE_UPDATE_FILE:-/tmp/update.img}"
LOG_FILE="${FIRMWARE_UPDATE_LOG:-/tmp/firmware_update.log}"
API_JSON=""
LATEST_URL=""
LATEST_TAG=""

log() {
  printf '%s\n' "$*" | tee -a "$LOG_FILE"
}

get_cfg() {
  key="$1"
  file="$2"
  sed -n "s/^${key}=//p" "$file" 2>/dev/null | tail -n 1
}

if [ ! -f "$CONF_FILE" ] && [ -f "$DEFAULT_CONF" ]; then
  CONF_FILE="$DEFAULT_CONF"
fi

if [ -z "$REPO" ] && [ -f "$CONF_FILE" ]; then
  REPO="$(get_cfg firmware_update_repo "$CONF_FILE")"
fi

if [ -z "$REPO" ]; then
  REPO="aymerici/wibox-media"
fi

if [ -f "$CONF_FILE" ]; then
  ENABLED="$(get_cfg firmware_update_enabled "$CONF_FILE")"
  if [ "${ENABLED:-1}" = "0" ]; then
    log "[*] Firmware updates are disabled in config"
    exit 0
  fi
fi

API_URL="https://api.github.com/repos/${REPO}/releases/latest"
log "[*] Querying latest release for ${REPO}"
if command -v wget >/dev/null 2>&1; then
  API_JSON="$(wget -qO- "$API_URL" 2>/dev/null || true)"
elif command -v busybox >/dev/null 2>&1; then
  API_JSON="$(busybox wget -qO- "$API_URL" 2>/dev/null || true)"
else
  log "[!] wget is not available"
  exit 1
fi
if [ -z "$API_JSON" ]; then
  log "[!] Failed to query release metadata"
  exit 1
fi

LATEST_TAG="$(printf '%s' "$API_JSON" | sed -n 's/.*"tag_name":"\([^"]*\)".*/\1/p' | head -n 1)"
LATEST_URL="$(printf '%s' "$API_JSON" | sed -n 's/.*"browser_download_url":"\([^"]*\.img\)".*/\1/p' | head -n 1 | sed 's#\\/#/#g')"

if [ -z "$LATEST_TAG" ] || [ -z "$LATEST_URL" ]; then
  log "[!] Could not extract latest release tag or image URL"
  exit 2
fi

log "[*] Latest release: ${LATEST_TAG}"
log "[*] Downloading image from ${LATEST_URL}"
rm -f "$UPDATE_FILE"
if command -v wget >/dev/null 2>&1; then
  wget -qO "$UPDATE_FILE" "$LATEST_URL" 2>/dev/null || {
    log "[!] Download failed"
    exit 3
  }
elif command -v busybox >/dev/null 2>&1; then
  busybox wget -qO "$UPDATE_FILE" "$LATEST_URL" 2>/dev/null || {
    log "[!] Download failed"
    exit 3
  }
else
  log "[!] Download failed"
  exit 3
fi

if [ -x /usr/bin/update_firmware.sh ]; then
  log "[*] Flashing with /usr/bin/update_firmware.sh"
  /usr/bin/update_firmware.sh
  log "[*] Rebooting"
  exec busybox reboot
fi

log "[!] /usr/bin/update_firmware.sh is missing"
exit 4
