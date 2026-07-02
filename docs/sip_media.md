# Runtime

`wibox-media-daemon` is the SIP, media, intercom, MQTT and update runtime.

## Configuration

Persistent config:

```text
/mnt/mtd/sip_media.conf
```

Default config in the image:

```text
/usr/etc/sip_media.conf.default
```

Key options:

```ini
outgoing_call_target=sip:1000@192.168.0.31:5060
outgoing_call_timeout=60
sip_port=5060
rtp_port=8000

video_enabled=1
video_rtp_port=8002
video_payload_type=96

serial_listener_enabled=1
intercom_device=/dev/ttySGK1

mqtt_enabled=1
mqtt_host=127.0.0.1
mqtt_user=
mqtt_pass=
mqtt_homeassistant_prefix=homeassistant
mqtt_base_topic=
mqtt_device_id=
mqtt_device_name=

firmware_update_enabled=1
firmware_update_repo=segator/wibox-media

prometheus_enabled=1
prometheus_port=9617

audio_buffer_size=160
audio_chip_gpio=18
```

Leave `mqtt_base_topic`, `mqtt_device_id` and `mqtt_device_name` empty unless
you need stable custom names. The daemon derives them from the WiBox hostname.

## SIP And Media

Outgoing doorbell calls are sent to `outgoing_call_target`.

The daemon advertises:

```text
audio: PCMA/8000
video: H.264/90000, payload type 96 by default
DTMF:  telephone-event/8000
```

When a SIP call is established, the daemon sends `START_CALL` to the intercom
MCU, starts direct GADI audio, and starts the D1 video worker if video was
negotiated and `video_enabled=1`.

On hangup or failure, it stops media and sends `STOP_CALL` when needed.

## Intercom Serial

Device:

```text
/dev/ttySGK1
```

Important commands:

```text
START_CALL   FB 14 01 20
STOP_CALL    FB 14 00 1F
OPEN_DOOR    FB 12 01 1E
```

Important incoming frames:

```text
ALARM_REPORT  FB 11 00 1C
PUSH_STATE_0  FB 19 00 24  (call forwarding off)
PUSH_STATE_1  FB 19 01 25  (call forwarding on)
HANG_UP       FB 13 00 1E / FB 13 01 1F
STOP_RING     FB 23 00 2E
```

Real outside-panel calls must arrive as `ALARM_REPORT`. If pressing the physical
WiBox forward button only produces `PUSH_STATE_0` / `PUSH_STATE_1`, that only
proves the call-forward button is being read. It does not prove the WiBox is
paired to the VDS address. See [Getting Started](getting_started.md#10-doorbell-call-troubleshooting).

See [UART Codes](codes.md) for the full list.

## Home Assistant / MQTT

The daemon publishes Home Assistant discovery using MQTT retained config
messages.

![WiBox Home Assistant device](img/homeassistant.png)

Default base topic:

```text
wibox/<hostname>
```

Commands:

```text
wibox/<hostname>/door/open/set = PRESS
wibox/<hostname>/f1/trigger/set = PRESS
wibox/<hostname>/video/enabled/set = ON|OFF
wibox/<hostname>/call_forward/enabled/set = ON|OFF
wibox/<hostname>/firmware/update/check/set = PRESS
wibox/<hostname>/firmware/update/install/set = PRESS
```

State topics:

```text
wibox/<hostname>/media/state
wibox/<hostname>/door/unlocked
wibox/<hostname>/video/enabled
wibox/<hostname>/call_forward/enabled
wibox/<hostname>/wifi/rssi
wibox/<hostname>/firmware/version
wibox/<hostname>/firmware/commit
wibox/<hostname>/firmware/build_timestamp
wibox/<hostname>/firmware/update/available
wibox/<hostname>/firmware/update/version
wibox/<hostname>/firmware/update/install/availability
```

`media/state` values:

```text
idle
ringing
established
```

`door/unlocked` is a short pulse: `ON` then `OFF`.

`f1/trigger/set` sends a short F1 auxiliary-function pulse. On Fermax systems
this is not the main door opener; it is intended for installations wired with an
additional F1 relay, for example an auxiliary door, lights or lift control.

`call_forward/enabled` controls the physical Fermax call-forward/redirect state.
`ON` sends `FB 19 01 25` and should leave the WiBox LED blue. `OFF` sends
`FB 19 00 24` and should leave it green. The same state is updated when the
physical WiBox forward button reports `PUSH_STATE_0` / `PUSH_STATE_1`.

The update install button is only available when an update is available. It is
set unavailable immediately after an install request is accepted.

## Prometheus

If `prometheus_enabled=1`, scrape:

```text
http://<wibox-ip>:9617/metrics
```

Health endpoint:

```text
http://<wibox-ip>:9617/healthz
```

Current metric families:

```text
wibox_info
wibox_uptime_seconds
wibox_health
wibox_mqtt_connected
wibox_call_active
wibox_sip_call_active
wibox_video_active
wibox_video_enabled
wibox_ringing
wibox_rings_total
wibox_calls_started_total
wibox_video_sessions_started_total
wibox_door_unlocks_total
wibox_last_ring_timestamp_seconds
wibox_last_unlock_timestamp_seconds
wibox_wifi_rssi_dbm
wibox_uart_frames_total
wibox_uart_unknown_frames_total
wibox_uart_alarm_reports_total
wibox_uart_hangups_total
wibox_uart_stop_rings_total
wibox_uart_resets_total
wibox_uart_push_state_total
wibox_uart_f1_total
```

Some Prometheus gauges expose lower-level runtime state for monitoring even
though Home Assistant intentionally presents only the simpler `media_state`.

The runtime log is `/var/log/wibox-media-daemon.log`. In the production image
`app_watchdog.sh` rotates it to `/var/log/wibox-media-daemon.log.old` at 100 KB.
`/var` is RAM-backed on the WiBox, so this protects RAM usage and does not write
logs to flash.

## Local Test API

The daemon creates a FIFO:

```text
/tmp/pipe_sip
```

Examples:

```sh
echo DING > /tmp/pipe_sip
echo 'UART FB 11 00 1C' > /tmp/pipe_sip
echo 'UART FB 19 00 24' > /tmp/pipe_sip
echo 'AUDIO_TEST 192.168.0.183 4012 5' > /tmp/pipe_sip
echo 'VIDEO_TEST 192.168.0.183 4014 5' > /tmp/pipe_sip
```

`DING` simulates a doorbell. `UART ...` injects a four-byte serial frame into
the same handler used by `/dev/ttySGK1`.

`AUDIO_TEST` and `VIDEO_TEST` are bounded diagnostics. They start the panel call
context, run media to the supplied IP/port for the requested number of seconds,
then stop the panel context.

## Logs

Runtime log:

```text
/var/log/wibox-media-daemon.log
```

Firmware update log:

```text
/tmp/firmware_update.log
```

Both are RAM-backed and reset on reboot.
