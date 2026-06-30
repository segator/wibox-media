# Installation Guide

Read [`README.md`](README.md) first. This is the short checklist.

## 1. Get A Shell

Choose the access method before doing anything else:

| Stock firmware / condition | Access method |
|----------------------------|---------------|
| B007 or B010 and telnet works | Telnet |
| B013 or newer | Serial |
| Telnet does not work | Serial |
| Device no longer boots normally | U-Boot recovery |

Network-based first installation has only been tested from:

```text
V500.R001.A103.00.G0021.B007
V500.R001.A103.00.G0021.B010
```

B013 blocks telnet.

Telnet credentials usually are:

```text
root / qv2008
```

Some Sofia console sessions use:

```text
root / aszeno
```

For serial, use `picocom` or `minicom` at `115200` baud with hardware flow
control disabled:

```bash
picocom -b 115200 /dev/ttyUSB0
```

Unplug the WiBox before opening the case or attaching serial wires. Do not
solder while the board is powered.

## 2. Back Up Flash

The build needs the factory `/usr` partition as `./mtd4`.

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

Copy `mtd4` into the repository root.

## 3. Configure Persistent WiFi

This is required before flashing. If this file is missing or wrong, the custom
firmware will not reconnect to WiFi after reboot and you will need serial
access to fix it.

Create `/mnt/mtd/wpa_supplicant.conf`:

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

## 4. Build

```bash
make docker
make build
make test
make verify-image
```

Final image:

```text
release/latest
```

## 5. First Flash

### Telnet Shell

On your computer:

```bash
nc -l -p 8888 < release/latest
```

On the WiBox shell:

```sh
PC_IP=192.168.1.100
nc "${PC_IP}" 8888 > /tmp/update.img
dd if=/tmp/update.img of=/dev/mtdblock4 bs=4096
sync
reboot
```

### Serial Shell

Transfer `release/latest` to `/tmp/update.img`.

If `rx` is available:

```sh
cd /tmp
rx update.img
```

Send `release/latest` from `minicom` using XMODEM, then write it:

```sh
ls -lh /tmp/update.img
dd if=/tmp/update.img of=/dev/mtdblock4 bs=4096
sync
reboot
```

## 6. Later Updates With Custom Firmware

After the custom firmware is installed, SSH is available:

```bash
make deploy-runtime
make verify-device
make flash-dry-run
make backup-mtd4
make flash CONFIRM_FLASH=YES
reboot
```

## 7. Recovery Via Shell

Use this when the custom firmware is already installed, Linux boots and serial
shell works.

Transfer `release/latest` to `/tmp/update.img`, then run:

```sh
/usr/bin/update_firmware.sh
reboot
```

If the updater is unavailable, write manually:

```sh
dd if=/tmp/update.img of=/dev/mtdblock4 bs=4096
sync
reboot
```

## 8. Recovery Via U-Boot

Use this when Linux does not boot far enough for a shell.

Connect the serial terminal, power the board, and press Enter immediately.
U-Boot waits for input for about one second after power is applied.

```sh
mw.b 0xC1000000 ff 00b10000
sf probe
loady 0xC1000000
sf erase 0x00460000 00b10000
sf write 0xC1000000 0x00460000 00b10000
reset
```

Send `release/latest` via YMODEM when `loady` waits for the file.
