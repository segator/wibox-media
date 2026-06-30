# Installation Guide

Read [`README.md`](README.md) first. It explains the project purpose, supported
firmware versions, build flow and normal flashing path.

This file is the focused installation and recovery checklist.

## 1. Confirm Device Access

Network-based first installation has only been tested from:

```text
V500.R001.A103.00.G0021.B007
V500.R001.A103.00.G0021.B010
```

`V500.R001.A103.00.G021.B013` blocks telnet. For B013 or newer, use serial.

Default shell credentials when available:

```text
root / qv2008
```

Some Sofia console sessions use:

```text
root / aszeno
```

## 2. Prepare Serial Recovery

Unplug the WiBox before opening the case or attaching serial wires. Do not
solder while the board is powered.

Use a serial terminal such as `picocom` or `minicom`. Configure it for `115200`
baud and disable hardware flow control.

```bash
picocom -b 115200 /dev/ttyUSB0
```

Or:

```bash
minicom -s
```

| WiBox board | USB TTL adapter |
|-------------|-----------------|
| GND         | GND             |
| TX          | RX              |
| RX          | TX              |

![Serial connector](docs/img/serial.jpg)

If no boot messages appear, set the console in U-Boot:

```sh
setenv consoledev 'ttySGK0'
saveenv
reset
```

## 3. Back Up Flash

The build needs the factory `/usr` partition:

```text
./mtd4
```

On your computer:

```bash
for i in $(seq 0 6); do
  nc -l -p 8888 > "mtd${i}"
done
```

On the WiBox:

```sh
PC_IP=192.168.1.100
for i in $(seq 0 6); do
  dd if=/dev/mtd${i} bs=4096 | nc "${PC_IP}" 8888
  sleep 1
done
```

## 4. Configure Persistent WiFi

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

## 5. Build

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

## 6. First Flash From Stock Firmware

Stock firmware does not have SSH/dropbear, so upload `release/latest` with
`nc`.

On your computer:

```bash
nc -l -p 8888 < release/latest
```

On the WiBox:

```sh
PC_IP=192.168.1.100
nc "${PC_IP}" 8888 > /tmp/update.img
/usr/bin/update_firmware.sh
reboot
```

If the updater is unavailable:

```sh
dd if=/tmp/update.img of=/dev/mtdblock4 bs=4096
sync
reboot
```

## 7. Later Updates With Custom Firmware

After the custom firmware is installed, SSH is available:

```bash
make deploy-runtime
make verify-device
make flash-dry-run
make backup-mtd4
make flash CONFIRM_FLASH=YES
reboot
```

## 8. Recovery Via Shell

Use this when Linux boots and serial shell works.

Use `picocom`, `minicom`, or an equivalent serial terminal at `115200` baud with
hardware flow control disabled.

Transfer `release/latest` to `/tmp/update.img`, then run:

```sh
/usr/bin/update_firmware.sh
reboot
```

If the updater is unavailable, manual write is the last resort:

```sh
dd if=/tmp/update.img of=/dev/mtdblock4 bs=4096
sync
```

## 9. Recovery Via U-Boot

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
