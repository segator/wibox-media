# Architecture

This is the current production shape of the custom WiBox image.

## Boot Flow

```text
/etc/init.d/rcS
  -> mounts /usr from mtd4 cramfs
  -> mounts /mnt from mtd5 jffs2
  -> /usr/run.sh
       -> GPIO and LED setup
       -> kernel modules
       -> WiFi station setup from /mnt/mtd/wpa_supplicant.conf
       -> Dropbear SSH
       -> short Sofia warmup for video hardware
       -> app_watchdog.sh wibox-media-daemon /usr/bin/wibox-media-daemon
       -> optional /mnt/mtd/post.sh
```

Sofia is still used once per boot to initialize hardware state that the D1 video
capture path depends on. It is stopped after the warmup. It is not used per
call.

## Runtime Ownership

`wibox-media-daemon` is the only packaged media runtime.

It owns:

- intercom serial state on `/dev/ttySGK1`;
- SIP signaling on UDP `5060` by default;
- PCMA RTP audio on UDP `8000` by default;
- optional H.264 RTP video on UDP `8002` by default;
- direct GADI audio hardware setup and teardown;
- D1 video worker lifecycle;
- DTMF door unlock from RTP telephone-event and SIP INFO;
- MQTT/Home Assistant discovery, commands and state;
- firmware update checks and install requests;
- Prometheus `/metrics` and `/healthz`.

## Call Flow

Doorbell-originated call:

```text
/dev/ttySGK1 ALARM_REPORT or PUSH_STATE_0
  -> media/state = ringing
  -> SIP INVITE to outgoing_call_target
  -> SIP established
  -> START_CALL to /dev/ttySGK1
  -> audio RTP starts
  -> video worker starts if negotiated and video_enabled=1
  -> media/state = established
```

Hangup/timeout:

```text
SIP BYE, SIP failure, HANG_UP or CMD_STOP_RING
  -> video worker stops
  -> audio stops
  -> STOP_CALL when an established panel context exists
  -> media/state = idle
```

Door unlock:

```text
DTMF # or MQTT door/open/set=PRESS
  -> FB 12 01 1E to /dev/ttySGK1
  -> door/unlocked pulses ON then OFF
```

## MQTT / Home Assistant

The daemon contains a small native MQTT 3.1.1 client. It does not package or
spawn `mosquitto_pub` or `mosquitto_sub`.

Default base topic:

```text
wibox/<hostname>
```

Primary entities:

```text
button.open_door
sensor.media_state
sensor.firmware_version
sensor.firmware_commit
sensor.firmware_build_timestamp
binary_sensor.door_unlocked
sensor.wifi_rssi
switch.video_enabled
binary_sensor.firmware_update_available
sensor.firmware_update_version
button.firmware_update_refresh
button.firmware_update_install
```

`media_state` is the high-level state for automation:

```text
idle
ringing
established
```

Older intermediate sensors such as `call_active`, `sip_call_active`,
`video_active`, `last_ring` and `last_unlock` are intentionally cleared from
retained MQTT discovery/state.

## Firmware Updates

Routine updates are handled by `/usr/bin/firmware_update`.

`wibox-media-daemon` checks GitHub releases at startup and roughly once per day.
Home Assistant can force a check with `Firmware Update Refresh` and start an
install with `Firmware Update Install`.

The install button is disabled unless an update is available. After an install
request is accepted, the daemon immediately disables the button and ignores
duplicate install requests while the updater is running.

## Prometheus

When enabled, the daemon listens on port `9617` by default:

```text
GET /healthz
GET /metrics
```

Metrics include build metadata, uptime, MQTT connection state, media state,
ring/unlock/call counters, video state and WiFi RSSI.

## LED Policy

LEDs are currently owned by `run.sh`, not the daemon:

```text
red    booting or WiFi failure
off    WiFi setup in progress
green  WiFi associated and DHCP succeeded
blue   production boot complete and daemon started
```

`gpio.sh` also initializes board lines that must be ready before media startup,
including the audio chip enable line on GPIO 18.

## Persistent Files

```text
/mnt/mtd/wpa_supplicant.conf   WiFi station config, required before first flash
/mnt/mtd/sip_media.conf        daemon runtime config
/mnt/mtd/dropbear/             SSH host keys
/mnt/mtd/post.sh               optional local boot hook
```

`/tmp` and `/var` are RAM-backed and disappear on reboot.

## Build Artifacts

The build starts from `mtd4`, extracts it into `cramfs/`, applies scripts from
`scripts/`, copies `include/`, then packs `release/latest`.

The production image should contain:

```text
/usr/bin/wibox-media-daemon
/usr/bin/firmware_update
/usr/bin/app_watchdog.sh
/usr/run.sh
/usr/etc/sip_media.conf.default
/usr/etc/wibox-release
```

It should not contain legacy listener scripts, web UI runtime scripts,
`mosquitto_*`, `ipctool`, SSH client tools, `audio_bridge`,
`video_rtp_bridge`, `sip_media`, or updater shell wrappers.
