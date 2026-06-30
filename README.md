# WiBox Custom Firmware Builder

Build custom firmware for the Fermax WiBox (GK710x SoC) intercom.

## Prerequisites

- **Factory `mtd4` backup** — copy it as `mtd4` in this directory.
  This file contains `/usr/` (cramfs): Sofia binary, WiFi binaries, etc.
  The build extracts it, applies our patches, and repacks it.
- **Docker** — to build the toolchain and firmware in a reproducible environment.

## Build

### Step 1 — Build Docker build-tool (one-time)

```bash
make docker
```

This compiles `cramfs-tools` statically against zlib 1.2.8 (Ubuntu 16.04), ensuring the
cramfs output is compatible with the GK710x kernel (3.4).

### Step 2 — Build firmware image

```bash
make build
```

This runs inside Docker:
1. Extracts factory `mtd4` → `cramfs/`
2. Applies patch scripts (dropbear, GPIO, custom run.sh, etc.)
3. Packs everything back into a cramfs image

Output: `release/image-YYMMDD-HHMM` (~4.8 MB, must fit within the 11 MB mtd4 partition).

### Runtime test on WiBox

Before flashing, deploy only the daemon to `/tmp` and restart it with the
persistent `/mnt/mtd/sip_media.conf`:

```bash
make deploy-runtime
make verify
```

`verify` runs the host regression test, checks the active WiBox daemon checksum,
and validates retained MQTT discovery/state using `/mnt/mtd/sip_media.conf`
from the device. You can also verify retained MQTT discovery and state manually
with:

```bash
MQTT_HOST=192.168.10.2 MQTT_USER=mqtt MQTT_PASS=password make verify-mqtt
```

Override connection settings when needed:

```bash
make deploy-runtime WIBOX_IP=192.168.0.196 WIBOX_USER=root WIBOX_PASS=qv2008
```

## What's inside the firmware

| Component | Description |
|-----------|-------------|
| **run.sh** | Custom boot script: GPIO setup, WiFi, SSH (dropbear), Sofia warmup, media daemon |
| **sofia_trace** | Ptrace wrapper — catches SEGVs + logs all ioctls to `/mnt/mtd/iotrace_boot.log` |
| **dropbear** | SSH server for remote access |
| **wibox-media-daemon** | SIP, RTP audio/video, serial intercom state, DTMF, MQTT/Home Assistant |
| **gpio.sh, heartbeat.sh** | WiBox service scripts |

## Flash to WiBox

### Normal flash (WiFi/SSH working)

Use the guarded Make target. It uploads `release/latest` to `/tmp/update.img`,
checks the local and remote hashes, confirms `mtd4` is the `/usr` partition, then
runs `/usr/bin/update_firmware.sh` on the device.

```bash
make flash-dry-run
make flash CONFIRM_FLASH=YES
reboot
```

`make flash-dry-run` performs the upload and all non-destructive checks, then
stops before writing mtd4. Do not use raw `dd` for normal updates; the updater
verifies the written image.

### Recovery flash (U-Boot via serial YMODEM)

```
mw.b 0xC1000000 ff 00b10000
sf probe
loady 0xC1000000
# → send release/latest via YMODEM from your terminal
sf erase 0x00460000 00b10000
sf write 0xC1000000 0x00460000 00b10000
reset
```

## Sofia IOCTL Tracing

Sofia runs under `sofia_trace` which:
- Catches SEGV crashes (skips faulting instruction, Sofia stays alive)
- Logs **every ioctl** call (fd, cmd, arg, return value) to `/mnt/mtd/iotrace_boot.log`

To capture a trace:
1. Flash firmware → boot → Sofia auto-starts under tracer
2. From the app, start/stop video call to capture VI+VENC streaming ioctls
3. Download trace: `nc` or `scp` from `/mnt/mtd/iotrace_boot.log`

## D1 Video Capture

The current working D1 H.264 capture flow is documented in
[`docs/d1_video_capture.md`](docs/d1_video_capture.md).

Current summary:
- Sofia warmup is still required once per boot, not per call.
- Per call, enable the MCU call path with `/dev/ttySGK1`, capture `stream_id==0`,
  then send the stop-call command.
- Verified output is H.264 Main profile at 688x576.

## SIP Media + Video

The imported audio stack and first audio+video RTP integration are documented in
[`docs/sip_media.md`](docs/sip_media.md).

The production migration plan is tracked in
[`docs/architecture.md`](docs/architecture.md).

## Clean build artifacts

```bash
make clean
```

Removes extracted cramfs, patch.log, and compiled host binaries.
