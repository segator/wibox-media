# SIP Media + D1 Video Media

This is the current integration path for replacing Sofia per call while keeping
the one-time Sofia boot warmup.

## Components

- `src/sip_media/`: current source directory for `wibox-media-daemon`, the
  SIP/RTP media app. It still uses the historical directory name during the
  migration.
- `src/sip_media/audio_hw.c`: direct GADI audio hardware module. SIP RTP
  threads read A-law frames from AI and send remote RTP payloads to AO without
  named pipes.
- `src/sip_media/video_worker.c`: in-daemon D1 H.264 RTP worker. It embeds the
  verified `src/video_rtp_bridge/` capture path and runs in a per-call child
  process for GADI/ioctl crash containment.
- `src/audio_bridge/`: retained as the source/debug copy of the imported
  `wibox-audio` bridge, not as a firmware runtime binary.
- `src/video_rtp_bridge/`: retained as a standalone research/debug tool, not as
  a firmware runtime binary.

Runtime processes:

```text
wibox-media-daemon  <-> GADI AI/AO audio hardware
wibox-media-daemon  <-> RTP audio PCMA/8000 on port 8000
wibox-media-daemon  ->  forks in-daemon video worker when SDP has a remote video port
video worker        -> RTP H.264/90000 on port 8002
wibox-media-daemon  <-> MQTT/Home Assistant over native MQTT 3.1.1 TCP client
```

## Call Flow

1. Sofia warms up the video hardware once after boot.
2. Sofia exits.
3. `wibox-media-daemon` runs under `app_watchdog.sh`.
4. Doorbell `ALARM_REPORT` is read directly from `/dev/ttySGK1` by
   `wibox-media-daemon`. The legacy `/tmp/pipe_sip` `DING` trigger remains for
   manual testing.
5. `wibox-media-daemon` sends SIP INVITE with:
   - `m=audio ... RTP/AVP 8`
   - `m=video ... RTP/AVP 96`
6. When the call is established:
   - `wibox-media-daemon` sends `START_CALL` to `/dev/ttySGK1`;
   - audio threads start direct GADI AI/AO plus PCMA RTP;
   - the in-daemon video worker starts D1 capture and sends H.264 RTP.
7. On hangup:
   - `wibox-media-daemon` stops the video worker child;
   - audio threads stop;
   - `STOP_CALL` is sent to `/dev/ttySGK1`.

## Build

```sh
make build-media
make build
```

The in-daemon video worker links `libadi.a`; `SDK_DIR` defaults to
`$HOME/config/GK710X_LinuxSDK_v2.0.0`.
The direct audio path needs `libap.so`, which is copied from `SDK_DIR` into the
firmware libraries. `make build` runs `make build-media` before packing the
cramfs image.

## Configuration

Defaults are installed to `/etc/*.default` and copied to `/mnt/mtd` on first
boot if persistent configs do not exist.

Relevant `/mnt/mtd/sip_media.conf` values:

```ini
rtp_port=8000
video_enabled=1
video_rtp_port=8002
video_payload_type=96
serial_listener_enabled=1
intercom_device=/dev/ttySGK1
audio_buffer_size=160
audio_chip_gpio=18
mqtt_enabled=1
mqtt_host=127.0.0.1
mqtt_user=
mqtt_pass=
mqtt_homeassistant_prefix=homeassistant
mqtt_base_topic=
mqtt_device_id=
mqtt_device_name=
```

## Local Control API

`wibox-media-daemon` keeps `/tmp/pipe_sip` as a local control FIFO. It accepts:

```sh
echo DING > /tmp/pipe_sip
echo 'UART FB 11 00 1C' > /tmp/pipe_sip
echo 'AUDIO_TEST 192.168.0.183 4012 5' > /tmp/pipe_sip
echo 'VIDEO_TEST 192.168.0.183 4014 5' > /tmp/pipe_sip
```

`DING` triggers an outgoing call directly. `UART ...` injects a 4-byte panel
frame into the same handler used by `/dev/ttySGK1`, useful for testing
`ALARM_REPORT`, `HANG_UP`, and `CMD_STOP_RING` without pressing the physical
button.
`VIDEO_TEST` starts the panel call context, runs the embedded D1 video worker
for a bounded local test, and then stops the panel context.
`AUDIO_TEST` starts the panel call context, starts direct GADI audio RTP for a
bounded local test, and then stops audio and the panel context.

## Verification Done

- `wibox-media-daemon` compiles with direct GADI audio and embedded video.
- Firmware image builds with all binaries and runtime libraries.
- Real MicroSIP call verified with audio and H.264 video.
- Real MicroSIP call verified the in-daemon video worker: `stream_id == 0`
  frames were captured and sent, DTMF `#` unlocked the door, and BYE stopped
  video/audio cleanly.
- DTMF door unlock works in MicroSIP automatic mode after negotiating
  `telephone-event/8000`; SIP INFO is also accepted as a fallback.
- WiBox smoke test:
  - `wibox-media-daemon` starts, binds SIP/RTP, creates `/tmp/pipe_sip`,
    opens `/dev/ttySGK1`, and exits cleanly.
  - Control FIFO can inject UART frames for local testing.
  - MQTT fake-client test publishes Home Assistant discovery and handles
    `video/enabled/set`.
  - `AUDIO_TEST` starts GADI/AEC directly, sends RTP audio packets, and stops
    audio cleanly without creating `/tmp/audio_from_intercom` or
    `/tmp/audio_to_intercom`.
  - `VIDEO_TEST` starts the embedded worker and captures D1 `stream_id == 0`.
- Real MicroSIP call on the direct GADI audio build verified audio, H.264
  video, automatic-mode DTMF `#`, door unlock and cleanup on BYE.
- Local MQTT mock test verifies the native MQTT client without mosquitto
  binaries.
- Real MQTT broker/Home Assistant verification passed:
  - retained discovery topics exist for 10 WiBox entities;
  - retained state topics show the WiBox online and idle;
  - call/SIP/video active sensors use plain ON/OFF semantics, not
    connectivity/disconnected wording;
  - WiFi RSSI is retained from `/usr/sbin/wpa_cli`;
  - `video/enabled/set OFF/ON` commands are consumed by the daemon.

## Still To Verify

- H.264 RTP compatibility with Asterisk/WebRTC/SIP-HASS.
- Long-call stability and cleanup around the 90s MCU auto-stop behavior.
- Daylight video quality; low light adds analog sensor/CVBS noise that bitrate
  cannot remove.
