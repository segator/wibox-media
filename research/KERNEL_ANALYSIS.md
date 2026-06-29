# media.ko Reverse Engineering — Hallazgos Clave

## Archivo
- Fuente: `/ko/media.ko` en WiBox (192.168.0.196)
- Tamaño: 303KB, **NO stripped** (tiene símbolos)
- Versión: svn r13210 (Feb 2018), ARM32

## Dispatch de ioctls (media_drv_ioctl @ 0x14d6c)

El byte de tipo (bits[15:8]) del número ioctl determina el handler:

| Tipo | Hex | Handler | Función |
|------|-----|---------|---------|
| 'i' | 0x69 | VI_ADAPTER_Ioctl @ 0xca18 | Configurar adaptador VI |
| 's' | 0x73 | VI_SOURCE_Ioctl @ 0xcb7c | Control fuente VI |
| '$' | 0x24 | VO_SOURCE_Ioctl @ 0xa204 | Fuente de video output |
| '#' | 0x23 | VO_SINK_Ioctl @ 0xa9c0 | Sink de video output |
| 'v' | 0x76 | SYS_Ioctl @ 0x1a1e8 | Sistema (VENC, recursos) |
| 'e' | 0x65 | VENC_ENCODE_Ioctl @ 0x24718 | Codificador H.264/MJPEG |
| 'd' | 0x64 | VDEC_Ioctl @ 0x126e4 | Decodificador |
| 'p' | 0x70 | ISP_Ioctl @ 0xef54 | ISP |
| 'o' | 0x6f | REGION_OVERLAY_Ioctl @ 0x13fd8 | Overlay |
| 'P' | 0x50 | REGION_PM_Ioctl @ 0x13628 | Power management |

## SYS_Ioctl: ioctls conocidos

| Ioctl | Decode | Handler offset | Descripción |
|-------|--------|----------------|-------------|
| 0x80047652 | _IOR('v',0x52,4) | 0x1b9f0 | GET_VERSION (lectura) |
| 0x40047654 | _IOW('v',0x54,4) | 0x1a66c | SET_ENCODE_STATE (reset=0) |
| 0x80047670 | _IOR('v',0x70,4) | ? | GET_CHIP_INFO → chip_type |
| 0x40047673 | _IOW('v',0x73,4) | ? | SET_RESOURCE_LIMITS |
| 0x40047683 | _IOW('v',0x83,4) | ? | SET_SRCBUF_TYPE → int[4] channel types |
| **0x40047687** | **_IOW('v',0x87,4)** | **0x1b9f0→0x177ec** | **SET_SRCBUF_FORMAT (40 bytes!)** |

## 0x40047687 (SET_SRCBUF_FORMAT) — ANÁLISIS COMPLETO

### El handler real
```
SYS_Ioctl dispatch: 0x40047689 - 2 = 0x40047687 → b 0x1b9f0
0x1b9f0: mov r0, r2 (r2=user_ptr)
         bl 0x177ec (sys_encode_guard_task+0x290)
```

### Tamaño REAL del struct: 40 bytes (no 4!)
`_IOC_SIZE(0x40047687) = 4` es ignorado. El handler llama:
`__copy_from_user(fp-104, user_ptr, 40)`

### Validaciones (todas deben pasar):
1. `struct[0] & 0xf == 0` → main_width múltiplo de 16
2. `struct[2] & 1 == 0` → main_height par
3. `.data[0xf8] >= struct[0]` → siempre pasa (.data[0xf8]=0x6976=26998 >> 688)
4. `.data[0xfa] >= struct[2]` → siempre pasa (.data[0xfa]=0x6564=25956 >> 576)
5. `SYS_MEM_Exit+0x16c(0, 688, 576, sys_state[0xdc][0x124], sys_state[0xdc][0x126])`
   → **FALLO** si sys_state[0xdc] == NULL (VI no inicializado correctamente)
6. `struct[10] <= struct[0]`, `struct[12] <= struct[2]` (x3 sub-canales)
7. Si sys_state[0x14]==1: struct[4] <= 2
8. Si ya configurado (mismo w,h,mode): return 0 (no-op)
9. `sys_state[0xd0] == 0` → encoder debe estar parado

### Layout del struct (40 bytes):
```c
struct srcbuf_format_t {
    uint16_t main_width;      // 0:  múltiplo de 16 (688 para PAL D1)
    uint16_t main_height;     // 2:  par (576 para PAL D1)
    uint16_t ch_mode_0;       // 4:  0..2
    uint16_t sub1_w;          // 6:  <= main_width
    uint16_t sub1_h;          // 8:  <= main_height
    uint16_t main_w_dup;      // 10: = main_width (validado)
    uint16_t main_h_dup;      // 12: = main_height
    uint16_t ch_mode_1;       // 14:
    uint16_t sub2_w;          // 16:
    uint16_t sub2_h;          // 18:
    uint16_t main_w_dup2;     // 20:
    uint16_t main_h_dup2;     // 22:
    uint16_t ch_mode_2;       // 24:
    uint16_t sub3_w;          // 26:
    uint16_t sub3_h;          // 28:
    uint16_t main_w_dup3;     // 30:
    uint16_t main_h_dup3;     // 32:
    uint16_t ch_mode_3;       // 34:
    uint8_t  interlace_scan;  // 36: 0=prog, 1=entrelazado
    uint8_t  pad[3];          // 37-39:
} __attribute__((packed));    // total: 40 bytes
```

### Condición para que funcione:
**sys_state[0xdc] debe ser no-NULL** → apuntando a struct VI activo.
Esto se consigue llamando la secuencia VI completa (incluyendo `0x80047305`).

## Global sys_state (.data + 0)

La sección `.data` de media.ko comienza con el estado global del sistema.
Ambas referencias en el código apuntan a `.data+0`:
- `text[0x17e6c]`: usado por `sys_encode_guard_task+0x290`
- `text[0xc770]`: usado por `VI_CORE_Add_Adapter`

### Campos importantes:
| Offset | Valor inicial | Descripción |
|--------|-------------|-------------|
| 0x00 | 0x1e0b40f0 | puntero (relocation) |
| 0x0c | 0x00 | sys_state[0x0c] = current_width (set por ioctl) |
| 0x0e | 0x00 | sys_state[0x0e] = current_height |
| 0x18 | 0x00 | sys_state[0x18] = current_mode |
| 0xcc | 0x32 | ? |
| 0xd0 | 0x32 | encode_state (0=stopped, 2=streaming) |
| 0xdc | 0x00 | puntero VI state (NULL inicial, set por VI_Init) |
| 0xf0 | 0x01 | |
| 0xf8 | 0x6976 | bytes del string "video_out0" (NO es max_width!) |
| 0xfa | 0x6564 | continúa el string "video_out0" |

## VENC_ENCODE_Ioctl: ioctls de streaming

| Ioctl | Decode | Descripción |
|-------|--------|-------------|
| 0xc004652a | _IOWR('e',0x2a,4) | START_STREAM → iniciar encoder |
| 0x4004653c | _IOW('e',0x3c,4) | FORCE_IDR → forzar keyframe |
| 0x4004653f | _IOW('e',0x3f,4) | SET_H264_CFG |
| 0x40046533 | _IOW('e',0x33,4) | SET_FRAME_INTERVAL |
| 0x40046538 | _IOW('e',0x38,4) | SET_BITRATE |
| 0xc0046540 | _IOWR('e',0x40,4) | GETSET_H264_CFG |
| 0x80046537 | _IOR('e',0x37,4) | GET_STREAM |

## Función VI_SOURCE_Ioctl (@ 0xcb7c)

Maneja ioctls `type = 's' (0x73)`:

| Ioctl | Handler | Descripción |
|-------|---------|-------------|
| 0x40047316 | 0xd344 | VI_SOURCE WRITE nr=0x16: set source mode |
| 0x80047305 | 0xce9c | VI_SOURCE READ nr=0x05: get VI capabilities → usa VI_CORE_Source_Cmd con cmd=0x2105 |
| 0x80047307 | ? | VI_SOURCE READ nr=0x07 |
| 0x80047309 | ? | VI_SOURCE READ nr=0x09 |
| ... | | |

El handler de `0x80047305` llama `VI_CORE_Source_Cmd` con comando 0x2105 y retorna 24 bytes.
Esta llamada probablemente inicializa `sys_state[0xdc]`.

## Funciones clave

| Función | Dirección | Descripción |
|---------|-----------|-------------|
| media_drv_ioctl | 0x14d6c | Dispatcher principal ioctl |
| SYS_Ioctl | 0x1a1e8 | Handler ioctls type=0x76 |
| VENC_ENCODE_Ioctl | 0x24718 | Handler ioctls type=0x65 |
| VI_SOURCE_Ioctl | 0xcb7c | Handler ioctls type=0x73 |
| VI_ADAPTER_Ioctl | 0xca18 | Handler ioctls type=0x69 |
| sys_encode_guard_task | 0x1755c | Loop principal del encoder |
| sys_encode_guard_task+0x290 | 0x177ec | Handler de SET_SRCBUF_FORMAT |
| SYS_Get_Vi_Capability | 0x18cc0 | Retorna sys_state[0xdc]+12 |
| VI_Init | 0xbaa0 | Inicializa subsistema VI |
| VI_CORE_Add_Adapter | 0xc640 | Registra adapter VI en sys_state |
| VENC_ENCODE_All_Stop | 0x244bc | Para todos los encoders |
| tv_probe | 0x003c | Inicializa TV decoder (VO side) |
| SYS_MEM_Exit+0x16c | 0x15b40 | Valida dimensiones vs tabla HW |
