# Firmware Updates

This page covers updates after the custom firmware is already installed.
For the first installation from stock firmware, use [Getting Started](getting_started.md).

## What Runs On The WiBox

The image includes `/usr/bin/firmware_update`, a small updater built for the
WiBox. It does not rely on the stock `wget`, because the stock binary cannot
download from HTTPS GitHub URLs.

The updater:

- reads `/mnt/mtd/sip_media.conf`;
- queries the latest GitHub release over HTTPS;
- downloads `wibox-media-${VERSION}.img` to `/tmp/update.img`;
- downloads `MD5SUMS` and verifies the image before flashing;
- unmounts `/usr`;
- erases and writes `/dev/mtd4` through the MTD character device;
- verifies the flashed bytes against the expected MD5;
- reboots when verification succeeds.

Do not use `dd` to `/dev/mtdblock4` for automatic updates. It can appear to
verify before reboot and still leave corrupted cramfs blocks afterwards.

## Configuration

Firmware updates are configured in `/mnt/mtd/sip_media.conf`:

```ini
firmware_update_enabled=1
firmware_update_repo=segator/wibox-media
```

`firmware_update_enabled` defaults to `1`. Set it to `0` to disable the update
entities in MQTT/Home Assistant.

`firmware_update_repo` defaults to `segator/wibox-media`. Keep the value as
`owner/repo`; the updater builds release URLs from that repository.

## Home Assistant

When firmware updates are enabled, the daemon publishes:

- `Firmware Update Available`: binary sensor, `ON` when GitHub has a newer release.
- `Firmware Update Version`: text sensor with the latest release tag.
- `Firmware Update Refresh`: button that checks GitHub immediately without installing.
- `Firmware Update Install`: button that launches `/usr/bin/firmware_update`.

The install button has its own availability topic:

```text
wibox/<device_id>/firmware/update/install/availability
```

It is published as:

- `online` when an update is available;
- `offline` when the WiBox is already on the latest release.

Home Assistant shows the button disabled while the availability topic is
`offline`.

The daemon checks for updates when it starts and then roughly once per day.
Press `Firmware Update Refresh` when you have just published a release and want
Home Assistant to show the new version immediately without waiting or rebooting.

## Terminal Usage

Check update status:

```sh
/usr/bin/firmware_update --status
```

Example output:

```text
repo=segator/wibox-media
local_version=v0.4.9
remote_version=v0.4.9
available=0
image_url=https://github.com/segator/wibox-media/releases/download/v0.4.9/wibox-media-v0.4.9.img
```

Install the latest release:

```sh
/usr/bin/firmware_update
```

Force reinstall even when the local version is current:

```sh
/usr/bin/firmware_update --force
```

Download to a custom path:

```sh
/usr/bin/firmware_update --output /tmp/update.img
```

Install an image that is already on the WiBox:

```sh
/usr/bin/firmware_update --image /tmp/update.img --expected-md5 0123456789abcdef0123456789abcdef
```

Skip reboot after flashing, useful only for controlled testing:

```sh
/usr/bin/firmware_update --no-reboot
```

## Troubleshooting

Logs are written to:

```text
/tmp/firmware_update.log
```

`/tmp` is tmpfs, so the log disappears after reboot.

After an update, verify:

```sh
cat /usr/etc/wibox-release
dmesg | grep -i -E 'cramfs|decompress|error'
```

For a release image, the MD5 of the image-sized prefix of `mtd4` should match
the release `MD5SUMS` entry. For example, if the image is `5070848` bytes:

```sh
dd if=/dev/mtdblock4 bs=4096 count=1238 2>/dev/null | md5sum
```

## Vendor Firmware Notes

Historical vendor update information kept for reference:

```text
Path: /usr/local/tdk/bin/UPF/pub/G0021/IPC/A103/7938_MPT_QT400_G0021_20220522_V500R001B013.upf
Length: 8316096
CRC32: A11B0EF
Build: 500001013
Date: 20220522
Files:
setup                              656 (1135)
u-boot.bin                       94363 (185792)
u-boot.env                         555 (65536)
uImage                         1755278 (1760376)
rootfs.squashfs                2226231 (2228224)
usr.cramfs                     4229264 (4235264)
mnt.jffs2                         7513 (8320)
PducDefault                         78 (65536)

V500.R001.A103.00.G0021.B007
2019-07-04T15:54:33Z
```
