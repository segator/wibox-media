# Fase 3: Replicar HW init de Sofia en sip_media

## 2026-06-28 — GK queries funcionando sin Sofia

### Confirmado
- `/dev/gk_video` se abre sin problemas (fd=3)
- ioctls de versión responden: `0x76,0x52` → 0, `0x76,0x54` OK, `0x76,0x70` → 0x4
- No hace falta libadi.so — ioctls crudos funcionan
- Sofia debe estar KILL antes de abrir el device (lo tiene en exclusiva)

### Problemas encontrados
- `_IOW` con arg=0 funciona para unos ioctls pero SEGV para otros
- Hay que pasar structs válidos (punteros a buffers alocados) para algunos ioctls
- Compilar con `-static -Os` en wibox-build:latest funciona

### Próximo
- Completar secuencia GK init (SYS 0x73, media 0x6d, sub 0x69, audio 0x50)
- Añadir VI init (0x59) — necesita structs con parámetros de sensor
- Añadir VENC init (0x65) — necesita structs con codec params
- Probar streaming (VI capture + VENC encode)

## 2026-06-28 (cont) — GK init completo: 17 ioctls OK

ioctls funcionando sin Sofia:
- Version: 0x76(0x52,0x54,0x70,0x74,0x73)
- SYS: 0x73(0x16,0x01,0x04,0x0b)
- Media: 0x6d(0x05,0x00,0x04,0x10,0x20)
- Sub: 0x69(0x20,0x21)
- Audio: 0x50(0x02) → raw=0x80000

Bug encontrado: los ioctls _IOW pasan buffers al kernel para DMA.
Si se hace free() o return 0 normal, el runtime de libc intenta
limpiar y da SEGV. Solución: _exit(0) + heap buffers (no stack).

Próximo: VI init (0x59) + VENC init (0x65) con buffers mmap para DMA.

## 2026-06-28 — Investigación VI/VENC

Sofia solo abre /dev/gk_video UNA vez (fd 8 en el proceso principal).
Los fds para VI (13, 17) y VENC child encoding son de THREADS hijos.
El orden de init es: GK queries → SYS → media → VENC channels → VI open → VI init → VI start.

Los ioctls VI (0x59) no funcionan en un simple segundo open() de gk_video
sin haber completado antes el init de GK + VENC.

Próximo: implementar la secuencia completa de init con el orden correcto.

## 2026-06-28 — Bloqueo en VENC/VI

VENC channel init (0x65,0x28) bloquea porque los structs necesitan
valores válidos (resolución, codec, bitrate, GOP, etc.). No podemos
pasar buffers con ceros — el driver del kernel se bloquea.

Opciones para resolver:
1. Linkar contra libadi.a del SDK (necesita adi_types.h + adi_sys.h que faltan)
2. Dumpear memoria de Sofia para leer los structs que pasa a ioctls
3. Extraer struct layouts del binario de Sofia con ingeniería inversa

La opción más rápida: usar nuestro sofia_trace modificado para dumpear
los primeros N bytes del buffer en cada ioctl _IOW. Así obtenemos
los valores exactos que Sofia usa.

## 2026-06-28 — Tracer v4 con buffer dumper

Añadido dump de buffers ioctl con PTRACE_PEEKDATA:
- Solo dumpea _IOW (WRITE) y _IOWR (READ|WRITE)
- Extrae el tamaño del comando: size = (cmd >> 16) & 0x3FFF
- Cap de 256 bytes por dump para no saturar

Bug corregido: el direction check estaba al revés (skip WRITE en vez de READ)

Para extraer datos de VENC/VI:
1. Recuperar WiBox vía serial
2. Arrancar con tracer v4 
3. Esperar a que Sofia termine init de boot
4. El tracer captura los structs de VENC (0x65) y VI (0x59)

## 2026-06-28 — Buffer dumps funcionando (7367 líneas, 221KB)

Datos extraídos de ioctls WRITE:
- VI 0x00: 0x00001f40 (8000) — buffer size or width
- VI 0x32: 0x71790000, 0x71788000 (canales diferentes) 
- VENC 0x33/0x28/0x38: enteros 1,2,3 (channel IDs)
- La config real de structs (resolución, codec, bitrate) se hace
  antes vía funciones de setup del SDK, no como ioctls individuales

El enfoque de replicación necesita entender el setup inicial del
GADI SDK (gadi_venc_set_channels_params y equivalentes).

## 2026-06-29 — SDK GADI funciona (pan602389160/gk7102)

Headers completas encontradas en gk7102_sdk/src/adi/include/:
- adi_types.h, adi_sys.h, adi_venc.h, adi_vi.h, adi_vout.h, adi_audio.h

PoC compila contra libadi.a y funciona en el WiBox:
- gadi_sys_init() ✓
- gadi_vi_init() + gadi_vi_open() ✓ 
- gadi_vout_init() + gadi_vout_open() ✓
- gadi_venc_init() + gadi_venc_open(vi, vo) ✓
- SEGV en gadi_venc_set_channels_params/stream_format/h264_config

No necesita Sofia — módulos kernel (media.ko, etc.) son suficientes.
Firmware (gk_fw.bin) ya está en /lib/firmware/.

## 2026-06-29 — GADI SDK: GET funciona, SET crashea

### Funciona
- gadi_sys_init() ✓
- gadi_vi_init() + gadi_vi_open() ✓  
- gadi_vout_init() + gadi_vout_open() ✓
- gadi_venc_init() + gadi_venc_open(vi, vo) ✓
- gadi_venc_print_params() ✓
- gadi_venc_get_channel_state() ✓
- gadi_venc_get_channels_params() ✓ — chan1=1 w=688 h=576
- gadi_venc_get_stream_format() ✓
- gadi_venc_get_h264_config() ✓

### Crashing (SEGV)
- gadi_venc_set_channels_params() — SEGV en libadi, ioctl OK
- gadi_venc_set_stream_format() — SEGV
- gadi_venc_set_h264_config() — SEGV

### Diagnóstico
libadi.a R10973 (nuestro SDK v2.0.0) tiene bug en funciones SET.
Sofia usa R13210 (funciona). Los ioctls pasan (el DSP recibe los datos)
pero la lib crashea al retornar.

Headers correctos combinados:
- adi_types.h, adi_sys.h, basetypes.h → del repo gk7102
- adi_venc.h, adi_vi.h → de nuestro SDK v2.0.0 (match libadi.a)
- Structs: sizeof(GADI_VENC_ChannelsParamsT)=48 (verificado por disassembly)

### Próximo
- Extraer libadi de Sofia (R13210) o encontrar SDK v2.1+
- O usar GET con SDK + SET con ioctls crudos (tenemos los datos del trace)

## 2026-06-29 — Investigación SDK/Ghidra

### Chip correcto
- WiBox usa GK7102S (ARM1176JZF-S, ARMv6)
- Nuestro SDK v2.0.0 target: `arm11-gcc-uClibc-linux-GK710XS` — correcto
- Kernel modules son R13210 (Feb 2018)
- Nuestra libadi.a es R10973 (Nov 2016)
- 14 meses de diferencia — la interfaz ioctl de escritura cambió

### Experimentos
- SEGV bypass con sigsetjmp: funciona pero los datos NO cambian
  → El SEGV ocurre ANTES de que libadi envíe el ioctl
  → Las funciones SET están rotas estructuralmente, no solo en el return path

### Ghidra
- Ghidra headless import: OK (165s analysis)
- Ghidra REST API (Docker biniamfd/ghidra-headless-rest): OK pero limitado 
  (100 funciones max, no encuentra strings GADI)
- PyGhidra: abre GUI (no útil)
- objdump completo: 800K líneas, 32MB
- Error strings encontradas en .rodata (VAs 0x0035b68f, 0x0035b711, 0x0035b75f)
- Literal pools referenciados en función "error" (que es enorme, >100KB)
- ioctl dinámico: Sofia llama a ioctl@0x303ce0 dinámicamente

### Próximo
- Encontrar SDK R13210 (GK710X v2.1+)
- O extraer ioctls de Sofia (muchos bl <ioctl> encontrados)
- Posible source: pan602389160/gk7102 con versión más reciente

## 2026-06-29 (cont) — IoCTL comparison

### Nuestra libadi.a (R10973) vs Sofia (R13210) — ioctls IDÉNTICOS

| Función | Nuestra libadi.a | Sofia trace |
|---------|-----------------|-------------|
| venc_get (nr=0x00) | 0x80046d00 READ | 0x80046d00 READ |
| venc_get (nr=0x04) | 0x80046d04 READ | 0x80046d04 READ |
| venc_get (nr=0x05) | 0x80046d05 READ | 0x80046d05 READ |
| encode (nr=0x42) | 0x40046542 WRITE | 0x40046542 WRITE |

IoCTL commands son los mismos. La dirección (READ/WRITE) también coincide.

### 71 ioctls extraídos de adi_venc.o
Mayoría usan tipo 'e' (0x65 = encode) y 'v' (0x76 = video).
Solo 5 usan tipo 'm' (0x6D = media/VENC directo):
- 0x00006d08 (NONE, nr=8) — venc_close
- 0x00006d09 (NONE, nr=9) — venc_close
- 0x80046d04 (READ, nr=4) — venc_get
- 0x80046d05 (READ, nr=5) — venc_get_channel_state
- 0x80046d00 (READ, nr=0) — venc_get_channels_params

### Conclusión
Los ioctl codes NO son el problema — son idénticos.
El SEGV está en el código de libadi R10973 ANTES del ioctl.
Posible causa: struct layouts diferentes entre R10973 y R13210
que causan acceso a memoria inválida durante la preparación del ioctl.

## 2026-06-29 — Sofia decompilado (Ghidra) y direct-ioctl

### Sofia decompilado
- Exportado a ~/Sofia.c (522K líneas, 14MB) desde Ghidra
- Funciones GADI VENC identificadas en decompilado:
  - `FUN_0026e800` = gadi_venc_set_channels_params
  - `FUN_0026f08c` = gadi_venc_set_stream_format
  - `FUN_0026f51c` = gadi_venc_set_h264_config
  - `FUN_0026d8d0` = helper sync (llamado por todas las SET)
  - `FUN_0026f404` = gadi_venc_get_h264_config

### IoCTL mapping R13210 (Sofia) vs R10973 (nuestra)

| IoCTL | Nombre | R13210 | R10973 | Status |
|-------|--------|--------|--------|--------|
| 0x80047670 | GET_CHIP_INFO | ✅ Sofia | ❌ ausente | NUEVO en R13210 |
| 0x40047687 | SET_SRCBUF_FORMAT | ✅ Sofia | ✅ nuestra | COMMAND OK, struct EINVAL |
| 0x40047683 | SET_SRCBUF_TYPE | ✅ Sofia | ✅ nuestra | FUNCIONA |
| 0x80047674 | GET_SYS_RESOURCE | ✅ Sofia | ✅ nuestra | FUNCIONA |
| 0x40047673 | SET_SYS_RESOURCE | ✅ Sofia | ✅ nuestra | FUNCIONA |
| 0x40046533 | SET_FRAME_INTERVAL | ✅ Sofia | ✅ nuestra | no probado |
| 0x40046528 | SET_ENCODE_FORMAT | ✅ Sofia | ✅ nuestra | no probado |
| 0xc0046540 | GETSET_H264_CFG | ✅ Sofia | ✅ nuestra | no probado |
| 0x4004653f | SET_H264_CFG | ✅ Sofia | ✅ nuestra | no probado |
| 0x40046538 | SET_BITRATE | ✅ Sofia | ✅ nuestra | no probado |

### Bloqueo: 0x40047687 (set source buffer format)
- El comando ioctl es correcto (ambas versiones lo usan)
- Siempre retorna -1 EINVAL (invalid argument)
- Probados structs de 25, 27, 28, 32, 48 bytes — todos EINVAL
- La función original R10973 construye un struct en sp+16 con datos de venc_check_resource
- venc_check_resource valida dimensiones contra límites del sistema

### Hipótesis
El struct para 0x40047687 es un formato interno del driver que NO coincide
con GADI_VENC_ChannelsParamsT. Sofia construye un struct específico byte a byte.
Necesitamos el layout exacto desde el código decompilado o desde el kernel module.

### Próximo
- Extraer struct layout de Sofia decompilado (trazar byte-a-byte desde local_6c)
- O analizar media.ko para encontrar el handler de 0x40047687

## 2026-06-29 — Direct ioctl approach: partial success

### Working direct ioctls (from Sofia decompilation)
| IoCTL | Nombre | Struct | Status |
|-------|--------|--------|--------|
| 0x40046533 | SET_FRAME_INTERVAL | `{int mask; u8 num; u8 den}` | ✅ OK |
| 0x40046538 | SET_BITRATE | `{int mask; int brc; int cbr; int vbrMin; int vbrMax}` | ✅ OK |
| 0xc0046540 | GETSET_H264_CFG | internal handle struct | ✅ OK (ret=0) |
| 0x40047683 | SET_SRCBUF_TYPE | `{int[4] types}` | ✅ OK |
| 0x80047670 | GET_CHIP_INFO | `{int chip_type, int vi_handle}` | ✅ OK |

### Not working
| IoCTL | Issue |
|-------|-------|
| 0x40047687 | SET_SRCBUF_FORMAT | EINVAL — exact struct layout unknown |
| 0x40046528 | SET_ENCODE_FORMAT | EINVAL — needs internal handle state |
| 0xc0046540 | SET_H264_CFG | EINVAL — needs internal handle state |
| 0x40047689 | SET_PREVIEW_FMT | not tested yet |

### SDK SET functions
All 3 SDK SET functions (set_channels_params, set_stream_format, set_h264_config) 
crash with SEGV in R10973. The SEGV happens BEFORE the ioctl, so data never 
reaches the driver.

### Sofia struct analysis (from disassembly)
- 0x40047687 struct at sp+28: 37 bytes, complex interleaved pattern
  {w1,h1,d1, w2,h2, sub_w,sub_h,d2, w3,h3, sub_w,sub_h,d3, w4,h4, sub_w,sub_h,d4, intlc}
  Sub-buffer dimensions are always chan1Width x chan1Height

### Path forward
Option A: Accept R10973 limitations — use SDK for init + direct ioctl for frame/bitrate
Option B: Find R13210 SDK
Option C: Extract full VENC state machine from Sofia and replicate internal handle

## 2026-06-29 — Raw fd breakthrough!

### Key finding: raw /dev/gk_video fd works better than SDK VENC

Opening `/dev/gk_video` directly (without gadi_venc_init/open) makes MORE
ioctls work. The SDK's gadi_venc_open appears to initialize state that BLOCKS
subsequent configuration ioctls.

| IoCTL | Via SDK handle | Via raw fd | 
|-------|---------------|------------|
| 0x80047670 GET_CHIP_INFO | ✅ | ✅ |
| 0x40046533 SET_FRAME_INTERVAL | ✅ | ✅ |
| 0x40046538 SET_BITRATE | ✅ | ✅ |
| 0xc0046540 GETSET_H264 | ✅ | ✅ |
| 0x4004653f SET_H264 | ❌ EINVAL | ✅ ret=0 |
| 0xc004652a START_STREAM | not tested | ✅ ret=0 |
| 0x40047687 SET_SRCBUF | ❌ EINVAL | not tested yet |

### New approach: bypass gadi_venc_open completely

Sequence:
1. gadi_sys_init() + gadi_vi_init/open + gadi_vout_init/open  (SDK works)
2. open("/dev/gk_video")  (raw fd)
3. ioctl GET_CHIP_INFO → raw fd
4. ioctl GET/SET_SYS_RESOURCE → raw fd  
5. ioctl SET_FRAME_INTERVAL → raw fd
6. ioctl SET_BITRATE → raw fd
7. ioctl GETSET_H264 → raw fd
8. ioctl SET_H264 → raw fd
9. ioctl START_STREAM → raw fd

This bypasses the entire libadi VENC layer. The kernel modules handle
everything through the raw device fd.

## 2026-06-29 — Full pipeline test results

### Pipeline status with raw fd

| Step | Method | Status |
|------|--------|--------|
| sys/vi/vout init | SDK (R10973) | ✅ |
| get_chip_info | raw ioctl | ✅ chip=4 |
| resource sync | raw ioctl | ✅ |
| set_srcbuf_type (0x40047683) | raw ioctl | ✅ |
| set_srcbuf_format (0x40047687) | raw ioctl | ❌ EINVAL |
| set_frame_interval (0x40046533) | raw ioctl | ✅ |
| set_bitrate (0x40046538) | raw ioctl | ✅ |
| getset_h264 (0xc0046540) | raw ioctl | ✅ (wrong struct) |
| set_h264 (0x4004653f) | raw ioctl | ✅ |
| map_bsb (0xc0047686) | raw ioctl | ❌ (needs srcbuf_fmt) |
| map_dsp (0x80046d04) | raw ioctl | ✅ 7.3MB buffer |
| start_stream (0xc004652a) | raw ioctl | ✅ ret=0 |
| stop_stream (0x80047652) | raw ioctl | ✅ ret=0 |
| get_stream (0x80046537) | raw ioctl | ❌ EFAULT (needs BSB) |

### Blocker: 0x40047687 (set_srcbuf_format)
- Sofia trace shows this ioctl at #115 with 4-byte buffer [024002b0]
- The 4-byte buffer is a VALUE, not a struct pointer
- Likely format: packed enable/handle bits
- Without this ioctl, map_bsb fails and get_stream can't work

### Attempts
- 37-byte Sofia struct: EINVAL
- 48-byte channels struct: EINVAL
- 4-byte values: EINVAL
- libadi.so dynamic link: pthread symbol resolution fails on WiBox

### Next: decode 0x40047687 from Sofia's actual usage
The 4 bytes [02 40 02 b0] from Sofia's trace at ioctl #115 encode
the source buffer configuration. Need to map these bytes to their
meaning by analyzing Sofia's code flow.

## 2026-06-29 — FIRST H.264 FRAMES CAPTURED!

### Breakthrough: encoder produces real H.264 data

Using hybrid approach (SDK for map_bsb + raw fd for config/start):
- `gadi_venc_get_stream()` returns REAL H.264 NAL units
- Start codes `00 00 00 01` followed by NAL header `41` (P-slice)
- Frame sizes: 14-22 bytes (fragments, not complete frames)
- Encoder is producing data but at sub-optimal configuration

### Hex dump of first capture (78 bytes):
```
00 00 00 01 41 e0 cd 5c 2d ff 00 00 52 40 00 00
00 01 41 e0 cd 5c 2d ff 00 00 03 00 00 03 00 00
03 00 0c f8 00 00 00 01 41 e0 5d 5c 2d ff 00 00
52 40 00 00 00 01 41 e0 dd 5c 2d ff 00 00 52 40
00 00 00 01 41 e0 6d 5c 2d ff 00 00 52 40
```

### Remaining issue: srcbuf_format (0x40047687)
Without correct configuration, encoder produces tiny fragments. 
Sofia trace shows this ioctl uses a 4-byte packed value [024002b0].
Need to decode this value to complete the VENC setup.

### Working pipeline:
SDK: sys_init + vi_open + vout_open + venc_open + map_bsb
Raw fd: config (frame_interval, bitrate, h264) + start_stream
SDK: get_stream (returns real H.264 data!)

## 2026-06-29 — Video capture works but unreliable

### Factory defaults produce real H.264
Without any manual configuration, the factory's pre-configured VENC produces:
- SPS + PPS + IDR frames with valid NAL units (00 00 00 01 47/48/45)
- First frame: 1779 bytes I-frame (stream 2)
- Sub-streams: 14-22 byte metadata NAL units
- 190 frames captured in one test run

### Reliability issue
- First run: captures frames successfully
- Subsequent runs: all get_stream calls return errors
- Likely cause: driver state doesn't reset between runs
- The SDK's gadi_venc_get_stream uses static internal state
- After stop_stream + restart, state is corrupted

### Video file analysis
- capture_factory.h264: 19KB, valid H.264 with SPS/PPS/IDR
- First 4 bytes: 00 00 00 01 47 (SPS NAL)
- Mixed streams: need to filter by stream_id for clean video

### Working recipe
1. Fresh WiBox reboot
2. Load modules + gadi_sys_init + vi/vout/venc open  
3. SDK gadi_venc_map_bsb
4. Raw fd ioctl START_STREAM (0xc004652a)
5. SDK gadi_venc_get_stream with stream_id=0xFF
6. Save frames to file

Needs fresh state each run — can't restart without reboot.

## 2026-06-29 — Working video capture! (sub-stream)

### Recipe for video capture (sub-stream only)
1. Reboot WiBox (clean VENC state)
2. Start Sofia for 25s (fully initializes VENC, starts all 3 encoders)  
3. Kill Sofia
4. Our program: map_bsb via SDK, start_stream via raw fd, force_idr
5. Capture with gadi_venc_get_stream (stream_id=0xFF)

### Results
- 150 frames in 10 seconds (~12KB total)
- P-slices only (NAL type 1: 0x41), no SPS/PPS/IDR  
- Sub-stream 2 dominates (352x288), main stream consumed by Sofia
- Frame sizes: 14-89 bytes (sub-stream, low bitrate)

### Limitations
- Main stream (688x576) not accessible because Sofia consumes it
- Need to configure our own encoder for clean main stream capture
- Requires working 0x40047687 (srcbuf_format) to bypass Sofia entirely
- All SET functions in R10973 SDK crash before ioctl

### Next milestone
- Main stream D1 capture: need to make 0x40047687 work
  OR extract the main stream before Sofia consumes it
  OR start our own encoder without Sofia

## 2026-06-29 — Video plays! Mixed streams issue confirmed

### Visual confirmation
User reports: video shows higher resolution first (D1 688x576), then switches
to lower resolution (CIF 352x288). This confirms:
1. Video IS playable ✓
2. Multiple streams are being captured together
3. Main stream (stream 0, D1) produces bigger frames
4. Sub-stream (stream 2, CIF) produces smaller frames
5. get_stream(0xFF) captures ALL streams mixed

### Solution for clean video
Filter by stream_id in get_stream:
- stream 0: main (688x576, D1)
- stream 1: sub1 (352x288, CIF)
- stream 2: sub2 (352x288, CIF)

Capture only stream 0 for clean D1 video.
But specific stream_id get_stream fails (returns errors).
Only 0xFF (all streams) works.

### Hypothesis
The SDK's gadi_venc_get_stream has a bug with specific stream IDs.
The 0xFF wildcard path works because it bypasses the stream matching logic.
Next: fix stream ID filtering to get clean single-stream video.

---

## 2026-06-29 — media.ko reverse engineering (SESIÓN ACTUAL)

### Objetivo de la sesión
Resolver el bloqueo de `0x40047687` (set_srcbuf_format) que siempre devuelve EINVAL,
y capturar video D1 (688x576) limpio del stream principal.

### Método: reversear media.ko directamente

**media.ko extraído del WiBox** (303KB, not stripped — tiene símbolos):
```bash
sshpass -p 'qv2008' ssh root@192.168.0.196 "cat /ko/media.ko | base64" | base64 -d > /tmp/media.ko
arm-goke-linux-uclibcgnueabi-objdump -d media.ko > /tmp/media_disasm.txt  # 55382 líneas
```

### Hallazgo 1: Dispatch table de media_drv_ioctl

El ioctl principal `media_drv_ioctl` despacha por el byte de tipo (bits [15:8]):
```
type=0x69 ('i') → VI_ADAPTER_Ioctl
type=0x73 ('s') → VI_SOURCE_Ioctl
type=0x24 ('$') → VO_SOURCE_Ioctl
type=0x23 ('#') → VO_SINK_Ioctl
type=0x76 ('v') → SYS_Ioctl          ← 0x40047687 va aquí
type=0x65 ('e') → VENC_ENCODE_Ioctl  ← START/STOP/GET_STREAM van aquí
type=0x64 ('d') → VDEC_Ioctl
type=0x70 ('p') → ISP_Ioctl
type=0x6f ('o') → REGION_OVERLAY_Ioctl
type=0x50 ('P') → REGION_PM_Ioctl
```

**Corrección importante:** `0x40047687` NO va a VENC_ENCODE (type=0x65) sino a SYS_Ioctl (type=0x76).

### Hallazgo 2: Handler de 0x40047687 — struct de 40 bytes

Rastreado el árbol de búsqueda binaria en `SYS_Ioctl`:
```
0x40047689 - 2 = 0x40047687 → handler en 0x1b9f0 → llama sys_encode_guard_task+0x290
```

La función `sys_encode_guard_task+0x290` (`0x177ec`):
1. Hace `__copy_from_user(fp-104, user_ptr, **40 bytes**)` ← NO son 4 bytes como dice _IOC_SIZE!
2. El `_IOC_SIZE(0x40047687) = 4` es MENTIRA — el driver copia 40 bytes internamente

**Layout del struct de 40 bytes** (derivado de Ghidra + disasm de media.ko):

| Offset | Tipo | Campo | Descripción |
|--------|------|-------|-------------|
| 0 | u16 | main_width | Debe ser múltiplo de 16 (e.g., 688) |
| 2 | u16 | main_height | Debe ser par (e.g., 576) |
| 4 | u16 | ch_mode_0 | Modo canal 0 (0..2) |
| 6 | u16 | sub1_w | Ancho sub-canal 1 (≤ main_width) |
| 8 | u16 | sub1_h | Alto sub-canal 1 (≤ main_height) |
| 10 | u16 | main_w_dup | = main_width (validado ≤ main_width) |
| 12 | u16 | main_h_dup | = main_height |
| 14 | u16 | ch_mode_1 | |
| 16 | u16 | sub2_w | |
| 18 | u16 | sub2_h | |
| 20 | u16 | main_w_dup2 | = main_width |
| 22 | u16 | main_h_dup2 | |
| 24 | u16 | ch_mode_2 | |
| 26 | u16 | sub3_w | |
| 28 | u16 | sub3_h | |
| 30 | u16 | main_w_dup3 | = main_width |
| 32 | u16 | main_h_dup3 | |
| 34 | u16 | ch_mode_3 | |
| 36 | u8 | interlace_scan | 0=progresivo, 1=entrelazado |
| 37-39 | u8[3] | pad | |

Confirmado por Sofia.c (Ghidra): `FUN_0026e800 → ioctl(*param_1, 0x40047687, &local_6c)`.

### Hallazgo 3: Validaciones del driver

La función `sys_encode_guard_task+0x290` hace 9 validaciones en orden:

1. `main_width & 0xf == 0` → debe ser múltiplo de 16 ← **688 ÷ 16 = 43 ✓**
2. `main_height & 1 == 0` → debe ser par ← **576 es par ✓**
3. `sys_state[0xf8] >= main_width` → max_width del hardware ← **PASA** (.data[0xf8]=0x6976=26998 >> 688)
4. `sys_state[0xfa] >= main_height` ← **PASA** (.data[0xfa]=0x6564=25956 >> 576)
5. `SYS_MEM_Exit+0x16c(ch_idx, width, height, ...)` → validación de memoria disponible
6. `sub_widths/heights <= main_width/height` (3x) ← **PASA** si usamos 688x576 en todos
7. `sys_state[0x14] == 1 → ch_mode_0 <= 2`
8. Si `sys_state[0x0c] == main_width && sys_state[0x0e] == main_height && sys_state[0x18] == ch_mode_0` → return 0 (ya configurado, no-op)
9. `sys_state[0xd0] == 0` → encoder debe estar PARADO ← **POSIBLE FALLO**

### Hallazgo 4: SYS_MEM_Exit+0x16c necesita sys_state[0xdc] no-null

La función `SYS_MEM_Exit+0x16c` (0x15b40) usa `r5 = sys_state[0xdc]` para:
```
ldrh ip, [r5, #0x126]   ← si r5=NULL → lee de addr 0x126 → valores incorrectos
ldrh r3, [r5, #0x124]   ← ídem
```

Si `sys_state[0xdc] = 0` (NULL), lee valores 0, y la validación:
```
cmp r1(688), r3(0) → 688 > 0 → bhi → EINVAL: "width exceeds hardware limits"
```

**Este es el verdadero EINVAL.** `sys_state[0xdc]` apunta a la estructura de estado VI, que se inicializa cuando VI está activo. Después de matar Sofia, este puntero puede ser NULL si el sistema no hace VI init propio.

### Hallazgo 5: Ioctls de reset incorrectos

**Error previo:** Usaba `0x80047652` como "STOP_STREAM" → en realidad es `_IOR('v', 0x52)` = GET_VERSION (lectura).

**Correcto:**
- `0x40047654` = `_IOW('v', 0x54)` = SET_ENCODE_STATE = reset del encoder
  - Valor 0 = reset/stop, confirma en sofia_ioctls_captured.log: `_IOC(_IOC_WRITE, 0x76, 0x54, 0x04), 0` al inicio
  - Handler en SYS_Ioctl: `ldr r4, [r3, #208]` = lee encode_state actual, decide acción
  - Si encode_state=2 (streaming) → llama `VENC_ENCODE_Ioctl` para parar

### Hallazgo 6: El global sys_state es .data section base

Ambas referencias:
- text[17e6c]: `R_ARM_ABS32 .data+0` (sys_encode_guard_task, nuestro ioctl)
- text[c770]: `R_ARM_ABS32 .data+0` (VI_CORE_Add_Adapter)

Apuntan al MISMO struct en `.data`. El struct es el estado global del sistema multimedia.

`.data[0xdc]` (campo 0xdc del global state) = puntero al VI subsystem.  
Es NULL en el .ko file → se inicializa en runtime por VI_Init/VI_CORE_Add_Adapter.

### Hallazgo 7: tv_probe escribe .bss, no .data[0xf8]

La única instrucción `strh r3, [r4, #248]` está en `tv_probe` (TV decoder init).
Su `r4` viene de `kmem_cache_alloc` y se guarda en `.bss+0` (TV device singleton).
NO escribe a `.data[0xf8]`.

`.data[0xf8] = 0x6976` es parte del string "video_out0" en la estructura global.
Como entero: 26998, que es >> 688 → la validación `max_width >= 688` SIEMPRE PASA.

### Hallazgo 8: Sofia.c confirm struct layout (FUN_0026e800)

```c
// gadi_venc_set_channels_params en Sofia (R13210)
local_6c = *(ushort*)(param_2 + 4);   // main_width (0x02b0=688)
local_6a = *(ushort*)((int)param_2+6); // main_height (0x0240=576)
local_68 = param_2[2];                 // ch_mode_0
local_66 = *(ushort*)(param_2+16);     // sub1_w
local_64 = *(ushort*)(param_2+18);     // sub1_h
local_62 = local_6c;                   // main_width REPEAT
local_60 = local_6a;                   // main_height REPEAT
...
ioctl(*param_1, 0x40047687, &local_6c);
```

Primeros 4 bytes del struct = `[b0, 02, 40, 02]` = `{width=0x02b0=688, height=0x0240=576}`.
Confirmado vs trace de Sofia: arg en trace = `0x7ecf26a4` con primeros 4 bytes = `0x024002b0`.

### Tests realizados

Programa compilado: SDK init (sys→vi→vout→venc→map_bsb) + raw fd para SYS ioctls.

Resultados sin éxito:
1. `IOC_STOP_STREAM = 0x80047652` (INCORRECTO) → era GET_VERSION, encoder state no se resetea
2. `IOC_SET_ENCODE_STATE = 0x40047654` (CORRECTO) → ret=0 pero EINVAL sigue
3. `IOC_SET_LIMITS = 0x40047673` (antes de FORMAT) → ret=0 pero EINVAL sigue
4. Struct con interlace=0 y interlace=1 → ambas EINVAL

**Estado actual:** EINVAL se produce en `SYS_MEM_Exit+0x16c` porque `sys_state[0xdc]` es NULL o incorrecto después de que `gadi_vi_open()` inicia VI pero no llega a establecer este puntero en el mismo contexto de proceso.

### Archivos de investigacion de esta sesion

| Archivo | Descripción |
|---------|-------------|
| `/tmp/media.ko` | Copia del kernel module extraído del WiBox |
| `/tmp/media_disasm.txt` | Disassembly completo (55382 líneas) |
| `src/d1_capture.c` | Primer intento (retirado tras consolidar la prueba final) |
| `src/d1_capture_v2.c` | SDK correcto + raw ioctl (renombrado a `src/d1_video_capture.c`) |
| `src/d1_factory.c` | Experimento factory config (retirado) |

Los binarios y capturas intermedias se limpiaron del repo. La unica prueba de video
conservada en `src/` es `src/d1_video_capture.c`.

### Secuencia ioctl de Sofia (de sofia_ioctls_captured.log)

El orden que Sofia usa ANTES de llamar `0x40047687`:
```
0x80047652 ('v',0x52) READ  → GET_VERSION al inicio
0x40047654 ('v',0x54) WRITE → RESET encode_state to 0
0x80047670 ('v',0x70) READ  → GET_CHIP_INFO
0x40047316 ('s',0x16) WRITE → VI_SOURCE: set source mode
0x80047301 ('s',0x01) READ  → VI_SOURCE: get state
0x80047304 ('s',0x04) READ  → VI_SOURCE: get format
0x80046920 ('i',0x20) READ  → VI_ADAPTER: get adapter info
0x40046921 ('i',0x21) WRITE → VI_ADAPTER: configure adapter
0x40047303 ('s',0x03) WRITE → VI_SOURCE: set something
0x4004730b ('s',0x0b) WRITE → VI_SOURCE: set something
0x40047304 ('s',0x04) WRITE → VI_SOURCE: set FPS
0x80047305 ('s',0x05) READ  → VI_SOURCE: GET CAPABILITIES ← crucial
0x40047673 ('v',0x73) WRITE → SYS: set resource limits
0x40047687 ('v',0x87) WRITE → SYS: SET_SRCBUF_FORMAT ← nuestro target
0x40047683 ('v',0x83) WRITE → SYS: SET_SRCBUF_TYPE
```

La llamada `0x80047305` (VI_SOURCE READ nr=5) probablemente inicializa/establece
`sys_state[0xdc]` con el puntero al estado VI activo. Esto es lo que falta en
nuestra secuencia actual.

### Hipótesis de bloqueo actual

1. `gadi_vi_open()` llama algunas VI_SOURCE ioctls pero NO exactamente la misma
   secuencia que Sofia (falta la llamada a `0x80047305` que establece `sys_state[0xdc]`)
2. Cuando `sys_encode_guard_task+0x290` intenta `sys_state[0xdc].field_0x124`,
   el puntero es NULL o apunta a memoria no inicializada
3. El resultado: `SYS_MEM_Exit+0x16c` falla → EINVAL

### Plan de resolución (próximos pasos)

**Opción A (más fácil): Usar factory config de Sofia sin SET_SRCBUF_FORMAT**
- Sofia ya configuró los 3 encoders (type0=D1, type1/2=CIF)
- Después de matar Sofia: SDK init + map_bsb + START_STREAM + get_stream
- Filtrar `stream_id == 0` para capturar D1 (688x576)
- NO se necesita 0x40047687 para este approach

**Opción B: Completar la secuencia VI_SOURCE antes de llamar 0x40047687**
1. Después de `gadi_vi_open()` SDK, llamar manualmente:
   - `ioctl(fd, 0x40047316, buf)` → set VI source mode
   - `ioctl(fd, 0x80047305, &v)` → read VI capabilities (establece sys_state[0xdc])
   - `ioctl(fd, 0x40047673, limits)` → set resource limits
2. Luego call `0x40047687` con el struct de 40 bytes correcto

**Opción C: Modificar approach de capture**
- La sesión anterior capturó frames funcionales usando la factory config
- El stream D1 principal (stream_id=0) produce IDR de ~1800 bytes y P-slices de ~89 bytes
- Problema anterior: filtrábamos stream_id==2 (sub-stream CIF)
- Solución: filtrar stream_id==0 para obtener D1

**Siguiente test en ese momento:** ejecutar el enfoque factory después de Sofia warmup,
capturando `stream_id==0`. Ese enfoque quedo reemplazado por la prueba consolidada
`src/d1_video_capture.c`.

## Actualización 2026-06-29 — D1 confirmado sin Sofia

**Resultado:** `src/d1_video_capture.c` captura H.264 `stream_id == 0` y `ffprobe`
confirma resolución **688x576**:

```
codec_name=h264
profile=Main
width=688
height=576
coded_width=688
coded_height=576
```

Artefactos generados durante la validacion:

| Archivo | Descripción |
|---------|-------------|
| `d1_capture_final_sid0.h264` | Captura H.264 D1 válida, `stream_id==0`, 688x576; retirado del repo tras documentar el resultado |
| `d1_capture_final.log` | Log de la ejecución confirmada; retirado del repo tras documentar el resultado |

Puntos técnicos que desbloquearon la captura:

- `0x80047305` después de abrir `/dev/gk_video` sí popula el estado VI necesario.
- `0x4004767b` se llama con argumento entero `0`, no puntero.
- `0x80047674` debe leerse y escribirse de vuelta con `0x40047673`.
- `0x40047687` funciona con el layout exacto de Sofia:
  - main `688x576`, ch0 mode `1`
  - sub1 `352x300`, ch1 mode `1`
  - sub2 `352x288`, ch2 mode `1`
  - ch3 desactivado, `interlace_scan=1`
- `0x40046541` es el start real; `0xc004652a` es query.
- `gadi_vi_enable(vi_handle, 1)` antes de start es necesario.
- Los buffers internos de H.264 deben reservarse grandes (`512` bytes). Con `128`
  bytes se corrompe el stack porque el driver usa offsets internos hasta al menos `0xb2`.
- El stream devuelve SPS/PPS/IDR en buffers cuyo primer NAL es `0x07`; no se deben
  descartar esperando a un IDR posterior. Hay que escribir esos buffers al `.h264`.

Observaciones pendientes:

- Al configurar streams `0/1/2`, `START mask=0x2` devuelve `EINVAL`, pero `mask=0x1`
  y `mask=0x4` arrancan y no impiden capturar D1 por `stream_id==0`.
- `FORCE_IDR 0x4004653c` con mask `1` retorna OK.
- `get_stream(0xFF)` ve `stream_id 0` y `2`; la captura final filtra/escribe solo
  `stream_id==0`.

## Actualización 2026-06-29 — vídeo real con llamada activa y fps correcto

La captura D1 inicial era técnicamente válida pero podía verse azul/falsa porque no
se había activado la llamada en el MCU. El vídeo real aparece al enviar antes de
capturar:

```sh
printf "\xfb\x14\x01\x20" > /dev/ttySGK1
```

Y al terminar:

```sh
printf "\xfb\x14\x00\x1f" > /dev/ttySGK1
```

Esto confirma que Sofia no es necesaria por llamada si el hardware ya fue inicializado
una vez tras boot. El modelo actual es:

1. Sofia warmup una vez por boot (~30s) para inicializar VI/sensor/driver.
2. Matar Sofia.
3. Por cada llamada: start-call serial -> capturar/enviar media -> stop-call serial.

También se corrigió el framerate. La llamada `0x40046533` debe recibir el intervalo
que Sofia calcula como `60/60`:

```c
mask = 1 << stream_id;
numerator = 60;
denominator = 60;
```

El intento anterior usaba `1/25`, lo que reducía el ritmo real de captura a ~1 fps.
Al reempaquetar después a 25 fps el vídeo se veía acelerado y tardaba minutos en
recoger 300 frames.

Captura confirmada con llamada activa y fps corregido:

| Archivo | Descripción |
|---------|-------------|
| `d1_capture_call_12s_fpsfix_sid0.h264` | Raw H.264 D1 con llamada activa; retirado del repo tras documentar el resultado |
| `d1_capture_call_12s_fpsfix_25fps.mp4` | MP4 de inspección, 688x576; retirado del repo tras documentar el resultado |
| `d1_capture_call_12s_fpsfix.log` | Log de la prueba; retirado del repo tras documentar el resultado |

Resultado de la prueba:

```text
[CAPTURE] Done: 331 frames, 594206 bytes, 0 IDRs
width=688
height=576
avg_frame_rate=25/1
nb_read_frames=361
duration=14.44
```

La documentación consolidada del flujo está en `docs/d1_video_capture.md`.

## Actualización 2026-06-29 — limpieza de pruebas

Se limpio `src/` para dejar una sola prueba de video mantenible:

- `src/d1_video_capture.c`

Se retiraron los PoC y experimentos antiguos (`d1_capture.c`, `d1_factory.c`,
`sip_media_*`, `src/gadi_tests/*`) porque sus hallazgos ya estan incorporados en
la documentacion y en la prueba consolidada.

Siguiente fase: revisar el proyecto `wibox-audio`, reutilizar el trabajo de captura
de audio ya existente e integrar audio + video como media SIP/RTP por llamada.

### Notas importantes para la próxima sesión

- `sys_state = .data section base` de media.ko (mismo puntero usado en todas las funciones)
- El campo `sys_state[0xd0]` = encode_state (debe ser 0 para reconfigurar)
- El campo `sys_state[0xdc]` = puntero al VI state (debe ser no-null para que 0x40047687 funcione)
- `tv_probe` en media.ko = inicializa el TV decoder (VO side), NO el VI input
- La única instrucción que escribe `sys_state[0xf8]` está en `tv_probe` (son bytes del string "vide_out0")
- `SYS_Get_Vi_Capability()` retorna `sys_state[0xdc] + 12` = puntero a VI capability struct
