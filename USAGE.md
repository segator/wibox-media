# Usage Cheatsheet

## Build

```bash
make docker
make build
make verify
```

Final image:

```text
release/latest
```

## Device Config

Persistent config:

```text
/mnt/mtd/sip_media.conf
```

Minimal SIP/MQTT example:

```ini
outgoing_call_target=sip:1000@192.168.0.31:5060
mqtt_enabled=1
mqtt_host=192.168.10.2
mqtt_user=mqtt
mqtt_pass=password
video_enabled=1
```

Set `video_enabled=0` for audio-only installations.

## Runtime Test Without Flashing

```bash
make deploy-runtime
make verify-device
```

## Flash

```bash
make flash-dry-run
make backup-mtd4
make flash CONFIRM_FLASH=YES
reboot
```

## Local Boot Hook

Optional executable hook:

```text
/mnt/mtd/post.sh
```

It runs after `wibox-media-daemon` starts. Do not start Sofia or another media
runtime from it.

## Local Test API

The daemon creates:

```text
/tmp/pipe_sip
```

Examples:

```bash
echo DING > /tmp/pipe_sip
echo 'UART FB 11 00 1C' > /tmp/pipe_sip
echo 'AUDIO_TEST 192.168.0.183 4012 5' > /tmp/pipe_sip
echo 'VIDEO_TEST 192.168.0.183 4014 5' > /tmp/pipe_sip
```

`UART ...` injects an intercom frame, useful when the physical portal button is
not reachable.
