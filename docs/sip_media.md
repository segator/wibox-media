# SIP Media + D1 Video Media

This is the current integration path for replacing Sofia per call while keeping
the one-time Sofia boot warmup.

## Components

- `src/audio_bridge/`: imports the working `wibox-audio` hardware bridge. It
  owns GADI audio input/output and exposes PCMA frames through named pipes.
- `src/sip_media/`: current source directory for `wibox-media-daemon`, the
  SIP/RTP media app. It still uses the historical directory name during the
  migration.
- `src/video_rtp_bridge/`: derived from the verified D1 capture test. It captures
  `stream_id == 0` and sends Annex-B H.264 as RTP/H264.

Runtime processes:

```text
audio_bridge        <-> /tmp/audio_from_intercom, /tmp/audio_to_intercom <-> wibox-media-daemon
wibox-media-daemon  <-> RTP audio PCMA/8000 on port 8000
wibox-media-daemon  ->  forks video_rtp_bridge when SDP has a remote video port
video_rtp_bridge -> RTP H.264/90000 on port 8002, payload type 96
```

## Call Flow

1. Sofia warms up the video hardware once after boot.
2. Sofia exits.
3. `audio_bridge` and `wibox-media-daemon` run under `app_watchdog.sh`.
4. Doorbell `ALARM_REPORT` is read directly from `/dev/ttySGK1` by
   `wibox-media-daemon`. The legacy `/tmp/pipe_sip` `DING` trigger remains for
   manual testing.
5. `wibox-media-daemon` sends SIP INVITE with:
   - `m=audio ... RTP/AVP 8`
   - `m=video ... RTP/AVP 96`
6. When the call is established:
   - `wibox-media-daemon` sends `START_CALL` to `/dev/ttySGK1`;
   - audio threads start PCMA RTP;
   - `video_rtp_bridge` starts D1 capture and sends H.264 RTP.
7. On hangup:
   - `wibox-media-daemon` stops `video_rtp_bridge`;
   - audio threads stop;
   - `STOP_CALL` is sent to `/dev/ttySGK1`.

## Build

```sh
make build-media
make build
```

`video_rtp_bridge` uses `SDK_DIR`, defaulting to
`$HOME/config/GK710X_LinuxSDK_v2.0.0`.
`make build` runs `make build-media` before packing the cramfs image.

## Configuration

Defaults are installed to `/etc/*.default` and copied to `/mnt/mtd` on first
boot if persistent configs do not exist.

Relevant `/mnt/mtd/sip_media.conf` values:

```ini
rtp_port=8000
video_enabled=1
video_rtp_port=8002
video_payload_type=96
video_bridge_path=/usr/bin/video_rtp_bridge
serial_listener_enabled=1
intercom_device=/dev/ttySGK1
```

## Local Control API

`wibox-media-daemon` keeps `/tmp/pipe_sip` as a local control FIFO. It accepts:

```sh
echo DING > /tmp/pipe_sip
echo 'UART FB 11 00 1C' > /tmp/pipe_sip
```

`DING` triggers an outgoing call directly. `UART ...` injects a 4-byte panel
frame into the same handler used by `/dev/ttySGK1`, useful for testing
`ALARM_REPORT`, `HANG_UP`, and `CMD_STOP_RING` without pressing the physical
button.

## Verification Done

- `audio_bridge`, `wibox-media-daemon`, and `video_rtp_bridge` compile.
- Firmware image builds with all binaries and runtime libraries.
- Real MicroSIP call verified with audio and H.264 video.
- DTMF door unlock works in MicroSIP automatic mode after negotiating
  `telephone-event/8000`; SIP INFO is also accepted as a fallback.
- WiBox smoke test:
  - `wibox-media-daemon` starts, binds SIP/RTP, creates `/tmp/pipe_sip`,
    opens `/dev/ttySGK1`, and exits cleanly.
  - Control FIFO can inject UART frames for local testing.
  - `audio_bridge` starts, creates audio pipes, exits cleanly.
  - `video_rtp_bridge` usage path works.

## Still To Verify

- H.264 RTP compatibility with Asterisk/WebRTC/SIP-HASS.
- Long-call stability and cleanup around the 90s MCU auto-stop behavior.
- Daylight video quality; low light adds analog sensor/CVBS noise that bitrate
  cannot remove.
