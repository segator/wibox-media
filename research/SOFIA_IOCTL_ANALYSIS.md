# Sofia IOCTL Analysis — GK710x Hardware Reverse Engineering

## Overview

Captured 4,969 ioctls across 6 Sofia threads using `sofia_trace` (ptrace tracer with `PTRACE_O_TRACECLONE`).
Two phases captured: boot init + video call activation.

## Thread Map

| PID | Role | Device | Key ioctls |
|-----|------|--------|------------|
| 815 | Main Sofia | `/dev/gk` (fd 9) | GK init, VI init, VENC init, VENC start, VI stop |
| 820 | Watchdog | fd 5 | `WDIOC_KEEPALIVE` (`0x80045705`) |
| 863 | VI Capture | fd 13 | `VI_GetFrame` (`0x80045906`) — 1,270 calls |
| 865 | VENC Encode | fd 9 | `VENC_EncodeFrame` (`0xc004652a`) — 1,082 calls |
| 838 | Network | fd 12 | Socket/ifconfig setup |
| 821,841-844 | Aux threads | various | Network, helpers |

## Device Map

| fd | Device | ioctl type byte |
|----|--------|-----------------|
| 9 | `/dev/gk` (main) | `0x76`(version), `0x73`(SYS), `0x6d`(media), `0x69`(sub), `0x6f`(other), `0x65`(VENC), `0x50`(audio) |
| 13 | `/dev/vi` or subdev | `0x59`(VI) |
| 17 | Second VI device | `0x59`(VI sub) |

## Phase 1: Boot Initialization (~140 ioctls)

### 1.1 GK Subsystem Version Queries (fd 9, type `0x76`)
```
0x80047652 — read version for subsystem 'R'
0x40047654 — write config for subsystem 'T'
0x80047600 — read version for subsystem 0x00
0x80047670 — read version for subsystem 'p'
0x4004767b — write config for subsystem 0x7b
0x40047673 — write config for subsystem 's'
0x40047683 — write config for subsystem 0x83
0x40047687 — write config for subsystem 0x87
0x00007681 — command for subsystem 0x81
```

### 1.2 SYS Configuration (fd 9, type `0x73`)
```
0x40047316 — write SYS param
0x80047301 — read SYS param
0x80047304 — read SYS param
0x4004730b — write SYS param
0x40047304 — write SYS param (arg=0x01388000 = 20,480,000 — likely DSP memory)
```

### 1.3 Media Configuration (fd 9, type `0x6d`)
```
0x80046d05 — read media param
0x80046d00 — read media param
0x80046d04 — read media param
0x80046d10 — read media param
0x80046d20 — read media param
```

### 1.4 Sub System (fd 9, type `0x69`)
```
0x80046920 — read sub param
0x40046921 — write sub param (3 calls with different buffer pointers)
```

### 1.5 Audio (fd 9, type `0x50`)
```
0x80045002 — SOUND_PCM_READ_RATE (8kHz confirmed)
```
No audio streaming ioctls — requires doorbell press to activate analog path.

### 1.6 Other (fd 9, type `0x6f`)
```
0x40046f00 — write loopback/repeat config (8 calls with 4 buffer addresses cycling)
```

## Phase 2: Video Call Activation

### 2.1 VI Initialization (fd 13, type `0x59`)
```
0x40045900 — VI_SetAttr (WRITE)
VI_0x30   — VI_Command30 (arg=0xa0)
0x40045932 — VI_SetParam (WRITE)
0x00005904 — VI_Start? (arg points to buffer)
```

### 2.2 VI Sub Channel (fd 17, type `0x59`)
```
0x40045910 — VI_SetAttr2 (WRITE)
VI_0x30   — VI_Command30 (arg=0x140)
0x40045932 — VI_SetParam (WRITE)
0x40045917 — VI_ExtraConfig (WRITE)
```

### 2.3 VENC Channel Init (fd 9, type `0x65`) — repeated per channel

Per-channel sequence:
```
0x40046528 — VENC_SetAttr (WRITE, arg=buffer ptr)
0x40046533 — VENC_Config3 (WRITE)
0xc0046540 — VENC_GetSet40 (READ|WRITE, arg=buffer ptr)
0x4004653f — VENC_Config3f (WRITE)
0x40046538 — VENC_Config38 (WRITE)
0xc0046545 — VENC_GetSet45 (READ|WRITE)
0x40046544 — VENC_Config44 (WRITE)
```
This sequence runs 3 times (channels 0, 1, 2) with different buffer addresses.

### 2.4 VENC Start (fd 9, type `0x65`)
```
0x40046541 arg=1 — VENC_Start stream 0
0x40046541 arg=2 — VENC_Start stream 1
0x40046541 arg=4 — VENC_Start stream 2
```
NOTE: args are bitmask: channel 0=1, channel 1=2, channel 2=4.

### 2.5 Streaming Loop

**VI Capture Thread (PID 863, fd 13):**
```
0x80045906 (READ) — VI_GetFrame (1,270 calls)
```
Returns in 0x4d-0x4e range (77-78 decimal, maybe frame size/status).

**VENC Encode Thread (PID 865, fd 9):**
Per frame loop:
```
0xc004652a (READ|WRITE) — VENC_EncodeFrame
0xc0046529 (READ|WRITE) — VENC_GetStream
0x80046537 (READ)        — VENC_GetStreamData
```

### 2.6 VI Stop
```
0x40045902 — VI_Stop (WRITE) on fd 13
```

## IOCTL Type Byte Reference

| Type byte | Subsystem | Description |
|-----------|-----------|-------------|
| `0x76` (`v`) | Version/Caps | Query driver versions and capabilities |
| `0x73` (`s`) | SYS | System configuration (memory, clocks) |
| `0x6d` (`m`) | Media | Media pipeline configuration |
| `0x69` (`i`) | Sub system | Secondary subsystem config |
| `0x50` (`P`) | Audio (OSS) | `SOUND_PCM_READ_RATE` |
| `0x6f` (`o`) | Other | Loopback/repeat configuration |
| `0x59` (`Y`) | **VI** | Video Input — sensor, capture |
| `0x65` (`e`) | **VENC** | Video Encoder — H.264 encode |

## VENC IOCTL Command Reference

| Cmd | Name (guessed) | Direction | Description |
|-----|---------------|-----------|-------------|
| 0x28 | SetAttr | WRITE | Set VENC attributes (resolution, format) |
| 0x29 | GetStream | READ\|WRITE | Get encoded stream after encode |
| 0x2a | EncodeFrame | READ\|WRITE | Encode one video frame |
| 0x33 | Config33 | WRITE | Configuration parameter |
| 0x37 | GetStreamData | READ | Read encoded stream data |
| 0x38 | Config38 | WRITE | Configuration parameter |
| 0x3f | Config3f | WRITE | Configuration parameter |
| 0x40 | GetSet40 | READ\|WRITE | Get/set param |
| 0x41 | **Start** | WRITE | Start encoding (arg = bitmask of streams) |
| 0x44 | Config44 | WRITE | Configuration parameter |
| 0x45 | GetSet45 | READ\|WRITE | Get/set param |

## VI IOCTL Command Reference

| Cmd | Name (guessed) | Direction | Description |
|-----|---------------|-----------|-------------|
| 0x00 | SetAttr | WRITE | Set VI attributes |
| 0x02 | Stop | WRITE | Stop VI capture |
| 0x04 | Start | — | Start VI capture |
| 0x06 | **GetFrame** | READ | Get captured frame (streaming loop) |
| 0x10 | SetAttr2 | WRITE | Set attributes for second channel |
| 0x17 | ExtraConfig | WRITE | Additional configuration |
| 0x30 | Command30 | — | VI command (arg varies) |
| 0x32 | SetParam | WRITE | Set VI parameter |

## Key Observations

1. Sofia opens `/dev/gk` once (fd 9) and uses it for ALL subsystems
2. VI has 2 separate devices/channels (fd 13 = main stream, fd 17 = sub stream)
3. VENC initializes 3 channels with identical config but different buffers
4. The streaming loop is: VI captures frame → VENC encodes → get stream data
5. Audio is NOT activated without physical doorbell press
6. No VENC stop ioctl was seen — Sofia might just close the fd
7. The `0x6f` (loopback) writes cycle through 4 buffer addresses — this configures the video pipeline routing

## Replication Strategy for sip_media

1. Open `/dev/gk` → do all `0x76` version queries and `0x73`/`0x6d`/`0x69` init
2. Open VI device → init with `0x59` sequence (0x00 → 0x30 → 0x32 → 0x04)
3. Init VENC 3 channels with `0x65` sequence (0x28 → 0x33 → 0x40 → 0x3f → 0x38 → 0x45 → 0x44)
4. Configure video pipeline routing (`0x6f` writes)
5. Start VENC (`0x41` arg=1|2|4)
6. Streaming: VI_GetFrame → VENC_EncodeFrame → VENC_GetStream → VENC_GetStreamData → RTP
7. Stop: VI_Stop (`0x02`) → close fds

Alternatively, link against `libadi.so` from the GK710x SDK to use the GADI API (gadi_vi_*, gadi_venc_*, etc.) which wraps these ioctls.
