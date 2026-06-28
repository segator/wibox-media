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
