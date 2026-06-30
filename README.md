# WiBox Media

Custom firmware for the Fermax WiBox GK7102S intercom.

This project replaces the vendor cloud/web workflow with a local media runtime:

- answer and place intercom calls over SIP;
- send and receive audio over RTP/PCMA;
- send D1 H.264 video over RTP when the intercom has video support;
- open the door with DTMF `#`;
- expose the intercom to MQTT/Home Assistant.

The main runtime is `wibox-media-daemon`. It owns SIP signaling, RTP
audio/video, `/dev/ttySGK1` intercom serial events, DTMF, MQTT discovery/state
and door unlock logic.

Sofia, the original vendor app, is still used once at boot as a short hardware
warmup for the GK7102 video pipeline. It is not started per call.

## Credits

This project builds on earlier hard work from:

- [`duhow/wibox`](https://github.com/duhow/wibox), for the original WiBox
  firmware patching, installation and recovery groundwork.
- [`Conclusio/wibox-audio`](https://github.com/Conclusio/wibox-audio), for the
  tracing work and audio base that made the current audio integration possible
  and gave us the path to add video.

## Repository Layout

```text
src/sip_media/                  wibox-media-daemon source
src/sip_media/video_worker.c    D1 H.264 RTP video worker
src/sip_media/audio_hw.c        direct GADI audio hardware bridge
include/                        files copied into the generated /usr image
scripts/                        build, deploy, verify and flash tooling
docs/                           architecture, hardware and research docs
research/                       reverse-engineering notes and Sofia traces
mtd4                            factory /usr cramfs backup, local only
release/latest                  generated firmware image
```

## Supported Starting Point

This firmware is intended for Fermax WiBox GK7102S units.

Network-based first installation has only been tested from these vendor
firmwares:

```text
V500.R001.A103.00.G0021.B007
V500.R001.A103.00.G0021.B010
```

`V500.R001.A103.00.G021.B013` is known to block telnet, so there is no network
shell path from a stock device. Use [serial recovery](#2-prepare-serial-access)
for B013 or newer firmware.

## What You Need

On your computer:

- Linux shell.
- Docker.
- This repository.
- A factory `mtd4` backup saved as `./mtd4`.
- Goke SDK at:

```text
$HOME/config/GK710X_LinuxSDK_v2.0.0
```

On the stock WiBox:

- Telnet access on B007/B010, or serial access for B013/newer.
- WiFi credentials ready for `/mnt/mtd/wpa_supplicant.conf`.

Safety notes:

- Unplug the WiBox before opening the case or attaching serial wires.
- Do not solder while the board is powered.
- Serial recovery is recommended before first flash, but it is not required for
  B007/B010 if telnet works. It becomes required if the device cannot be
  reached over the network after flashing.

## Fresh Device Journey

The first install has shared preparation steps, then two possible install
transports:

- **Network install** for tested B007/B010 units where telnet works.
- **Serial install** for B013/newer, units without telnet, or recovery cases.

### 1. Choose Access Method

B007/B010 stock firmware normally exposes telnet:

```text
user: root
pass: qv2008
```

Older Sofia console sessions may use:

```text
user: root
pass: aszeno
```

If telnet works, use the network install transport later in this guide. If login
fails, or the unit is B013/newer, use serial.

Serial terminal options:

```bash
picocom -b 115200 /dev/ttyUSB0
```

Or:

```bash
minicom -s
```

Serial settings: `115200` baud, hardware flow control disabled. The WiBox
console is `ttySGK2`.

| WiBox board | USB TTL adapter |
|-------------|-----------------|
| GND         | GND             |
| TX          | RX              |
| RX          | TX              |

![Serial connector](docs/img/serial.jpg)
![WiBox board](docs/img/board.jpg)
![WiBox back board](docs/img/backboard.jpg)

### 2. Back Up the Factory Flash

At minimum, keep a copy of `mtd4`, the `/usr` cramfs partition. This repository
builds the custom image by extracting that partition and replacing selected
files.

On a stock device there is no SSH/dropbear yet, so use `nc`. Start a receiver
on your computer:

```bash
for i in $(seq 0 6); do
  nc -l -p 8888 > "mtd${i}"
done
```

Then run on the WiBox, replacing the IP:

```sh
PC_IP=192.168.1.100
for i in $(seq 0 6); do
  dd if=/dev/mtd${i} bs=4096 | nc "${PC_IP}" 8888
  sleep 1
done
```

The file needed by this project is `mtd4`.

After the custom firmware is running, future backups can use:

```bash
make backup-mtd4
```

### 3. Configure Persistent WiFi

This step is critical. Configure it before flashing the custom image.

If `/mnt/mtd/wpa_supplicant.conf` is missing or wrong, the custom firmware will
not be able to join your WiFi network after reboot. You will lose network access
and will need serial access to fix it.

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

The generated firmware uses this file to bring WiFi back after boot.

### 4. Build the Firmware on Your Computer

Build the project Docker image:

```bash
make docker
```

This creates `wibox-build-tool:latest` from the existing `wibox-build:latest`
base image. The base image provides the ARM toolchain, PJProject and SDK build
environment; this repository adds statically built `cramfsck/mkcramfs` using
Ubuntu 16.04 zlib 1.2.8.

Build the daemon and final `/usr` cramfs image:

```bash
make build
```

Output:

```text
release/image-YYMMDD-HHMM
release/latest -> image-YYMMDD-HHMM
```

### 5. Verify the Local Image

Before the first flash, only local verification is possible:

```bash
make test
make verify-image
```

After the custom firmware is running, full device verification is available:

```bash
make verify-device
make verify
```

### 6. Configure SIP, Video and MQTT

The runtime config lives on the WiBox:

```text
/mnt/mtd/sip_media.conf
```

If the file does not exist, first boot copies:

```text
/etc/sip_media.conf.default
```

Important settings:

```ini
outgoing_call_target=sip:1000@192.168.0.31:5060
outgoing_call_timeout=60
sip_port=5060
rtp_port=8000

video_enabled=1
video_rtp_port=8002
video_payload_type=96

serial_listener_enabled=1
intercom_device=/dev/ttySGK1

mqtt_enabled=1
mqtt_host=192.168.10.2
mqtt_user=mqtt
mqtt_pass=password
mqtt_homeassistant_prefix=homeassistant
```

Set `video_enabled=0` for intercom installations without video support.

Do not commit real MQTT credentials. Store them only on the device.

### 7. Install The First Custom Image

Use one of the two transports below:

- [Option A: Network install with telnet and nc](#option-a-network-install-with-telnet-and-nc)
- [Option B: Serial install](#option-b-serial-install)

#### Option A: Network Install With Telnet And nc

Use this on tested B007/B010 stock firmware when telnet works.

On your computer, serve the generated image:

```bash
nc -l -p 8888 < release/latest
```

On the WiBox, download it and write it to the `/usr` partition:

```sh
PC_IP=192.168.1.100
nc "${PC_IP}" 8888 > /tmp/update.img
dd if=/tmp/update.img of=/dev/mtdblock4 bs=4096
sync
reboot
```

The stock image is not expected to have our guarded updater. After the custom
firmware is installed, use the later-update tooling instead of raw `dd`.

#### Option B: Serial Install

Use this for B013/newer firmware, units without telnet, or recovery cases.

The goal is to place `release/latest` on the WiBox as `/tmp/update.img`, then
run the updater.

If Linux boots to a serial shell, use a terminal file-transfer feature. With
`rx` available on the WiBox:

```sh
cd /tmp
rx update.img
```

Then, from `minicom`, use the send-file menu and send `release/latest` with
XMODEM. Some terminal programs also support YMODEM; either is fine as long as
it creates `/tmp/update.img`.

After the transfer finishes, write it to the `/usr` partition:

```sh
ls -lh /tmp/update.img
dd if=/tmp/update.img of=/dev/mtdblock4 bs=4096
sync
reboot
```

The stock image is not expected to have our guarded updater. After the custom
firmware is installed, use the later-update tooling instead of raw `dd`.

If Linux does not boot far enough for a shell, use the
[U-Boot recovery](#3-recovery-via-u-boot) steps. To enter U-Boot, connect the
serial console, power the board, and press Enter immediately; the bootloader
wait window is about one second after power is applied.

### 8. Later Flashes With Custom Firmware

After this firmware is installed, dropbear SSH is available and future updates
should use the guarded tooling:

```bash
make flash-dry-run
make backup-mtd4
make flash CONFIRM_FLASH=YES
reboot
```

`make flash-dry-run` builds the image, uploads or reuses `/tmp/update.img`,
checks hashes and stops before writing flash. `make flash` automatically runs
`backup-mtd4` before writing.

## Runtime Behavior

Boot sequence:

```text
/usr/run.sh
  -> mount persistent config
  -> setup GPIO and LEDs
  -> start SSH
  -> load kernel modules
  -> bring up WiFi
  -> start cron and heartbeat
  -> run Sofia_temp.sh once for video hardware warmup
  -> create /mnt/mtd/sip_media.conf if missing
  -> start app_watchdog.sh wibox-media-daemon /usr/bin/wibox-media-daemon
  -> run /mnt/mtd/post.sh if executable
  -> set final LED state
```

`app_watchdog.sh` restarts the daemon if it exits and writes:

```text
/var/log/wibox-media-daemon.log
```

`/mnt/mtd/post.sh` is a local boot hook for site-specific extras. Keep it small
and do not start Sofia or another media runtime from it.

## Calling and Door Unlock

When the panel reports a ring on `/dev/ttySGK1`, `wibox-media-daemon` can place
an outgoing SIP call to `outgoing_call_target`.

When a SIP call is established:

- the daemon starts the intercom call path;
- direct GADI audio starts;
- D1 H.264 RTP video starts if negotiated and `video_enabled=1`;
- DTMF `#` opens the door.

DTMF over RTP telephone-event is supported, and SIP INFO is accepted as a
fallback.

## Home Assistant

The daemon publishes MQTT discovery and state directly. No `mosquitto_pub` or
`mosquitto_sub` binaries are needed.

Main entities:

```text
button.open_door
binary_sensor.ringing
binary_sensor.call_active
binary_sensor.sip_call_active
binary_sensor.video_active
sensor.media_state
sensor.last_ring
sensor.last_unlock
sensor.wifi_rssi
switch.video_enabled
```

The door command is high-level:

```text
wibox/<device>/door/open/set = PRESS
```

Home Assistant should not need to orchestrate `START_CALL`, unlock and
`STOP_CALL`; the daemon handles that sequence.

## LEDs

Current LED ownership remains in `run.sh`:

```text
red    booting or WiFi failure
off    WiFi setup in progress
green  WiFi associated and DHCP succeeded
blue   application boot finished
```

Runtime call-state LEDs are intentionally not implemented yet. If added later,
they should live in the daemon with an explicit priority model and physical
verification.

## Recovery

Use recovery methods in this order.

### 1. Recovery Over SSH

Use this if the device still boots and SSH works.

```bash
make flash-dry-run
make flash CONFIRM_FLASH=YES
reboot
```

To restore a previous backup, point `release/latest` or `IMAGE` to the backup
image before flashing, or manually upload it as `/tmp/update.img` and run the
device updater.

### 2. Recovery Via Shell

Use this if the custom firmware is already installed, Linux boots, but WiFi or
normal startup is broken and you have serial shell access.

Use `picocom`, `minicom`, or an equivalent serial terminal at `115200` baud with
hardware flow control disabled.

On the WiBox:

```sh
cd /tmp
rx /tmp/update.img
```

From your terminal program, send `release/latest` with XMODEM/YMODEM support.
After transfer:

```sh
/usr/bin/update_firmware.sh
reboot
```

If `/usr/bin/update_firmware.sh` is not available, write `mtd4` manually only
as a last resort:

```sh
dd if=/tmp/update.img of=/dev/mtdblock4 bs=4096
sync
```

### 3. Recovery Via U-Boot

Use this when Linux does not boot far enough to get a shell.

Connect with `picocom`, `minicom`, or an equivalent serial terminal at `115200`
baud with hardware flow control disabled. Power the board and press Enter
immediately; U-Boot only waits for about one second after power is applied.

From U-Boot:

```sh
mw.b 0xC1000000 ff 00b10000
sf probe
loady 0xC1000000
sf erase 0x00460000 00b10000
sf write 0xC1000000 0x00460000 00b10000
reset
```

Send `release/latest` via YMODEM when `loady` waits for the file.

## Research Notes

The current D1 video path is documented in:

- [`docs/d1_video_capture.md`](docs/d1_video_capture.md)
- [`docs/sip_media.md`](docs/sip_media.md)
- [`docs/architecture.md`](docs/architecture.md)

The standalone D1 capture PoC was removed after the SIP media daemon absorbed
the working video path. Reverse-engineering traces are kept under `research/`.
They are not part of the production runtime.
