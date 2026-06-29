# WiBox Media Architecture

This document tracks the migration from the current working multi-process
prototype to the production `wibox-media-daemon`.

## Current Runtime

The current firmware boots these relevant processes:

```text
run.sh
  -> Sofia_temp.sh                 one-time video hardware warmup
  -> listener.sh                   serial intercom event listener
       -> listener_mqtt.sh         MQTT/Home Assistant command listener
  -> app_watchdog.sh audio_bridge  audio hardware bridge
  -> app_watchdog.sh sip_media     SIP/RTP media application

sip_media
  -> video_rtp_bridge              forked per video call
```

Data flow:

```text
/dev/ttySGK1
  ^      ^
  |      |
  |      +-- sip_media: START_CALL, STOP_CALL, UNLOCK_DOOR
  |
  +--------- listener.sh: ALARM_REPORT, call state, legacy MQTT actions

listener.sh --DING--> /tmp/pipe_sip --> sip_media

audio_bridge <--> /tmp/audio_from_intercom
audio_bridge <--> /tmp/audio_to_intercom
                              ^
                              |
                         sip_media <--> SIP/RTP audio
                              |
                              +--> video_rtp_bridge <--> RTP H.264 video

listener_mqtt.sh <--> MQTT/Home Assistant
```

## Current Problems

- `/dev/ttySGK1` has two owners: `listener.sh` and `sip_media`.
- MQTT commands expose low-level intercom sequencing instead of one safe
  operation such as "open door".
- SIP media, DTMF, serial state and MQTT state live in different runtimes.
- The web UI is not part of the desired operational path; Home Assistant via
  MQTT is the UI.
- `Sofia_temp.sh` is still required once per boot as a hardware warmup.

## Target Runtime

The target production shape is one main daemon:

```text
run.sh
  -> Sofia warmup                  temporary, until replicated fully
  -> app_watchdog.sh wibox-media-daemon

wibox-media-daemon
  - serial /dev/ttySGK1 state machine
  - SIP signaling
  - RTP audio
  - H.264 D1 video capture/RTP
  - DTMF via RTP telephone-event and SIP INFO
  - MQTT/Home Assistant discovery and state
  - doorbell/call/unlock lifecycle
```

Temporary workers may stay separate during migration, but the final ownership
model is a single daemon controlling serial, media and MQTT.

## Home Assistant Model

Expose state reactively. Do not expose manual `start_call` or `stop_call`
buttons as normal user controls.

Entities:

```text
button.wibox_open_door
binary_sensor.wibox_ringing
binary_sensor.wibox_call_active
binary_sensor.wibox_sip_call_active
binary_sensor.wibox_video_active
sensor.wibox_media_state
sensor.wibox_last_ring
sensor.wibox_last_unlock
sensor.wibox_wifi_rssi
switch.wibox_video_enabled
```

Primary MQTT command:

```text
wibox/<id>/door/open/set = PRESS
```

Internal behavior:

```text
open_door()
  if call_active:
      unlock_door()
  else if panel_requires_call_context:
      start_call_context()
      wait until ready or short delay
      unlock_door()
      stop_call_context()
```

Call state is reactive:

```text
ALARM_REPORT from serial
  -> ringing=true
  -> publish last_ring
  -> optionally place outgoing SIP call

SIP incoming/outgoing established
  -> call_active=true
  -> START_CALL to panel
  -> media active

SIP BYE, serial hangup or timeout
  -> call_active=false
  -> STOP_CALL if needed
  -> media stopped

DTMF # or MQTT open_door
  -> unlock_door()
  -> publish last_unlock
```

## Migration Phases

Each phase must keep the previous working behavior verified before moving on.

### Phase 0: Lock Current Working Baseline

Status: complete.

Evidence:
- MicroSIP call shows audio and video.
- DTMF `#` works in automatic mode after negotiating `telephone-event/8000`.
- `video_enabled=1` default exists; setting it to `0` disables video SDP and
  prevents `video_rtp_bridge` launch.
- `make build-media` compiles media artifacts.

### Phase 1: Create `wibox-media-daemon` Shell

Goal:
- Rename/evolve `sip_media` into `wibox-media-daemon` as the owner of media and
  intercom state.
- Keep compatibility symlinks or wrapper names only as needed during migration.
- Do not remove `listener.sh` yet.

Implementation:
- Build `src/sip_media` as `wibox-media-daemon`.
- Keep `sip_media` as a compatibility symlink during migration.
- Keep the existing `/mnt/mtd/sip_media.conf` config name until the daemon
  owns all subsystems.

Verification:
- Build succeeds.
- Firmware includes `/usr/bin/wibox-media-daemon`.
- Existing SIP audio/video/DTMF call still works on WiBox.
- `video_enabled=0` still suppresses video.

### Phase 2: Move Serial Listener Into Daemon

Goal:
- Daemon reads `/dev/ttySGK1` and handles:
  - `ALARM_REPORT`
  - `START_CALL`
  - `HANG_UP`
  - `CMD_STOP_RING`
  - push/forward state codes currently handled by `listener.sh`
- Daemon emits the same logical states currently published through scripts.
- Stop starting `listener.sh` by default.

Implementation:
- `wibox-media-daemon` opens `/dev/ttySGK1` when
  `serial_listener_enabled=1`.
- `ALARM_REPORT` triggers the same outgoing SIP call path as the legacy
  `/tmp/pipe_sip` `DING` message.
- `/tmp/pipe_sip` also accepts `UART FB 11 00 1C` style injected frames as a
  local test API.
- `HANG_UP` and `CMD_STOP_RING` terminate an active SIP call.
- `run.sh` starts `listener.sh` only for legacy `sip_media` fallback installs.
- MQTT/Home Assistant publication remains in Phase 3.

Verification:
- Doorbell press triggers SIP call without `listener.sh`.
- SIP incoming call still starts panel media context.
- Hangup and timeout clear state and stop media.
- DTMF/MQTT open door still unlocks.

### Phase 3: Move MQTT/Home Assistant Into Daemon

Goal:
- Replace `listener_mqtt.sh`, `mqtt_config_homeassistant.sh`,
  `heartbeat_mqtt.sh` and `mqtt_wifi_stats.sh` with daemon MQTT logic.
- Publish Home Assistant discovery for the target entity model.
- Expose one high-level door open action.

Verification:
- Home Assistant discovers entities.
- `button.open_door` performs the full required intercom sequence.
- Sensors update on ring, call start, call end, unlock and video state.
- MQTT reconnect/reboot behavior is sane.

Implementation:
- `wibox-media-daemon` owns MQTT/Home Assistant discovery and state publishing.
- MQTT transport currently uses the bundled `mosquitto_pub`/`mosquitto_sub`
  clients from daemon-owned code instead of the legacy shell scripts.
- The daemon publishes the target Home Assistant model:
  `button.open_door`, ringing/call/SIP/video binary sensors, media state,
  last ring, last unlock, WiFi RSSI and `switch.video_enabled`.
- The only primary door command is `wibox/<id>/door/open/set = PRESS`.
- `video/enabled/set` updates the in-memory per-call video flag.
- MQTT connection is probed before discovery is published; if the broker is
  unavailable or rejects authentication, the daemon retries without spawning
  a persistent `mosquitto_sub` loop.
- Legacy scripts remain in the image until Phase 6 but are no longer started
  on the `wibox-media-daemon` path.

Verification:
- `make build-media` succeeds.
- WiBox fake-client test published Home Assistant discovery and initial state.
- WiBox fake-client test consumed `video/enabled/set OFF` and updated daemon
  state without crashing.
- Real broker is reachable but rejected the provided runtime credentials during
  direct `mosquitto_pub/sub` tests, so real HA discovery is pending corrected
  auth/ACL.

### Phase 4: Integrate Video Worker

Goal:
- Move `video_rtp_bridge` logic into the daemon or a daemon-owned module.
- Keep per-call isolation only if it is proven useful for crash containment.

Implementation:
- Added `src/sip_media/video_worker.c` as a daemon-linked wrapper around the
  verified D1 H.264 capture path.
- `wibox-media-daemon` still forks per call, but the child executes the
  in-process video worker function instead of `execl()`ing
  `/usr/bin/video_rtp_bridge`.
- `video_rtp_bridge` remains available as a standalone source/debug tool, but
  the firmware runtime no longer packages `include/bin/video_rtp_bridge`.
- The fork boundary is intentionally retained because the GADI/ioctl path is
  complex and has prior crash risk; ownership is now daemon-controlled.
- `/tmp/pipe_sip` supports `VIDEO_TEST <ip> <port> <seconds>` for bounded local
  D1 worker tests without a SIP peer.

Verification:
- D1 `stream_id==0` RTP H.264 works.
- Long call cleanup works.
- `video_enabled=0` keeps video fully disabled.

Evidence:
- `make build-media` links `wibox-media-daemon` with `libadi.a`.
- Real MicroSIP call started `Started in-daemon video worker`, captured and
  sent D1 `stream_id == 0` frames, accepted DTMF `#`, and cleaned up on BYE.
- `VIDEO_TEST 192.168.0.183 4014 5` captured `stream_id == 0` frames and
  stopped the worker/panel context cleanly.

### Phase 5: Integrate Audio Bridge

Goal:
- Move `audio_bridge` into the daemon and remove audio pipes.
- Own audio hardware and RTP timing from one event loop/thread model.

Verification:
- Two-way audio works.
- DTMF still works.
- No pipe recovery logic is needed.

### Phase 6: Remove Web UI and Legacy Scripts

Goal:
- Remove unused web UI from the generated image.
- Remove `listener.sh`, `listener_mqtt.sh` and legacy MQTT helper scripts after
  daemon replacement is verified.

Verification:
- Firmware boots without web UI/scripts.
- SSH and MQTT/Home Assistant remain usable.
- No boot errors from missing files.

## Production Invariants

- Only one runtime owns `/dev/ttySGK1`.
- Home Assistant receives state; it does not orchestrate serial command
  sequences.
- Door unlock is a single high-level operation.
- Video is optional by config and defaults to enabled.
- Sofia warmup is once per boot only until the remaining VI/VENC init is
  replicated.
