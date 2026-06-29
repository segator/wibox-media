# AGENTS.md — WiBox GK7102S Media Capture Project

## Purpose

Reverse-engineering H.264 D1 (688×576) video capture from a **WiBox GK7102S** IP camera
without relying on the vendor's Sofia binary. Goal: capture raw H.264 bitstream directly
via kernel ioctls using the GADI SDK (`libadi.a`) for initialization only.

## Before Any Task — Read These Files

**Start here (current state + full history):**
- `research/FASE3_PROGRESS.md` — complete progress log, all blockers, next steps
- `research/KERNEL_ANALYSIS.md` — `media.ko` reverse engineering reference (ioctl dispatch table, struct layouts, sys_state fields)
- `research/SOFIA_IOCTL_ANALYSIS.md` — full trace analysis of Sofia's 4,969 ioctls (thread map, init sequence, streaming sequence)

**Source code:**
- `src/d1_video_capture.c` — consolidated working D1 H.264 capture test.
- `src/audio_bridge/` — working GADI audio hardware bridge imported from `wibox-audio`.
- `src/sip_media/` — SIP/RTP audio app extended to advertise video and launch `video_rtp_bridge`.
- `src/video_rtp_bridge/` — D1 H.264 RTP sender derived from the verified video capture path.

**Sofia ioctl traces (ground truth):**
- `sofia_ioctls_captured.log` — full ioctl trace from Sofia (root/working dir copy)
- `research/iotrace_full_4969_ioctls.log` — complete 4969-ioctl trace with all args
- `research/iotrace_with_buffer_dumps.log` — trace with buffer contents for key ioctls
- `research/sofia_ioctls_unique.txt` — deduplicated list of all unique ioctls seen

**Device documentation:**
- `docs/system.md` — WiBox hardware: CPU, sensors, memory map, device nodes
- `docs/codes.md` — error codes, return value reference
- `include/wibox_codes.txt` — WiBox-specific error codes

## Hardware & Environment

| Item | Value |
|------|-------|
| Device | WiBox GK7102S, ARM11 (ARM1176JZF-S) |
| Camera | PAL CVBS sensor, D1 = 688×576 |
| IP | 192.168.0.196, root / qv2008 |
| SSH | dropbear (no sftp — use base64 pipe for binary upload) |
| Kernel module | `/ko/media.ko` (r13210, not stripped) |
| SDK lib | `libadi.a` r10973 in `GK710X_LinuxSDK_v2.0.0/` |

**Binary upload to WiBox:**
```bash
base64 binary | ssh root@192.168.0.196 "base64 -d > /tmp/bin && chmod +x /tmp/bin"
```

**Build (Docker):**
```bash
docker run --rm -v $(pwd):/work -v /path/to/sdk:/sdk wibox-build:latest \
  arm-goke-linux-uclibcgnueabi-gcc -static -std=gnu99 \
  -I/sdk/include -I/sdk/gadi_include \
  -o /work/out /work/src.c /sdk/lib/libadi.a -lpthread -lm
```

## Current State (as of 2026-06-29)

### What works
- `src/d1_video_capture.c` captures H.264 `stream_id==0` from the main D1 encoder.
- Resolution verified by `ffprobe`: `688x576`, H.264 Main profile.
- Real camera image works when the MCU call path is enabled with `/dev/ttySGK1`.
- Sofia is still required as a one-time warmup after boot, but not per call.

### Stream ID map
| stream_id | Encoder type | Resolution |
|-----------|-------------|------------|
| 0 | type0 (main) | 688×576 — **D1 target** |
| 1 | type1 | 352×288 |
| 2 | type2 | 352×288 |
| 3 | type3 | preview |

### Current video workflow
1. Start Sofia once after boot and let it warm up for about 30 seconds.
2. Kill Sofia.
3. Start call video path: `printf "\xfb\x14\x01\x20" > /dev/ttySGK1`.
4. Run `/tmp/d1_video_capture /tmp/d1_capture.h264 10`.
5. Stop call video path: `printf "\xfb\x14\x00\x1f" > /dev/ttySGK1`.
6. Verify with `ffprobe`; expected resolution is `688x576`.

### SIP media integration
- `audio_bridge`, `sip_media`, and `video_rtp_bridge` build and are included in firmware.
- `sip_media` advertises PCMA audio plus H.264 video in SDP.
- On established calls, `sip_media` sends `START_CALL`, starts audio RTP, and forks
  `video_rtp_bridge` for D1 H.264 RTP.
- On hangup, it stops video, stops audio, and sends `STOP_CALL`.
- End-to-end SIP video negotiation still needs a real call test.

## Key Technical Facts

### `0x40047687` struct (40 bytes, NOT 4)
`_IOC_SIZE` reports 4 bytes but the driver copies 40 internally:
```c
struct srcbuf_format_t {
    uint16_t main_width;    // 0:  must be multiple of 16 (688)
    uint16_t main_height;   // 2:  must be even (576)
    uint16_t ch_mode_0;     // 4:  0..2
    uint16_t sub1_w;        // 6:  <= main_width
    uint16_t sub1_h;        // 8:  <= main_height
    uint16_t main_w_dup;    // 10: = main_width
    uint16_t main_h_dup;    // 12: = main_height
    uint16_t ch_mode_1;     // 14:
    uint16_t sub2_w;        // 16:
    uint16_t sub2_h;        // 18:
    uint16_t main_w_dup2;   // 20:
    uint16_t main_h_dup2;   // 22:
    uint16_t ch_mode_2;     // 24:
    uint16_t sub3_w;        // 26:
    uint16_t sub3_h;        // 28:
    uint16_t main_w_dup3;   // 30:
    uint16_t main_h_dup3;   // 32:
    uint16_t ch_mode_3;     // 34:
    uint8_t  interlace_scan;// 36: 0=progressive, 1=interlaced
    uint8_t  pad[3];        // 37-39
};
```

### Sofia ioctl sequence (before `0x40047687`)
```
0x80047652  ('v',0x52) READ   GET_VERSION
0x40047654  ('v',0x54) WRITE  SET_ENCODE_STATE=0  (reset)
0x80047670  ('v',0x70) READ   GET_CHIP_INFO
0x40047316  ('s',0x16) WRITE  VI_SOURCE: set source mode
0x80047301  ('s',0x01) READ   VI_SOURCE: get state
0x80047304  ('s',0x04) READ   VI_SOURCE: get format
0x80046920  ('i',0x20) READ   VI_ADAPTER: get info
0x40046921  ('i',0x21) WRITE  VI_ADAPTER: configure
0x40047303  ('s',0x03) WRITE  VI_SOURCE: set
0x4004730b  ('s',0x0b) WRITE  VI_SOURCE: set
0x40047304  ('s',0x04) WRITE  VI_SOURCE: set FPS
0x80047305  ('s',0x05) READ   VI_SOURCE: GET CAPABILITIES  ← sets sys_state[0xdc]
0x40047673  ('v',0x73) WRITE  SYS: set resource limits
0x40047687  ('v',0x87) WRITE  SYS: SET_SRCBUF_FORMAT (40 bytes)
0x40047683  ('v',0x83) WRITE  SYS: SET_SRCBUF_TYPE
```

### sys_state global (.data+0 of media.ko)
| Offset | Description |
|--------|-------------|
| 0x0c   | current_width (set by 0x40047687) |
| 0x0e   | current_height |
| 0x18   | current_ch_mode |
| 0xd0   | encode_state (0=stopped, 2=streaming) — must be 0 to reconfigure |
| 0xdc   | pointer to VI subsystem state — **must be non-NULL for 0x40047687** |
| 0xf8   | part of string "video_out0" (NOT a width limit — value 0x6976=26998) |

### SDK init order (correct)
```c
gadi_sys_init(NULL);
gadi_vi_init(handle);   gadi_vi_open(handle, 0);
gadi_vout_init(handle); gadi_vout_open(handle, 0);
gadi_venc_init(handle); gadi_venc_open(handle, 0);
gadi_venc_map_bsb(handle);   // no extra args!
// then: gadi_venc_get_stream(handle, stream_id, &stream)
```

### Known pitfalls
- **Sofia warmup required once per boot**: VI system state (`sys_state[0xdc]`) is only
  valid after Sofia initializes the VI pipeline (~30s). Kill Sofia after warmup.
- **`0x80047652` is GET_VERSION, NOT stop_stream** — the correct reset is `0x40047654` with value 0.
- **SDK SET functions SEGV**: `gadi_venc_set_channels_params`, `gadi_venc_set_stream_format`,
  `gadi_venc_set_h264_config` — all crash before reaching the ioctl (SDK r10973 vs kernel r13210 mismatch).
  Use raw fd ioctls for all SET operations.
- **`get_stream(0xFF)` wildcard works**: passing specific stream_id to `gadi_venc_get_stream` fails;
  0xFF captures any available stream — then filter by `stream.stream_id`.
- **`[ERROR] drv in wrong state 2`**: appears on `gadi_sys_init` when encoder was already running.
  Not fatal — init continues.
- **Upload binaries via base64**: WiBox dropbear has no sftp support.
- **`/tmp` is tmpfs**: all uploaded binaries are lost on reboot.

## Compilation Environment

- Docker image: `wibox-build:latest` (cross-compiler `arm-goke-linux-uclibcgnueabi-gcc 4.6.1`)
- SDK headers: `GK710X_LinuxSDK_v2.0.0/adi/include/` + `gk7102_sdk/src/adi/include/`
- SDK lib: `GK710X_LinuxSDK_v2.0.0/install/arm11-gcc-uClibc-linux-GK710XS/lib/libadi.a`
- Flags: `-static -std=gnu99 -Os`
