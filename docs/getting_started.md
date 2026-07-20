# Getting Started

This guide is the stock-to-custom install path for a WiBox GK7102S.

The first installation is different from later updates:

- stock firmware normally has telnet or serial, not SSH;
- stock `wget` cannot download GitHub HTTPS release assets;
- WiFi can be preconfigured before flashing or entered through the temporary
  provisioning access point after the first custom boot.

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

### First setup without serial

You do not need to create a WiFi file manually. If
`/mnt/mtd/wpa_supplicant.conf` is absent, the first custom boot automatically
starts the provisioning access point:

1. Wait for the status LED to blink blue. The normal boot and Sofia hardware
   warmup can take about one minute.
2. Join `IDS7938XXXX`, where `XXXX` is the last four characters of the WiBox
   Device ID.
3. Use the full 12-character Device ID printed on the WiBox label as the access
   point password.
4. Open `http://192.168.111.1/` in a browser. Disable mobile data temporarily if
   the phone avoids WiFi networks without Internet access.
5. Enter the destination network name and WPA/WPA2 password, then select
   **Save and restart**.
6. The LED turns green briefly, the access point disappears and the WiBox
   restarts in station mode. Find its new address in the router's DHCP leases.
   Dropbear SSH starts after the network transition.

### Reconfigure an installed WiBox

Hold the physical WiFi button for at least five seconds. When the request is
accepted, the WiFi LED turns off and the WiBox reboots into the same provisioning
access point. The existing credentials are preserved until **Save and restart**
successfully replaces them. **Return to saved Wi-Fi** leaves them unchanged and
restarts in station mode.

An unavailable router or an incorrect saved password does not automatically
force AP mode or a reboot. The WiBox stays in station mode and retries
association and DHCP; this avoids stranding a working installation after a
temporary router outage. Use the physical button when you intentionally need
to reconfigure it.

### Optional manual configuration

For a preconfigured first boot or serial recovery, create
`/mnt/mtd/wpa_supplicant.conf` on the WiBox:

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
VERSION="v0.18.0"  # x-release-please-version
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
sip_outgoing_call_enabled=1
outgoing_call_target=sip:1000@192.168.0.31:5060
mqtt_enabled=1
mqtt_host=192.168.0.203
mqtt_user=wibox
mqtt_pass=change-me
firmware_update_enabled=1
prometheus_enabled=1
```

Fresh installations default to audio-only: `video_enabled=0` and
`rtsp_enabled=0`. Enable video from Home Assistant when the doorphone has a
camera path. Enable RTSP only when you want a persistent Frigate/go2rtc/VLC
stream. With `rtsp_enabled=1` and `video_enabled=0`, RTSP stays audio-only; with
both enabled, RTSP serves H.264 D1 video plus PCMA audio. Raise
`video_bitrate_kbps` for less blocky video if your WiFi and SIP client can
handle the extra RTP bandwidth. `ring_snapshot_delay_ms` controls how long the
daemon waits after a physical ring before taking the automatic snapshot.

Without retained MQTT commands or file overrides, the built-in defaults are
`video_enabled=0`, `rtsp_enabled=0`, `video_bitrate_kbps=4096`,
`video_gop_n=25`, `video_idr_interval=1`, `video_brc_mode=0`,
`video_rtsp_periodic_idr_ms=0`, `outgoing_call_timeout=60` and
`ring_snapshot_delay_ms=2000`. These keys may be kept in the config file as boot
defaults, but the Home Assistant entities publish retained MQTT command values
for their exposed controls. Those retained values are replayed after reboot and
take priority over the file.
Resolution is intentionally not configurable yet; D1 `688x576` is the only
validated video mode.

`video_recording_enabled=0` is the safe default; at 4096 kbps, 30 seconds is
roughly 15 MiB and should only be written to `/tmp` if explicitly enabled for
testing.

Optional RTSP test stream:

```ini
rtsp_enabled=1
rtsp_port=8554
rtsp_auth_user=
rtsp_auth_pass=
```

The URL is `rtsp://<wibox-ip>:8554/live`. It serves RTP over RTSP/TCP
interleaved with PCMA audio, and H.264 video when `video_enabled=1`. SIP calls
attach to the same audio/video runtime while RTSP clients remain connected.
When video is enabled and no panel call is active, clients may see a blue/static
frame until the outside panel video path is opened by a ring or `START_CALL`.

Reboot after editing persistent config:

```sh
reboot
```

## 9. Home Assistant

When MQTT is configured, the daemon publishes Home Assistant discovery for:

- `Open Door` button;
- `Take Snapshot` button and `Snapshot` image;
- `Media State` sensor: `idle`, `ringing`, `established`;
- firmware version, commit and build timestamp sensors;
- `Door Unlocked` pulse binary sensor;
- `WiFi RSSI` sensor;
- `Video Enabled` switch;
- `Video Bitrate` and `Outgoing Call Timeout` controls;
- `Call Forward Enabled` switch;
- firmware update available/version sensors;
- firmware update refresh/install buttons.

Home Assistant should be the normal control surface after installation.

## 10. Doorbell Call Troubleshooting

If Home Assistant/SIP works when you trigger `DING` manually but real visitors
do not generate calls, check the physical WiBox intercom state before debugging
SIP.

The Fermax call-forward/redirect LED must be blue. Green means the physical
call-forward path is disabled, so the WiBox may not receive the outside-panel
call. Press the WiBox forward button or use the Home Assistant
`Call Forward Enabled` switch to toggle it back to blue.

The WiBox also has to be programmed on the VDS bus. If it is not paired with
the installation address, the indoor monitor can still ring while the WiBox sees
no `ALARM_REPORT` frame on `/dev/ttySGK1`. Re-run the Fermax PB2/VDS address
programming flow: put the WiBox in address programming mode with PB2, then press
the corresponding button on the physical indoor monitor so the WiBox stores the
same address.

A correctly detected outside-panel call appears in the daemon log as:

```text
UART code received: ALARM_REPORT [FB 11 00 1C]
DING detected from serial alarm
```

`PUSH_STATE_0` / `PUSH_STATE_1` only indicate the call-forward button state.
They are not the doorbell event.

## 11. Future Updates

Do not repeat the first-install `dd` flow for routine upgrades.

Use Home Assistant firmware update buttons or the on-device updater:

```sh
/usr/bin/firmware_update --status
/usr/bin/firmware_update
```

See [Firmware Updates](updates.md).
