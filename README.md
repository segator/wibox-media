# WiBox Media

Custom firmware for the Fermax WiBox GK7102S intercom. It replaces the vendor
cloud/app workflow with a local SIP media daemon so the doorphone can be used
from a SIP client and Home Assistant.

The firmware provides:

- SIP audio calls with PCMA RTP;
- optional H.264 D1 video from the main encoder;
- DTMF `#` and MQTT door unlock;
- Home Assistant discovery over MQTT;
- Prometheus metrics;
- firmware update checks and installs from Home Assistant or the WiBox shell.

The runtime binary is `/usr/bin/wibox-media-daemon`.

![WiBox Home Assistant device](docs/img/homeassistant.png)

## Credits

This project builds on prior reverse engineering work:

- [`duhow/wibox`](https://github.com/duhow/wibox) for the original WiBox
  firmware patching, installation and recovery groundwork.
- [`Conclusio/wibox-audio`](https://github.com/Conclusio/wibox-audio) for the
  tracing work and audio bridge that made the direct audio integration possible.

## Current Release

<!-- x-release-please-start-version -->
[v0.5.2](https://github.com/segator/wibox-media/releases/tag/v0.5.2) <!-- x-release-please-version -->
<!-- x-release-please-end -->

```bash
VERSION="v0.5.2"  # x-release-please-version
wget -O wibox-media.img "https://github.com/segator/wibox-media/releases/download/${VERSION}/wibox-media-${VERSION}.img"
```

Run that command on your computer, not on the stock WiBox. The stock WiBox
`wget` cannot download GitHub HTTPS release assets. For first installation,
download the image on your computer and transfer it to the WiBox with `nc` as
shown in [Getting Started](docs/getting_started.md).

You do not need to build from source unless you are developing the firmware.

## Supported Devices

Tested WiBox stock firmware:

- `V500.R001.A103.00.G0021.B007`
- `V500.R001.A103.00.G0021.B010`

`V500.R001.A103.00.G0021.B013` blocks telnet, so first access requires serial.
Newer versions should also be treated as serial-only until proven otherwise.

## User Journey

1. Read [Getting Started](docs/getting_started.md) before touching a stock
   device. It covers access, backup, WiFi persistence, first flash and first
   boot.
2. If telnet is unavailable or the device does not boot, use
   [Serial TTL](docs/serial_ttl.md) and [Recovery](docs/recovery.md).
3. After first boot, configure `/mnt/mtd/sip_media.conf` for SIP, MQTT,
   optional video, Prometheus and firmware updates.
4. Add the discovered MQTT device in Home Assistant. Normal daily control is
   through Home Assistant: media state, open door, video enable/disable and
   firmware update buttons.
5. Future updates should use [Firmware Updates](docs/updates.md). The WiBox
   includes its own HTTPS-capable updater because stock `wget` cannot download
   GitHub release assets.

## Configuration Summary

Persistent config lives at:

```text
/mnt/mtd/sip_media.conf
```

Minimal example:

```ini
outgoing_call_target=sip:1000@192.168.0.31:5060
mqtt_enabled=1
mqtt_host=192.168.0.203
mqtt_user=wibox
mqtt_pass=change-me
video_enabled=1
firmware_update_enabled=1
prometheus_enabled=1
```

Set `video_enabled=0` for audio-only installations.

## Runtime Model

At boot, `run.sh` initializes hardware, WiFi and SSH, runs a short Sofia warmup
for the video subsystem, then starts `wibox-media-daemon` under
`app_watchdog.sh`.

The daemon owns:

- `/dev/ttySGK1` intercom serial events and commands;
- SIP signaling and call lifecycle;
- direct GADI audio hardware;
- D1 H.264 video worker lifecycle;
- MQTT/Home Assistant discovery and state;
- firmware update checks and install requests;
- Prometheus `/metrics` on port `9617` by default.

Sofia is still required once per boot as a hardware warmup. It is not required
per call.

## Documentation

- [Getting Started](docs/getting_started.md): stock-to-custom install path.
- [Recovery](docs/recovery.md): SSH, shell and U-Boot recovery paths.
- [Serial TTL](docs/serial_ttl.md): wiring and U-Boot entry.
- [Runtime](docs/sip_media.md): SIP, media, MQTT, local test API and metrics.
- [Firmware Updates](docs/updates.md): Home Assistant and terminal update flow.
- [Architecture](docs/architecture.md): current boot/runtime layout.
- [Hardware Reference](docs/system.md): flash layout, boot notes and serial
  devices.
- [UART Codes](docs/codes.md): known `/dev/ttySGK1` frames.
- [D1 Video Capture](docs/d1_video_capture.md): low-level H.264 capture notes.

Reverse-engineering logs remain in `research/` for contributors; they are not
needed for normal installation.

## Development

Build and verify locally:

```bash
make docker
make build
make verify
```

Useful development commands:

```bash
make build-media       # rebuild wibox-media-daemon and firmware_update
make verify-image      # inspect release/latest contents
make deploy-runtime    # upload current daemon to /tmp on a running WiBox
make verify-device     # verify runtime + MQTT against a running WiBox
make device-status     # show runtime status and recent logs
```

See [Development](docs/development.md) for build details and release notes.
