# Recovery

Use the least invasive path that still gives you control of the device.

## 1. Running Custom Firmware With SSH

Use this when the WiBox boots, has network, and SSH works.

Check the installed release:

```sh
cat /usr/etc/wibox-release
```

Check whether GitHub has a newer release:

```sh
/usr/bin/firmware_update --status
```

Install the latest release:

```sh
/usr/bin/firmware_update
```

This is the preferred recovery path for a working custom firmware because the
updater downloads over HTTPS, verifies `MD5SUMS`, writes through `/dev/mtd4`,
verifies the flashed bytes, then reboots.

## 2. Linux Shell With Network

Use this when Linux boots far enough to provide a telnet/serial shell, but the
normal runtime or SSH is broken.

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

Replace `192.168.1.100` with your computer's IP. Confirm the MD5 before writing
flash.

## 3. Linux Shell Without Network

If Linux boots but network is unusable, fix `/mnt/mtd/wpa_supplicant.conf` if
possible and reboot.

If you cannot transfer a known-good image into `/tmp/update.img`, use U-Boot
recovery instead.

## 4. U-Boot Recovery

Use this when Linux does not boot far enough for a shell.

Serial console is required. See [Serial TTL](serial_ttl.md) for wiring and
terminal settings.

Power the board and press Enter immediately. The U-Boot input window is about
one second after power is applied.

The `/usr` partition is `mtd4`:

```text
offset: 0x00460000
size:   0x00b10000
```

From U-Boot, transfer the image with YMODEM and write it to flash:

```sh
mw.b 0xC1000000 ff 00b10000
sf probe
loady 0xC1000000
sf erase 0x00460000 00b10000
sf write 0xC1000000 0x00460000 00b10000
reset
```

The image must fit in `mtd4` (`0x00b10000`, 11,599,872 bytes). Release images
are built to fit this partition.

## After Recovery

After booting, verify:

```sh
cat /usr/etc/wibox-release
dmesg | grep -i -E 'cramfs|decompress|error'
tail -80 /var/log/wibox-media-daemon.log
```

If WiFi does not come up, check:

```sh
cat /mnt/mtd/wpa_supplicant.conf
ifconfig -a
route -n
```
