# WiBox Media

Custom firmware for the Fermax WiBox GK7102S intercom.

It replaces the vendor Sofia-driven flow with a local media runtime that can:

- answer and place SIP calls;
- carry audio and optional H.264 video;
- open the door with DTMF `#`;
- publish state to MQTT / Home Assistant;
- expose a local firmware update command on the WiBox.

The main runtime is `wibox-media-daemon`.

## Start Here

1. Read [Getting Started](docs/getting_started.md).
2. If you need serial wiring, read [Serial TTL](docs/serial_ttl.md).
3. If the device does not boot, read [Recovery](docs/recovery.md).
4. If you want the runtime layout, read [Architecture](docs/architecture.md).
5. If you want MQTT entity details, read [SIP Media](docs/sip_media.md).

## Current Release

<!-- x-release-please-start-version -->
v0.4.6
<!-- x-release-please-end -->

Download the latest release image:

```bash
VERSION="v0.4.6"  # x-release-please-version
wget -O wibox-media.img "https://github.com/aymerici/wibox-media/releases/download/${VERSION}/wibox-media-${VERSION}.img"
```

## Credits

- [`duhow/wibox`](https://github.com/duhow/wibox) for the original firmware
  patching, installation and recovery groundwork.
- [`Conclusio/wibox-audio`](https://github.com/Conclusio/wibox-audio) for the
  tracing work and audio base that made the current integration possible.

## Build And Test

```bash
make docker
make build
make test
make verify-image
```

If you already have the firmware running on the WiBox:

```bash
make verify-device
make verify
```
