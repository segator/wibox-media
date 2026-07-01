# Development

This page is for contributors building or testing the firmware from source.
Normal users should install a GitHub Release image.

## Build Tool

Build the Docker tool image once:

```bash
make docker
```

The firmware build uses `wibox-build-tool:latest`, which wraps the pinned build
environment needed for compatible cramfs/zlib behavior.

## Local Build

```bash
make build
```

This runs:

```text
prepare-base -> extracts ./mtd4 into ./cramfs when build libraries are needed
build-media  -> builds wibox-media-daemon and firmware_update
extract      -> refreshes ./cramfs from ./mtd4 for image packaging
patch        -> applies scripts/??_*.sh
pack         -> writes release/image-YYMMDD-HHMM and release/latest
```

The generated image is:

```text
release/latest
```

## Tests And Verification

Host regression tests:

```bash
make test
```

Image verification:

```bash
make verify-image
```

Full local verification:

```bash
make verify
```

`make verify` intentionally stays local: host tests plus image verification.

## Device Development

Against a running WiBox with custom firmware:

```bash
make deploy-runtime
make verify-device
make device-status
```

Defaults:

```text
WIBOX_IP=192.168.0.196
WIBOX_USER=root
WIBOX_PASS=qv2008
```

Override them on the command line:

```bash
make deploy-runtime WIBOX_IP=192.168.0.50 WIBOX_PASS=secret
```

`deploy-runtime` uploads the current daemon to `/tmp/wibox-media-test` and runs
it without flashing. A reboot restores the daemon from `/usr`.

## Releases

Release Please owns version bumps and changelog updates. Tags are plain semver
tags such as `v0.5.1`.

The firmware workflow builds an image on pull requests and pushes to `main`.
When a GitHub Release is published, it attaches:

```text
wibox-media-${VERSION}.img
MD5SUMS
SHA256SUMS
```

The on-device updater consumes the release image and `MD5SUMS`.

## What Should Not Be Reintroduced

The production image should not contain:

- legacy listener scripts;
- web UI runtime scripts;
- `mosquitto_pub` / `mosquitto_sub`;
- `ipctool`;
- SSH client tools such as `dbclient` and `scp`;
- `dropbearconvert`;
- `audio_bridge` or `video_rtp_bridge` runtime binaries;
- `sip_media` compatibility binary;
- shell updater wrappers such as `update_firmware.sh`.

`scripts/verify_image.sh` enforces these invariants.
