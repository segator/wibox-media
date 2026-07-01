# Getting Started

This guide is the stock-to-custom install path for a WiBox GK7102S.

The first installation is different from later updates:

- stock firmware normally has telnet or serial, not SSH;
- stock `wget` cannot download GitHub HTTPS release assets;
- WiFi configuration must be persisted before flashing or the custom firmware
  may boot without network access.

## 1. Check The Stock Firmware

Tested stock firmware versions:

- `V500.R001.A103.00.G0021.B007`
- `V500.R001.A103.00.G0021.B010`

Known caveat:

- `V500.R001.A103.00.G0021.B013` blocks telnet. Use serial for B013 or newer.

If you have B007/B010 and telnet works, serial is not required for the first
install. If telnet does not work, use [Serial TTL](serial_ttl.md).

## 2. Get A Shell

Use one of these paths:

- Telnet on B007/B010 when available.
- Serial shell when telnet is unavailable.
- U-Boot recovery only when Linux does not boot far enough to provide a shell.

For serial wiring and terminal settings, read [Serial TTL](serial_ttl.md).
For dead-boot recovery, read [Recovery](recovery.md).

## 3. Back Up Factory Flash

Keep a copy of every MTD partition before flashing. At minimum, preserve
`mtd4`, the factory `/usr` cramfs image.

On your computer, run this from a directory where you want to store the backup:

```bash
for i in $(seq 0 6); do
  echo "waiting for mtd${i}"
  nc -l -p 8888 > "mtd${i}.img"
done
```

On the WiBox shell:

```sh
PC_IP=192.168.1.100
for i in $(seq 0 6); do
  dd if=/dev/mtd${i} bs=4096 | nc "${PC_IP}" 8888
  sleep 1
done
```

Replace `192.168.1.100` with your computer's IP on the same network.

## 4. Configure Persistent WiFi

This is required before the first custom flash. If
`/mnt/mtd/wpa_supplicant.conf` is missing or wrong, the custom firmware can boot
without WiFi and you will need serial to fix it.

Create `/mnt/mtd/wpa_supplicant.conf` on the WiBox:

```ini
ctrl_interface=/var/run/wpa_supplicant
ap_scan=1

network={
        ssid="YOUR_WIFI_NAME"
        psk="YOUR_WIFI_PASSWORD"
        scan_ssid=1
        key_mgmt=WPA-PSK
}
```

## 5. Get The Firmware Image

On your computer, download the latest GitHub Release image:

```bash
VERSION="v0.5.2"  # x-release-please-version
wget -O wibox-media.img "https://github.com/segator/wibox-media/releases/download/${VERSION}/wibox-media-${VERSION}.img"
```

Do not run this on the stock WiBox. Its `wget` cannot download GitHub HTTPS
release assets. The next step transfers the already-downloaded image from your
computer to the WiBox with `nc`.

To build locally instead:

```bash
make docker
make build
```

Local builds write the image to `release/latest`.

## 6. Install The First Custom Image

Use `nc` to transfer the image because stock firmware cannot fetch GitHub HTTPS
assets directly.

On your computer:

```bash
nc -l -p 8888 < wibox-media.img
```

On the WiBox shell:

```sh
PC_IP=192.168.1.100
nc "${PC_IP}" 8888 > /tmp/update.img
md5sum /tmp/update.img
dd if=/tmp/update.img of=/dev/mtdblock4 bs=4096
sync
fsync /dev/mtdblock4
reboot
```

Use the MD5 output to confirm the transferred file matches your computer before
writing flash.

If you are installing from a serial shell without network, transfer the image
with your serial tooling or use the U-Boot method in [Recovery](recovery.md).

## 7. First Boot

After the custom image boots:

- SSH should be available through Dropbear.
- The default password remains the device root password unless you bind a custom
  `/mnt/mtd/passwd`.
- `/mnt/mtd/sip_media.conf` is created from `/etc/sip_media.conf.default` if it
  does not already exist.
- `wibox-media-daemon` starts under `app_watchdog.sh`.

Check the release metadata:

```sh
cat /usr/etc/wibox-release
```

Check the daemon log:

```sh
tail -80 /var/log/wibox-media-daemon.log
```

## 8. Configure Runtime

Edit:

```text
/mnt/mtd/sip_media.conf
```

Important options:

```ini
outgoing_call_target=sip:1000@192.168.0.31:5060
outgoing_call_timeout=60
video_enabled=1
mqtt_enabled=1
mqtt_host=192.168.0.203
mqtt_user=wibox
mqtt_pass=change-me
firmware_update_enabled=1
prometheus_enabled=1
```

Set `video_enabled=0` for audio-only doorphones.

Reboot after editing persistent config:

```sh
reboot
```

## 9. Home Assistant

When MQTT is configured, the daemon publishes Home Assistant discovery for:

- `Open Door` button;
- `Media State` sensor: `idle`, `ringing`, `established`;
- firmware version, commit and build timestamp sensors;
- `Door Unlocked` pulse binary sensor;
- `WiFi RSSI` sensor;
- `Video Enabled` switch;
- firmware update available/version sensors;
- firmware update refresh/install buttons.

Home Assistant should be the normal control surface after installation.

## 10. Future Updates

Do not repeat the first-install `dd` flow for routine upgrades.

Use Home Assistant firmware update buttons or the on-device updater:

```sh
/usr/bin/firmware_update --status
/usr/bin/firmware_update
```

See [Firmware Updates](updates.md).
