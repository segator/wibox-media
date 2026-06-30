# WiBox Media Architecture

This document tracks the migration from the current working multi-process
prototype to the production `wibox-media-daemon`.

## Current Runtime

The current firmware boots these relevant processes:

```text
run.sh
  -> Sofia_temp.sh                 one-time video hardware warmup
  -> app_watchdog.sh wibox-media-daemon

wibox-media-daemon
  - serial /dev/ttySGK1 state machine
  - SIP signaling and call lifecycle
  - direct GADI AI/AO audio
  - native MQTT/Home Assistant client
  -> video worker                  forked per video call
```

Data flow:

```text
/dev/ttySGK1
  ^      ^
  |      |
  |      +-- wibox-media-daemon: START_CALL, STOP_CALL, UNLOCK_DOOR
  |
  +--------- wibox-media-daemon: ALARM_REPORT, call state, MQTT actions

/tmp/pipe_sip --> wibox-media-daemon

wibox-media-daemon <--> GADI AI/AO audio
wibox-media-daemon <--> SIP/RTP audio
wibox-media-daemon  --> video worker <--> RTP H.264 video

wibox-media-daemon <--> MQTT/Home Assistant
```

## Current Problems

- `Sofia_temp.sh` is still required once per boot as a hardware warmup.
- Video still runs in a daemon-forked child for GADI/ioctl crash containment.
- `/mnt/mtd/sip_media.conf` keeps the historical config filename, but the only
  packaged runtime binary is `wibox-media-daemon`.
- Persistent flashing is intentionally guarded; the current WiBox runtime can
  be tested from `/tmp` before writing `mtd4`.

## Target Runtime

The target production shape is the current daemon model, with the Sofia warmup
removed once VI/VENC init is fully replicated:

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

The daemon owns serial, SIP, audio, video worker lifecycle, DTMF and MQTT.

## LED Policy

Current LED ownership stays in `run.sh` because those LEDs describe boot and
network health before the daemon exists:

```text
red   booting or WiFi failure
off   WiFi setup in progress
green WiFi associated and DHCP succeeded
blue  production boot complete, daemon started
```

Do not move this initial GPIO setup into the daemon yet. `gpio.sh` also
initializes board lines that must be ready before media startup, including the
audio chip enable line on GPIO 18.

If we later want call-state LEDs, add a daemon-owned GPIO module with an
explicit priority model instead of overloading `wifi_led()`:

```text
boot/network state  handled by run.sh until daemon starts
runtime idle        blue
ringing             blinking blue or green
active call         green
error/media fault   red
```

That should be a separate change with device-side visual verification.

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

Current verification targets:

```text
make test            host-side native MQTT regression test
make verify-image    extract release/latest and validate firmware contents
make verify-device   verify active WiBox daemon checksum and MQTT state
make verify          run test + verify-image + verify-device
```

`verify-image` checks that the generated cramfs contains the expected daemon,
default config, runtime libraries and boot script, and that legacy web/script
runtime artifacts are absent. `verify-device` reads `/mnt/mtd/sip_media.conf`
from the WiBox, verifies the active daemon executable checksum against the
local build, then validates the retained Home Assistant MQTT discovery/state
model against the configured broker.

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
- Remove compatibility binaries and wrappers from the production image.

Implementation:
- Build `src/sip_media` as `wibox-media-daemon`.
- Keep the existing `/mnt/mtd/sip_media.conf` config name for persistent
  upgrade compatibility.

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
- `run.sh` starts only `wibox-media-daemon` under `app_watchdog.sh`.
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
- MQTT transport is implemented in the daemon with a built-in MQTT 3.1.1
  client over plain TCP; no `mosquitto_pub/sub` binaries are packaged.
- The daemon publishes the target Home Assistant model:
  `button.open_door`, ringing/call/SIP/video binary sensors, media state,
  last ring, last unlock, WiFi RSSI and `switch.video_enabled`.
- The only primary door command is `wibox/<id>/door/open/set = PRESS`.
- `video/enabled/set` updates the in-memory per-call video flag.
- If the broker is unavailable or rejects authentication, the daemon reconnects
  without spawning helper processes.
- Legacy scripts are removed from the production image in Phase 6.

Verification:
- `make build-media` succeeds.
- WiBox fake-client test published Home Assistant discovery and initial state.
- WiBox fake-client test consumed `video/enabled/set OFF` and updated daemon
  state without crashing.
- Real broker verification passed after credentials/ACL were corrected:
  Home Assistant discovery topics are retained for all WiBox entities, runtime
  state is published under `wibox/IDS7938jrvc/#`, and
  `video/enabled/set OFF/ON` is consumed by the daemon.

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

Implementation:
- Added `src/sip_media/audio_hw.c` as a direct GADI audio hardware module.
- `wibox-media-daemon` initializes GADI AI/AO and AP/AEC when an audio session
  starts, then the existing RTP threads read AI frames and write AO frames
  directly.
- The firmware runtime no longer packages or starts `/usr/bin/audio_bridge`.
- The named-pipe handoff was removed from the active audio path; legacy
  `audio_ai_pipe`, `audio_ao_pipe`, `audio_bridge_pipe` and pipe retry config
  keys are accepted but ignored for old `/mnt/mtd/sip_media.conf` files.
- `libap.so` is now packaged for the daemon because direct audio uses the
  imported AP/AEC path.

Verification:
- `make build-media` links `wibox-media-daemon` with direct audio hardware
  support and `libap.so`; `include/bin/audio_bridge` is absent.
- WiBox smoke test starts only one `wibox-media-daemon` process; there is no
  `audio_bridge`, `video_rtp_bridge` or `listener.sh` runtime process.
- No `/tmp/audio_from_intercom` or `/tmp/audio_to_intercom` files are created.
- `AUDIO_TEST 192.168.0.183 4012 5` starts the panel context, initializes
  GADI/AEC directly, sends 248 RTP audio packets / 39,680 payload bytes, then
  disables the audio chip and stops cleanly.
- `VIDEO_TEST 192.168.0.183 4014 8` still captures D1 `stream_id == 0` frames
  after direct audio integration.

Remaining:
- None. Real MicroSIP call validation passed on the direct audio build.

### Phase 6: Remove Web UI and Legacy Scripts

Goal:
- Remove unused web UI from the generated image.
- Remove `listener.sh`, `listener_mqtt.sh` and legacy MQTT helper scripts after
  daemon replacement is verified.

Verification:
- Firmware boots without web UI/scripts.
- SSH and MQTT/Home Assistant remain usable.
- No boot errors from missing files.

Implementation:
- Removed web UI files from `include/web`.
- Removed legacy `listener*.sh`, `mqtt_*.sh`, `heartbeat_mqtt.sh` and
  packaged `mosquitto_pub/sub` clients from the generated image.
- Removed the mosquitto-client build patch; MQTT is now daemon-native.

Verification:
- `make build-media` succeeds with the native MQTT client.
- `make build` produces an image with no web UI, `listener*.sh`,
  `mqtt_*.sh`, `heartbeat_mqtt.sh` or `mosquitto_*` files.
- Local MQTT mock test exercises CONNECT, SUBSCRIBE, retained state publish,
  Home Assistant discovery publish, `door/open/set` and `video/enabled/set`.
- `make verify-image` extracts `release/latest`, confirms the packaged daemon
  checksum matches `include/bin/wibox-media-daemon`, checks the boot script and
  default config, and rejects legacy runtime artifacts.
- `make verify-device` passes against the real WiBox and broker using the
  device's `/mnt/mtd/sip_media.conf`, with retained discovery/state validated
  for the full 10-entity Home Assistant model.

## Production Invariants

- Only one runtime owns `/dev/ttySGK1`.
- Home Assistant receives state; it does not orchestrate serial command
  sequences.
- Door unlock is a single high-level operation.
- Video is optional by config and defaults to enabled.
- Sofia warmup is once per boot only until the remaining VI/VENC init is
  replicated.
