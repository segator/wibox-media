# WiBox Media Firmware

Custom firmware image for the Fermax WiBox GK7102S intercom. The production
runtime is `wibox-media-daemon`: one process owns SIP, RTP audio/video, serial
intercom state, DTMF door unlock and MQTT/Home Assistant integration.

Sofia is still used only as a short boot warmup for the GK7102 video pipeline.
It is not started per call.

## Prerequisites

- Factory `mtd4` backup copied to this repository as `./mtd4`.
- Goke SDK at `$HOME/config/GK710X_LinuxSDK_v2.0.0`, or set `SDK_DIR`.
- Docker.
- Existing local `wibox-build:latest` base image. `make docker` wraps it with
  the pinned cramfs tooling used to generate the final image.

## Build

Build the single project Docker image:

```bash
make docker
```

This creates `wibox-build-tool:latest`. It contains the ARM toolchain,
PJProject, SDK build environment, and statically built `cramfsck/mkcramfs`
from Ubuntu 16.04 zlib 1.2.8.

Build the daemon and final cramfs image:

```bash
make build
```

Output:

```text
release/image-YYMMDD-HHMM
release/latest -> image-YYMMDD-HHMM
```

`make build` performs the full flow:

```text
scripts/build_wibox_media_daemon.sh
  -> docker run wibox-build-tool:latest
  -> compile src/sip_media as include/bin/wibox-media-daemon
  -> copy libap.so from SDK

docker run wibox-build-tool:latest make build-inside
  -> extract ./mtd4 into ./cramfs
  -> move original run.sh to run-orig.sh
  -> copy include/ into the rootfs
  -> pack release/latest with mkcramfs
```

## Verify

```bash
make verify
```

This runs the host MQTT regression test, verifies `release/latest` contents,
then checks the active WiBox runtime and MQTT retained state.

For a non-persistent runtime test before flashing:

```bash
make deploy-runtime
```

Connection settings can be overridden:

```bash
make deploy-runtime WIBOX_IP=192.168.0.196 WIBOX_USER=root WIBOX_PASS=qv2008
```

## Install

Non-destructive flash check:

```bash
make flash-dry-run
```

Backup current `/usr` partition:

```bash
make backup-mtd4
```

Flash the generated image:

```bash
make flash CONFIRM_FLASH=YES
reboot
```

`make flash` automatically runs `backup-mtd4` first. The flash script uploads
or reuses `/tmp/update.img`, verifies local and remote hashes, confirms `mtd4`
is mounted as `/usr`, then calls `/usr/bin/update_firmware.sh`.

## Configuration

On first boot, `/etc/sip_media.conf.default` is copied to:

```text
/mnt/mtd/sip_media.conf
```

Important keys:

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
mqtt_host=127.0.0.1
mqtt_user=
mqtt_pass=
mqtt_homeassistant_prefix=homeassistant
mqtt_base_topic=
mqtt_device_id=
mqtt_device_name=
```

Do not commit real MQTT credentials. Configure them directly on the device in
`/mnt/mtd/sip_media.conf`.

## Boot Runtime

`/usr/run.sh` performs the production boot:

```text
mount persistent config
setup GPIO and boot LEDs
start SSH
load kernel modules
bring up WiFi
start cron and heartbeat
run Sofia_temp.sh once for video hardware warmup
create /mnt/mtd/sip_media.conf if missing
start app_watchdog.sh wibox-media-daemon /usr/bin/wibox-media-daemon
run /mnt/mtd/post.sh if it exists and is executable
set final LED state
```

`/mnt/mtd/post.sh` is a local boot hook for site-specific extras that should
survive firmware updates. Keep it small and do not start Sofia, `sip_media`, or
another media daemon from it.

`app_watchdog.sh` supervises the daemon. If it exits, it writes
`/var/log/wibox-media-daemon.log`, rotates that log, waits a few seconds and
starts the daemon again.

## GPIO and LEDs

`gpio.sh` is still a boot script, not daemon logic. It exports the board GPIOs
early enough for WiFi and service startup:

```text
GPIO 10: red LED
GPIO 11: blue LED
GPIO 12: green LED
GPIO 18: audio chip enable, default high
GPIO 19: board line initialized low
GPIO 34: board line initialized low
```

Current LED workflow:

```text
red   booting or WiFi failure
off   WiFi setup in progress
green WiFi associated and DHCP succeeded
blue  application boot finished
```

The daemon controls intercom/audio/video state, but LED ownership remains in
`run.sh` until we define a richer runtime LED policy.

## Debugging

`research/` keeps Sofia traces and reverse-engineering notes. `sofia_trace` is
not packaged in the production image. If ioctl tracing is needed again, build
or copy the tracer manually for that investigation rather than enabling it in
normal boot.

## More Docs

- [`docs/architecture.md`](docs/architecture.md)
- [`docs/sip_media.md`](docs/sip_media.md)
- [`docs/d1_video_capture.md`](docs/d1_video_capture.md)
