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
- The working path is integrated into `src/sip_media/video_worker.c` and runs as
  part of `wibox-media-daemon`.

Still not fully independent:

- Sofia is still needed as a one-time hardware warmup after boot.
- Sofia is not needed per call once the driver/hardware state is initialized.
- The full VI/sensor/decoder init sequence still needs to be reproduced to remove
  the boot warmup dependency.

## Operational Flow Inside The Daemon

Current reliable flow:

1. Boot the WiBox.
2. `run.sh` starts `Sofia_temp.sh` for the one-time video hardware warmup.
3. `run.sh` starts `wibox-media-daemon`.
4. For each call:
   - `wibox-media-daemon` sends the MCU "start call" command;
   - the in-daemon video worker starts the D1 capture path;
   - H.264 NAL units are packetized as RTP and sent to the negotiated SIP peer;
   - `wibox-media-daemon` sends the MCU "stop call" command on hangup.

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

## Test Entry Points

The standalone capture PoC has been removed. Use the daemon-local test API:

```sh
echo 'VIDEO_TEST 192.168.0.183 4014 5' > /tmp/pipe_sip
```

Expected daemon behavior:

```text
start panel call context
initialize D1 video worker
capture stream_id == 0 frames
send RTP H.264 to 192.168.0.183:4014
stop panel call context
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
- Audio is handled separately by `src/sip_media/audio_hw.c` and shares the SIP
  call lifecycle with this video worker.
