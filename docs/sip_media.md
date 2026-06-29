# SIP Media + D1 Video Media

This is the current integration path for replacing Sofia per call while keeping
the one-time Sofia boot warmup.

## Components

- `src/audio_bridge/`: imports the working `wibox-audio` hardware bridge. It
  owns GADI audio input/output and exposes PCMA frames through named pipes.
- `src/sip_media/`: imports the working SIP/RTP audio app and now advertises an
  optional H.264 video media line in SDP.
- `src/video_rtp_bridge/`: derived from the verified D1 capture test. It captures
  `stream_id == 0` and sends Annex-B H.264 as RTP/H264.

Runtime processes:

```text
audio_bridge  <-> /tmp/audio_from_intercom, /tmp/audio_to_intercom <-> sip_media
sip_media     <-> RTP audio PCMA/8000 on port 8000
sip_media     ->  forks video_rtp_bridge when SDP has a remote video port
video_rtp_bridge -> RTP H.264/90000 on port 8002, payload type 96
```

## Call Flow

1. Sofia warms up the video hardware once after boot.
2. Sofia exits.
3. `audio_bridge` and `sip_media` run under `app_watchdog.sh`.
4. Doorbell event writes `DING` to `/tmp/pipe_sip`.
5. `sip_media` sends SIP INVITE with:
   - `m=audio ... RTP/AVP 8`
   - `m=video ... RTP/AVP 96`
6. When the call is established:
   - `sip_media` sends `START_CALL` to `/dev/ttySGK1`;
   - audio threads start PCMA RTP;
   - `video_rtp_bridge` starts D1 capture and sends H.264 RTP.
7. On hangup:
   - `sip_media` stops `video_rtp_bridge`;
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
```

## Verification Done

- `audio_bridge`, `sip_media`, and `video_rtp_bridge` compile.
- Firmware image builds with all binaries and runtime libraries.
- Real MicroSIP call verified with audio and H.264 video.
- DTMF door unlock works in MicroSIP automatic mode after negotiating
  `telephone-event/8000`; SIP INFO is also accepted as a fallback.
- WiBox smoke test:
  - `sip_media` starts, binds SIP/RTP, creates `/tmp/pipe_sip`, exits cleanly.
  - `audio_bridge` starts, creates audio pipes, exits cleanly.
  - `video_rtp_bridge` usage path works.

## Still To Verify

- H.264 RTP compatibility with Asterisk/WebRTC/SIP-HASS.
- Long-call stability and cleanup around the 90s MCU auto-stop behavior.
- Daylight video quality; low light adds analog sensor/CVBS noise that bitrate
  cannot remove.
