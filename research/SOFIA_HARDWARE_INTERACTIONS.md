# Sofia hardware interactions relevant to WiBox Media

## Purpose and scope

This document audits the decompiled vendor application at `$HOME/Sofia.c` and
compares its hardware-facing behavior with `wibox-media-daemon`. The goal is not
to reproduce Sofia. It is to identify mechanisms that solve an observed product
problem, distinguish them from generic GK710x SDK code, and avoid speculative
writes to shared hardware.

Evidence levels used below:

- **Confirmed**: direct control flow, device access or ioctl visible in
  `Sofia.c`, supported by an existing trace or by current runtime behavior.
- **Inferred**: function name and control flow are clear, but the hardware
  meaning of a value is not independently verified.
- **Library only**: implementation exists in the statically linked SDK, but no
  Sofia call site was found. Presence does not prove use on this board.

The decompilation contains 522251 lines and includes substantial generic DVR,
ONVIF, display, storage and board-support code. Those facilities are not WiBox
requirements unless a live call path reaches them.

## Executive result

| Area | Sofia behavior | Current daemon | Decision |
|------|----------------|----------------|----------|
| UART framing | Parses FB/FC/FD with checksum modulo 0xf0 | Experimental branch now parses all three | Keep passive parser |
| `SysUpToMcu` | Sends mutable `FB 10 xx` state every 2000 ms | Not required for working calls | Do not replicate |
| Call reopen guard | Waits up to 3 s after close before opening again | Tracks active state, but has no post-close cooldown | Strong separate candidate |
| Push state | Caches state and writes only on change | Writes explicit requested state and boot policy | Consider idempotency later |
| Audio format | 8 kHz, 16-bit, mono, 160 samples, 30 frames | Same essential format | Already covered |
| Audio gain | Input 0-100 maps to 0-15; output maps to 0-12 table | Same mapping | Already covered |
| AEC | Registers vendor `ap` AEC and processes AI/AO together | Imports the same AP helper | Already covered |
| Video | Initializes VI, VENC and stream formats in strict order | Warmup plus direct D1 worker | Already covered |
| 3D denoise | Programs VPS stage 2 during VideoManager creation | Sofia warmup already executes first | Do not duplicate without evidence |
| Hardware watchdog | 30 s timeout, fed every 5 s | Shell process supervisor only | Useful, but separate high-risk feature |
| RTC | Reads `/dev/rtc1` and conditionally adjusts system time | Networked firmware uses system/NTP time | Not needed |
| ADC/I2C/SPI/display | Generic SDK implementations and board variants | Not used | Do not import |

## 1. MCU UART

### 1.1 Device setup and framing

Sofia opens `/dev/ttySGK1`, configures it through its UART wrapper and registers
`FUN_000a27b8` as the receive callback from `CUart::Start`. The lower-level
reader accepts three frame families:

| Prefix | Total bytes | Use observed in Sofia |
|--------|-------------|------------------------|
| `FB` | 4 | MCU command/event with one parameter byte |
| `FC` | 7 | Extended MCU payload |
| `FD` | 11 | AACB/version payload |

For every family the final byte is the sum of preceding bytes modulo `0xf0`.
This matches `FUN_0009d0f0` for writes and `FUN_0009d214` for reads. The parser
on `codex/uart-init-experiments` now follows these lengths and checksum rules.
That is useful because it changes observability only; it does not need an MCU
write.

### 1.2 `SysUpToMcu`

`CUart::Start` calls the device-UART start routine. That routine registers the
receive callback and creates a timer named `SysUpToMcu`:

```text
callback: FUN_0009d1d0
period:   2000 ms
frame:    FB 10 <mutable-state> <checksum>
```

The callback copies a three-byte template, replaces its parameter byte from
internal UART state and passes it to the normal checksum/write function.
Observed values include `0x00`, `0x04` and `0x5e`, but the decompilation does
not prove their semantic names.

The likely role is an application-alive or application-state signal to the MCU.
There is no evidence that it is necessary for doorbell detection, SIP, audio,
video, snapshots, forwarding or unlock in the custom firmware. Tests with
one-shot values also did not expose a useful new capability. Therefore:

- do not send the three observed values as an invented initialization sequence;
- do not add the 2-second heartbeat without a concrete missing requirement;
- retain the information for future diagnosis if the MCU is shown to suppress
  a required event when Sofia is absent.

### 1.3 Open/close line cooldown

This is the most actionable UART finding.

Sofia's `CloseDoorBell` sends the close frame and stores `sysinfo.uptime`.
`OpenDoorBell` calls `FUN_000a22d8(..., 3)` before sending the open frame. That
function waits in 500 ms increments until one of these conditions is true:

- 3 seconds have elapsed since the last close;
- it has waited the bounded maximum;
- no previous close timestamp exists.

This protects the analog panel path from an immediate close/reopen cycle. The
daemon currently prevents a duplicate `START_CALL` while its active flag is set,
but does not enforce a cooldown after `STOP_CALL`, `HANG_UP` or another external
close. A separate conservative change should track the last confirmed close and
delay only a subsequent open. This may explain previous frozen video, missing
audio and blue-frame hardware states after rapid tests.

This should not be mixed into the UART observability PR because it changes call
timing and needs physical testing.

### 1.4 Cached push/forward state

Sofia keeps the last `PUSH_STATE` value and sends `FB 19 xx` only when the
requested value differs. Incoming `FB 19` updates the same cache. This gives it
idempotent call-forward control.

The daemon already exposes explicit forward control and receives both state
frames. A future cleanup could suppress a write when the latest known state
already equals the requested state. It must not assume an initial state before
receiving MCU feedback.

### 1.5 Other UART behavior

The receive dispatcher handles or identifies:

- alarm report variants (`FB 11`);
- stream reader state (`FB 14`);
- hang-up parameters 0, 1 and 2 (`FB 13`);
- call-guard response (`FB 15`);
- MCU state (`FB 16`);
- saved address (`FB 18`);
- push state (`FB 19`);
- reset and station-to-AP requests (`FB 20`, `FB 21`);
- stop-ring parameters 0 and 1 (`FB 23`);
- long-press/periodic messages (`FB 24`);
- factory and SSID postfix messages (`FB 25`, `FB 26`);
- debug messages (`FB 66`);
- FC extended frames and FD AACB version frames.

Most of these should remain telemetry. In particular, a known alias is not by
itself justification for rebooting, changing Wi-Fi mode, entering factory mode
or changing `media_state`.

Sofia maps `CMD_STOP_RING` parameters 0 and 1 to two distinct higher-level
notifications. We have physical evidence that parameter 0 corresponds to the
physical handset answering, so the production alias `physical_handset_answered`
is retained. Parameter 1 remains experimental.

### 1.6 F1 and guard behavior

Sofia's F1 operation sends ON and schedules OFF after a configured duration.
The daemon already performs the same bounded pulse. Sofia's guard-call path
waits for UART acknowledgement and retries once after approximately 500-600 ms;
guard calling is not a current product feature, so this adds no value today.

## 2. Audio hardware

### 2.1 Format and buffers

Sofia configures the intercom audio path with the following effective defaults:

```text
sample rate:    8000 Hz
sample width:   16 bit
sound mode:     mono/single
frame samples:  160
frame count:    30
```

It opens the GADI AI device and `/dev/ao_dev`, maps DMA buffers, enables AI/AO
only when required and disables them during teardown. The current daemon uses
the same GADI path and the same 160-sample PCMA cadence.

### 2.2 Gain and volume

Sofia maps input gain percentage to a 0-15 hardware level:

```text
gain_level = percent * 15 / 100
```

It maps output percentage to 0-12 and then uses the vendor volume table. The
daemon's `audio_hw.c` implements both mappings. There is no missing Sofia gain
formula to import.

### 2.3 Echo cancellation

Sofia registers an AEC implementation named `ap`, enables coupled AI/AO AEC and
processes captured frames through that callback. The daemon imports the same AP
helper and aligns its low-level echo/noise path. Automatic gain control is
intentionally not copied because fixed gain avoids amplifying analog pops.

The useful differences already implemented by the daemon are explicit line
mute around open/close transients and configurable fixed gain. No additional
Sofia audio initialization is currently missing.

## 3. Video, encoder and image pipeline

### 3.1 Initialization order

Sofia performs a strict sequence:

1. system/GK media initialization and firmware loading;
2. VI open and source configuration;
3. VOUT setup required by the vendor API;
4. VENC open and channel configuration;
5. stream format, H.264 configuration and QP configuration;
6. VI enable and VENC stream start;
7. frame/query loops;
8. VI/VENC stop and close.

The existing Sofia boot warmup leaves the sensor/media hardware initialized.
The production worker then owns D1 stream 0 at 688x576 and does not need Sofia
per call. Earlier reverse engineering of the ioctls and struct layouts remains
in `SOFIA_IOCTL_ANALYSIS.md`, `KERNEL_ANALYSIS.md` and `FASE3_PROGRESS.md`.

### 3.2 Encoder controls

Sofia exposes or uses bitrate, GOP, QP limits, stream format, H.264 settings,
start/stop and force-IDR operations. The daemon already covers the useful set:

- stream 0 D1 capture;
- configurable bitrate;
- H.264 QP tuning;
- natural GOP plus force-IDR when a SIP/RTSP sink attaches;
- explicit worker lifecycle and sink sharing.

The large set of additional vendor encoder controls is not evidence that they
improve this analog source. They should be introduced only for a measured
decoder, bandwidth or latency problem.

### 3.3 3D denoise and image controls

During `VideoManager::create`, Sofia calls the YUV-input 3D denoise routine after
VI is enabled. The implementation opens `/dev/gk_video`, reads resolution data,
builds a large VPS stage-2 structure and sends ioctl `0x40047026`.

This matters for interpretation of earlier denoise tests:

- Sofia warmup already executes before the daemon;
- its VPS configuration can persist in the kernel/media pipeline;
- adding a second runtime denoise control may be a no-op, may overwrite only
  part of the structure, or may not address analog sensor noise;
- visual tests did not show a useful improvement.

Sofia also contains APIs for brightness, contrast, saturation, hue, exposure,
white balance, WDR, smart IR and day/night switching. Much of this is generic
IP-camera ISP functionality. The WiBox camera is PAL CVBS converted to digital,
so controls designed for a directly attached Bayer sensor may be irrelevant or
operate after the dominant analog noise has already been introduced.

No image-control feature should return to production without a repeatable A/B
capture showing improvement and stable video/audio calls.

### 3.4 `/proc/goke/video_sync`

Sofia's ISP subsystem opens `/proc/goke/video_sync` and uses blocking reads to
drive per-frame ISP/3A work, with a write used to wake the thread on shutdown.
This is not the SIP audio/video synchronizer. The daemon does not run Sofia's
ISP 3A controller and therefore does not need this interface.

## 4. Platform and board hardware

### 4.1 Hardware watchdog

Sofia starts `/dev/watchdog`, sets a 30-second heartbeat and feeds it every
5 seconds. Its reboot fallback reduces the watchdog timeout to 1 second if a
normal reboot command fails.

The custom firmware currently has `app_watchdog.sh`, which restarts the daemon
after a process exit but cannot recover a kernel deadlock or a process that is
alive but stuck. A hardware watchdog could improve unattended reliability, but
it is a separate high-risk feature because:

- OTA flashing and controlled reboot paths must keep feeding or explicitly
  hand off the watchdog;
- a bug in the feed thread creates a reboot loop;
- development binaries and recovery sessions need an escape mechanism;
- ownership must be singular across Sofia warmup, updater and daemon.

If implemented, it should be opt-in first, expose feed/timeout metrics and be
tested through updater, reboot, daemon crash and network-loss scenarios.

### 4.2 GPIO

The linked SDK includes generic sysfs GPIO export/read/write/pulse helpers.
Current board initialization already configures GPIO 10/11/12 for the Wi-Fi
LED, GPIO 18 for the audio chip, and additional board lines through `gpio.sh`.
The daemon directly toggles configured GPIO 18 around audio lifecycle.

No additional Sofia GPIO operation has been tied to a missing WiBox feature.
Blindly importing GPIO numbers from generic board profiles risks driving a
power, reset or peripheral line incorrectly.

### 4.3 RTC

Sofia opens `/dev/rtc1`, validates RTC fields and conditionally adjusts system
time. The custom firmware already operates as a networked appliance and uses
system time for MQTT and logs. Direct RTC ownership adds no current product
benefit and could fight network time synchronization.

### 4.4 ADC, I2C, SPI and display

The binary contains generic implementations for:

- two-channel ADC reads from `/dev/adc`;
- I2C buses 0, 1 and 2;
- SPI display access through `/dev/spidev0.0`;
- framebuffer, LCD timing, backlight and panel GPIO;
- firmware and media devices `/dev/gk_fw` and `/dev/gk_video`.

No call site was found for the ADC wrapper, and display/backlight branches are
selected by generic product profiles. The WiBox service does not have a local
display requirement. These facilities should not be moved into the daemon.

## 5. Prioritized actions

### Safe in this experimental PR

1. Keep passive FB/FC/FD parsing and checksums.
2. Publish raw bytes, known alias, parameter and direction over MQTT.
3. Keep unknown families observable without assigning product behavior.
4. Remove every automatic `FB 10` probe.

### Best separate candidate

Add a bounded 3-second post-close cooldown before `START_CALL`, based on Sofia's
`CloseDoorBell`/`OpenDoorBell` behavior. It should cover local close, UART hangup
and external close state, and must be validated with physical and simulated
rings plus SIP/RTSP snapshots.

### Potential reliability project

Evaluate the hardware watchdog only after defining ownership and OTA/recovery
behavior. It should not be bundled with UART decoding.

### Explicitly rejected without new evidence

- periodic `SysUpToMcu` heartbeat;
- invented one-shot Sofia initialization sequence;
- ISP controls or denoise profiles without visual A/B evidence;
- generic ADC/I2C/SPI/display integration;
- changing media state for every newly decoded UART alias.

## 6. Remaining unknowns

- Exact semantics of `SysUpToMcu` values `0x00`, `0x04` and `0x5e`.
- Exact distinction between `CMD_STOP_RING` parameters 0 and 1.
- Whether FC frames carry a useful WiBox-specific payload on this MCU version.
- Whether the FD AACB version should become a retained diagnostic sensor rather
  than remain an event.
- Whether the 3-second close/open guard alone prevents the observed transient
  hardware lockups.

These unknowns can be resolved with passive logs or narrowly scoped physical
tests. None requires reproducing Sofia wholesale.
