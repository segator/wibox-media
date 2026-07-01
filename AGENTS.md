# AGENTS.md

<!-- codebase-memory-mcp:start -->
# Codebase Knowledge Graph (codebase-memory-mcp)

This project uses codebase-memory-mcp to maintain a knowledge graph of the
codebase. Prefer MCP graph tools over grep/glob/file-search for code discovery.

Priority order:

1. `search_graph` / `search_code`
2. `trace_path`
3. `get_code_snippet`
4. `query_graph`
5. `get_architecture`

Fallback to `rg` for string literals, non-code files, scripts, configs and
documentation.
<!-- codebase-memory-mcp:end -->

## Project Purpose

WiBox Media is custom firmware for the Fermax WiBox GK7102S intercom. It runs a
local SIP media daemon with audio, optional H.264 D1 video, DTMF/MQTT door
unlock, Home Assistant discovery, Prometheus metrics and GitHub-release based
firmware updates.

The packaged runtime binary is:

```text
/usr/bin/wibox-media-daemon
```

## Start Here

For product/user context:

- `README.md`
- `docs/getting_started.md`
- `docs/recovery.md`
- `docs/sip_media.md`
- `docs/updates.md`
- `docs/architecture.md`

For low-level reverse-engineering context:

- `research/FASE3_PROGRESS.md`
- `research/KERNEL_ANALYSIS.md`
- `research/SOFIA_IOCTL_ANALYSIS.md`
- `docs/d1_video_capture.md`
- `research/iotrace_full_4969_ioctls.log`
- `research/iotrace_with_buffer_dumps.log`

## Important Source Paths

- `src/sip_media/`: `wibox-media-daemon`.
- `src/sip_media/audio_hw.c`: direct GADI audio hardware path.
- `src/sip_media/video_worker.c`: D1 H.264 video worker wrapper.
- `src/video_rtp_bridge/video_rtp_bridge.c`: embedded D1 capture implementation
  included by the daemon worker, not a packaged runtime binary.
- `src/audio_bridge/ap.c`: AP/AEC helper imported from `wibox-audio`, used by
  direct daemon audio.
- `src/firmware_update.c`: HTTPS release downloader, verifier and MTD updater.
- `scripts/verify_image.sh`: generated image invariant checks.
- `tests/mqtt_native_mock.py`: host MQTT/Home Assistant regression test.

## Hardware And Device

| Item | Value |
|------|-------|
| Device | WiBox GK7102S, ARM11 |
| Camera | PAL CVBS sensor, D1 = 688x576 |
| Default dev IP | `192.168.0.196` |
| Default root password | `qv2008` |
| Intercom serial | `/dev/ttySGK1` |
| Custom `/usr` partition | `mtd4` |
| Persistent config | `/mnt/mtd` |

Stock firmware normally has telnet or serial, not SSH. Custom firmware starts
Dropbear SSH.

## Build And Test

```bash
make docker
make build
make verify
```

Development against a running custom WiBox:

```bash
make deploy-runtime
make verify-device
make device-status
```

Routine firmware updates should use `/usr/bin/firmware_update` or Home
Assistant. Do not reintroduce Makefile flashing targets unless there is a
specific reason.

## Current Runtime Facts

- Sofia is still run once per boot as `Sofia_temp.sh` for video hardware warmup.
- Sofia is not used per call.
- `wibox-media-daemon` owns serial, SIP, audio, video, MQTT and updater control.
- Home Assistant state is intentionally simple: `media_state` is `idle`,
  `ringing` or `established`.
- Older MQTT entities such as `last_ring`, `last_unlock`, `call_active`,
  `sip_call_active` and `video_active` should remain cleared from discovery.
- `video_enabled=1` defaults to video-capable calls; set it to `0` for audio-only
  installations.

## D1 Video Notes

The working D1 path captures H.264 `stream_id == 0` from the main encoder:

```text
stream_id 0 -> 688x576 main stream
stream_id 2 -> CIF/sub stream observed during testing
```

The daemon video worker uses GADI open/map calls plus raw ioctls. The full
VI/sensor initialization is not yet reproduced, so the boot Sofia warmup remains
necessary.

## Repository Hygiene

- Keep generated `cramfs/`, `release/`, `.verify-image-root/`, backups and
  extracted Sofia binaries out of git.
- Keep the README user-focused. Put detailed operational docs in `docs/` and
  reverse-engineering evidence in `research/`.
- The production image must not package legacy listener scripts, web UI runtime
  scripts, `mosquitto_*`, `audio_bridge`, `video_rtp_bridge`, `sip_media`, or
  shell updater wrappers.
