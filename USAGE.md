# WiBox Media Usage

## Build Image

```bash
make docker
make build
make verify
```

The final image is `release/latest`. Both daemon compilation and cramfs packing
run inside `wibox-build-tool:latest`.

## Configure Device

Persistent runtime config:

```text
/mnt/mtd/sip_media.conf
```

Minimal MQTT/Home Assistant example:

```ini
mqtt_enabled=1
mqtt_host=192.168.10.2
mqtt_user=mqtt
mqtt_pass=password
```

Video is enabled by default:

```ini
video_enabled=1
```

Set it to `0` for installations without video intercom support.

## Runtime Test Without Flashing

```bash
make deploy-runtime
make verify-device
```

This uploads `wibox-media-daemon` to `/tmp`, restarts it, verifies its checksum
and checks MQTT discovery/state.

## Local Boot Hook

For site-specific startup extras, create an executable file on the device:

```bash
/mnt/mtd/post.sh
```

`run.sh` executes it after starting `wibox-media-daemon`. Do not use this hook
to start Sofia or a second media runtime.

## Flash

```bash
make flash-dry-run
make backup-mtd4
make flash CONFIRM_FLASH=YES
reboot
```

`flash` runs `backup-mtd4` automatically before writing. Backups are stored in
`backups/` and are intentionally ignored by git.

## Local Test API

The daemon creates `/tmp/pipe_sip`:

```bash
echo DING > /tmp/pipe_sip
echo 'UART FB 11 00 1C' > /tmp/pipe_sip
echo 'AUDIO_TEST 192.168.0.183 4012 5' > /tmp/pipe_sip
echo 'VIDEO_TEST 192.168.0.183 4014 5' > /tmp/pipe_sip
```

`UART ...` injects an intercom frame, which is useful when the physical portal
button is not reachable.
