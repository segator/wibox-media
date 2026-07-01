# Hardware Reference

Reference notes for the WiBox GK7102S hardware and stock boot layout.

## Power

The board is powered from the intercom bus, around 18V DC in the observed
installation. Voltage can drop during physical handset or relay activity. If
the board repeatedly resets during bench work, verify the intercom power path
before debugging software.

## Flash Layout

The device has 16MB SPI flash:

```text
dev:    size      erasesize  name
mtd0:   00040000  00010000   "uboot"
mtd1:   00010000  00010000   "env"
mtd2:   001e0000  00010000   "kernel"
mtd3:   00230000  00010000   "rootfs"
mtd4:   00b10000  00010000   "usr"
mtd5:   00080000  00010000   "mnt"
mtd6:   00010000  00010000   "cfg"
```

Mounted layout:

```text
/dev/root      /      squashfs  read-only
/dev/mtdblock4 /usr   cramfs    read-only
/dev/mtdblock5 /mnt   jffs2     writable
/dev/mem       /var   ramfs
/dev/mem       /etc   ramfs overlay
/tmp           /tmp   tmpfs
```

Important partitions:

- `mtd4`: `/usr`, the custom firmware image replaces this partition.
- `mtd5`: `/mnt`, persistent user/config data.
- `mtd6`: factory/device identity data.

Release images must fit inside `mtd4` (`0x00b10000`, 11,599,872 bytes).

## Boot Chain

Stock `rcS` mounts the filesystems and then executes:

```text
/usr/run.sh
```

The custom `run.sh`:

- initializes GPIO and LEDs;
- brings up `eth0` as `192.168.1.10`;
- starts telnet briefly, then Dropbear SSH when available;
- loads media, audio, sensor and WiFi kernel modules;
- configures WiFi using `/mnt/mtd/wpa_supplicant.conf` when present;
- runs `Sofia_temp.sh` once for video hardware warmup;
- starts `wibox-media-daemon` with `app_watchdog.sh`;
- runs optional `/mnt/mtd/post.sh`.

## Network Notes

Ethernet is configured as:

```text
eth0 192.168.1.10/24
```

WiFi station configuration should be persisted before flashing:

```text
/mnt/mtd/wpa_supplicant.conf
```

If this file is missing or invalid, the custom firmware may boot without
network access and require serial recovery.

## Serial Devices

Known serial paths:

```text
/dev/ttySGK1  intercom MCU protocol
ttySGK2       serial console observed on board pads
```

See [Serial TTL](serial_ttl.md) for console wiring and U-Boot entry.
See [UART Codes](codes.md) for intercom MCU frames.

## Sofia Notes

The vendor Sofia binary is not used for calls in the custom firmware. It is
still run briefly at boot by `Sofia_temp.sh` because it initializes VI/VENC
state required by the current H.264 D1 capture path.

The goal remains to remove this warmup once the full VI/sensor/encoder
initialization sequence is reproduced.

## Debug Commands

Useful checks on a running WiBox:

```sh
cat /proc/mtd
mount
ifconfig -a
route -n
ps | grep -E 'wibox-media-daemon|Sofia|app_watchdog' | grep -v grep
cat /usr/etc/wibox-release
tail -80 /var/log/wibox-media-daemon.log
```
