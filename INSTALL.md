# Install WiBox Media Firmware

This project replaces the WiBox `/usr` cramfs partition (`mtd4`) with a custom
image that boots `wibox-media-daemon`.

Serial recovery access is strongly recommended before flashing.

## Prepare

Required local files and tools:

```text
./mtd4                                      factory /usr cramfs backup
$HOME/config/GK710X_LinuxSDK_v2.0.0         Goke SDK
Docker
```

Build the project Docker image:

```bash
make docker
```

Build and verify the firmware image:

```bash
make build
make verify
```

The final image is:

```text
release/latest
```

## Configure WiFi

The firmware expects WiFi configuration in the persistent partition. Create or
update `/mnt/mtd/wpa_supplicant.conf` on the device:

```ini
ctrl_interface=/var/run/wpa_supplicant
ap_scan=1

network={
        ssid="CHANGE_WIFI_NAME"
        psk="CHANGE_WIFI_PASSWORD"
        scan_ssid=1
        key_mgmt=WPA-PSK
}
```

## Configure Media/MQTT

Runtime config lives on the device:

```text
/mnt/mtd/sip_media.conf
```

If it does not exist, the first boot copies `/etc/sip_media.conf.default`.

Example MQTT settings:

```ini
mqtt_enabled=1
mqtt_host=192.168.10.2
mqtt_user=mqtt
mqtt_pass=password
```

Do not commit real credentials to the repository.

## Test Without Flashing

Upload the current daemon to `/tmp` and restart it:

```bash
make deploy-runtime
make verify-device
```

## Flash Over SSH

Run the non-destructive dry run first:

```bash
make flash-dry-run
```

Create a verified backup of current `/usr`:

```bash
make backup-mtd4
```

Flash:

```bash
make flash CONFIRM_FLASH=YES
reboot
```

`make flash` runs `backup-mtd4` automatically. Backups are written to
`backups/` and ignored by git.

## Serial Recovery

The serial console is `ttySGK2` at `115200`, no hardware flow control.

| Board | TTL |
|-------|-----|
| GND   | GND |
| TX    | RX  |
| RX    | TX  |

If U-Boot is available, load `release/latest` via YMODEM and write it to the
`mtd4` offset:

```sh
mw.b 0xC1000000 ff 00b10000
sf probe
loady 0xC1000000
sf erase 0x00460000 00b10000
sf write 0xC1000000 0x00460000 00b10000
reset
```
