# Getting Started

This is the shortest path to install the custom image on a WiBox.

Start with the supported firmware range:

- `V500.R001.A103.00.G0021.B007`
- `V500.R001.A103.00.G0021.B010`

`V500.R001.A103.00.G0021.B013` blocks telnet, so use serial for B013 or newer.

## 1. Get A Shell

Use the access method that matches your stock firmware:

- Telnet for B007/B010 when it works.
- Serial for B013/newer or when telnet is unavailable.
- U-Boot recovery when Linux will not boot.

For serial wiring and terminal settings, read [Serial TTL](serial_ttl.md).
For dead-boot recovery, read [Recovery](recovery.md).

## 2. Back Up The Factory Flash

Keep at least `mtd4`, the factory `/usr` cramfs image.

On your computer:

```bash
for i in $(seq 0 6); do
  nc -l -p 8888 > "mtd${i}"
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

## 3. Configure Persistent WiFi

Do this before flashing. If `/mnt/mtd/wpa_supplicant.conf` is missing or
wrong, the custom image will boot without network access.

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

## 4. Build Or Download The Image

The easiest path is the latest GitHub Release image:

```bash
VERSION="v0.5.0"  # x-release-please-version
wget -O wibox-media.img "https://github.com/segator/wibox-media/releases/download/${VERSION}/wibox-media-${VERSION}.img"
```

To build locally:

```bash
make docker
make build
```

Local builds write the image to `release/latest`.

## 5. Flash

### Telnet

```bash
PC_IP=192.168.1.100
nc -l -p 8888 < wibox-media.img
```

On the WiBox:

```sh
nc "${PC_IP}" 8888 > /tmp/update.img
dd if=/tmp/update.img of=/dev/mtdblock4 bs=4096
sync
fsync /dev/mtdblock4
reboot
```

### Serial

For first installation over serial, transfer `wibox-media.img` or
`release/latest` to `/tmp/update.img`, then write it to the `/usr` partition:

```sh
dd if=/tmp/update.img of=/dev/mtdblock4 bs=4096
sync
fsync /dev/mtdblock4
reboot
```

## 6. Configure Runtime

After first boot, edit:

```text
/mnt/mtd/sip_media.conf
```

The file is copied from `/etc/sip_media.conf.default` on first boot.

The key options are:

- `video_enabled`
- `mqtt_enabled`
- `firmware_update_enabled`
- `firmware_update_repo`
- `prometheus_enabled`

For the full runtime model, read [SIP Media](sip_media.md).

## 7. Future Updates

After the custom firmware is installed, update from Home Assistant or with the
on-device updater:

```sh
/usr/bin/firmware_update --status
/usr/bin/firmware_update
```

For the full update workflow and troubleshooting notes, read [Updates](updates.md).
