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
