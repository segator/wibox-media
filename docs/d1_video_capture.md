# D1 Video Capture on WiBox GK7102S

This document summarizes the working H.264 D1 capture path discovered on
2026-06-29.

## Status

Working:

- H.264 main stream capture from VENC `stream_id == 0`.
- Resolution verified by `ffprobe`: `688x576`, H.264 Main profile.
- Real camera image when the intercom call line is enabled through the MCU serial
  port.
- Correct real-time-ish frame pacing after matching Sofia's frame interval ioctl.

Still not fully independent:

- Sofia is still needed as a one-time hardware warmup after boot.
- Sofia is not needed per call once the driver/hardware state is initialized.
- The full VI/sensor/decoder init sequence still needs to be reproduced to remove
  the boot warmup dependency.

## Operational Flow

Current reliable flow:

1. Boot the WiBox.
2. Let Sofia run for about 30 seconds.
3. Kill Sofia and its watchdog/tracer wrapper.
4. For each call:
   - send the MCU "start call" command;
   - run or reuse the D1 capture path;
   - send media over RTP/SIP;
   - send the MCU "stop call" command.

Serial commands:

```sh
# Start call / enable real video path
printf "\xfb\x14\x01\x20" > /dev/ttySGK1

# Stop call
printf "\xfb\x14\x00\x1f" > /dev/ttySGK1

# Door relay
printf "\xfb\x12\x01\x1e" > /dev/ttySGK1
```

Important: without the start-call command, VENC can still produce a valid D1 H.264
bitstream, but the image content is not the real camera image.

## Build

```sh
docker run --rm \
  -v $(pwd):/work \
  -v /home/aymerici/config/GK710X_LinuxSDK_v2.0.0:/sdk \
  wibox-build:latest \
  arm-goke-linux-uclibcgnueabi-gcc -static -std=gnu99 \
  -I/work/include/adi \
  -I/sdk/adi/include \
  -I/sdk/install/arm11-gcc-uClibc-linux-GK710XS/include \
  -o /work/d1_capture_v2 /work/src/d1_capture_v2.c \
  /sdk/install/arm11-gcc-uClibc-linux-GK710XS/lib/libadi.a \
  -lpthread -lm
```

Upload through base64 because dropbear on the device has no SFTP support:

```sh
base64 d1_capture_v2 | ssh root@192.168.0.196 \
  "base64 -d > /tmp/d1_capture_v2 && chmod +x /tmp/d1_capture_v2"
```

## Capture Test

```sh
ssh root@192.168.0.196 '
  killall Sofia Sofia_temp.sh sofia_trace system_sofia timeout d1_capture_v2 2>/dev/null || true
  printf "\xfb\x14\x01\x20" > /dev/ttySGK1
  sleep 1
  /tmp/d1_capture_v2 /tmp/d1_capture.h264
  printf "\xfb\x14\x00\x1f" > /dev/ttySGK1
'
```

Fetch:

```sh
ssh root@192.168.0.196 'base64 /tmp/d1_capture.h264' | base64 -d > d1_capture.h264
ffprobe -v error -select_streams v:0 \
  -show_entries stream=codec_name,profile,width,height,coded_width,coded_height \
  -of default=noprint_wrappers=1 d1_capture.h264
```

Expected:

```text
codec_name=h264
profile=Main
width=688
height=576
coded_width=688
coded_height=576
```

## Internal Sequence

The current working program uses the GADI SDK only for module open/map calls and
raw ioctls for configuration.

SDK open/map order:

```text
gadi_sys_init
gadi_vi_init
gadi_vi_open
gadi_vout_init
gadi_vout_open
gadi_venc_init
gadi_venc_open
gadi_venc_map_bsb
gadi_venc_map_dsp
```

Raw ioctls:

```text
0x80047305  VI_SOURCE_CAPS
0x80047670  GET_CHIP_INFO
0x4004767b  Sofia pre-limits call, integer arg 0
0x40047654  SET_ENCODE_STATE=0
0x80047674  GET_LIMITS
0x40047673  SET_LIMITS with returned buffer
0x40047687  SET_SRCBUF_FORMAT
0x40047683  SET_SRCBUF_TYPE
0x40046533  SET_FRAME_INTERVAL
0x40046528  SET_ENCODE_FORMAT
0xc0046540  GET H264 internal config
0x4004653f  SET H264 internal config
0x40046538  SET bitrate
gadi_vi_enable(vi_handle, 1)
0x40046541  START_ENCODE
0x4004653c  FORCE_IDR
gadi_venc_get_stream(handle, 0xff, &stream)
```

## Key Details

`0x40047687` source-buffer format must match Sofia's layout:

```text
main: 688x576, mode 1
sub1: 352x300, mode 1
sub2: 352x288, mode 1
sub3: disabled, mode 0
interlace_scan: 1
```

Stream mapping observed:

```text
stream_id 0: D1 main stream, 688x576
stream_id 2: CIF/sub stream
```

`gadi_venc_get_stream(..., 0xff, ...)` is used as a wildcard. The returned
`stream.stream_id` must be inspected and only `stream_id == 0` is written for
the main D1 output.

SPS/PPS/IDR are delivered in buffers whose first NAL type can be `7`. Do not
discard these packets while waiting for a separate IDR packet; write them to the
output.

The frame interval ioctl must match Sofia:

```c
mask = 1 << stream_id;
numerator = 60;
denominator = 60;
```

The earlier `1/25` setting made the real capture rate around 1 fps. Re-timing
that raw file to 25 fps made the video appear as fast motion and caused the
test to take minutes to collect 300 frames.

## Known Limitations

- Sofia boot warmup is still required.
- OSD/date overlay is not configured by this path. Sofia sets up OSD separately.
- `START_ENCODE` for mask `0x2` has returned `EINVAL` in this test path, while
  masks `0x1` and `0x4` start.
- Audio is not handled here. The next integration step is to inspect
  `wibox-audio` and combine audio RTP with this H.264 stream.
