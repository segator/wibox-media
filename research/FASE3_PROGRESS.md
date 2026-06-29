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
