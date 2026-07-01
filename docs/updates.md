# Firmware Updates

This page covers updates after the custom firmware is already installed. For a
stock device, use [Getting Started](getting_started.md).

## What Runs On The WiBox

The image includes `/usr/bin/firmware_update`, a small HTTPS-capable updater for
the WiBox. It exists because the stock `wget` cannot download GitHub release
assets over HTTPS.

The updater:

- reads `/mnt/mtd/sip_media.conf`;
- queries the latest GitHub release from `firmware_update_repo`;
- downloads `wibox-media-${VERSION}.img` to `/tmp/update.img`;
- downloads `MD5SUMS`;
- verifies the downloaded image;
- unmounts `/usr`;
- erases and writes `/dev/mtd4` through the MTD character device;
- verifies flashed bytes against the expected MD5;
- reboots when verification succeeds.

Do not use `dd` to `/dev/mtdblock4` for routine automatic updates. The updater
uses the MTD character device because block-device writes can appear valid
before reboot and still leave corrupted cramfs blocks afterwards.

## Configuration

In `/mnt/mtd/sip_media.conf`:

```ini
firmware_update_enabled=1
firmware_update_repo=segator/wibox-media
```

`firmware_update_enabled` defaults to `1`. Set it to `0` to disable update
entities in MQTT/Home Assistant.

`firmware_update_repo` must be `owner/repo`. The default points to this project.

## Home Assistant

When firmware updates are enabled, the daemon publishes:

- `Firmware Update Available`: `ON` when GitHub has a newer release.
- `Firmware Update Version`: latest remote release tag.
- `Firmware Update Refresh`: checks GitHub immediately.
- `Firmware Update Install`: launches `/usr/bin/firmware_update`.

Install button availability topic:

```text
wibox/<device_id>/firmware/update/install/availability
```

It is `online` only when an update is available and no install is already in
progress.

When Home Assistant sends an install request, the daemon immediately publishes
the install button as `offline` and ignores duplicate install requests while the
updater runs. The normal successful path is a verified flash followed by reboot.

The daemon checks for updates at startup and roughly once per day. Press
`Firmware Update Refresh` after publishing a release if you want Home Assistant
to show it immediately.

## Terminal Usage

Check status:

```sh
/usr/bin/firmware_update --status
```

Example:

```text
repo=segator/wibox-media
local_version=v0.5.1
remote_version=v0.5.1
available=0
image_url=https://github.com/segator/wibox-media/releases/download/v0.5.1/wibox-media-v0.5.1.img
```

Install latest release:

```sh
/usr/bin/firmware_update
```

Force reinstall current release:

```sh
/usr/bin/firmware_update --force
```

Download only:

```sh
/usr/bin/firmware_update --output /tmp/update.img
```

Install an image already present on the WiBox:

```sh
/usr/bin/firmware_update --image /tmp/update.img --expected-md5 0123456789abcdef0123456789abcdef
```

Skip reboot after flashing, for controlled tests only:

```sh
/usr/bin/firmware_update --no-reboot
```

## Troubleshooting

Log:

```text
/tmp/firmware_update.log
```

`/tmp` is tmpfs, so the log disappears after reboot.

After an update:

```sh
cat /usr/etc/wibox-release
dmesg | grep -i -E 'cramfs|decompress|error'
tail -80 /var/log/wibox-media-daemon.log
```

If the WiBox does not boot cleanly, follow [Recovery](recovery.md).
