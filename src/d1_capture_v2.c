/*
 * d1_capture_v2.c - GK7102S D1 H.264 capture - FIXED SDK init
 *
 * SDK init order: sys_init → vi_init+open → vout_init+open → venc_init+open+map_bsb
 * Then raw fd ioctls for VENC config (SET_SRCBUF_FORMAT, etc.)
 *
 * KEY DISCOVERY: ioctl 0x40047687 expects 40 bytes (NOT 4!)
 * Requires sys_state->max_width/height to be set by VI init (Sofia warmup)
 *
 * Prerequisites:
 *   1. Sofia warmup 25-30s (sets VI sys_state: max_width=688, max_height=576)
 *   2. Kill Sofia
 *   3. Run this program
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <stdint.h>
#include <errno.h>
#include "adi_types.h"
#include "adi_sys.h"
#include "adi_vi.h"
#include "adi_vout.h"
#include "adi_venc.h"

/* ================================================================
 * IOCTL definitions (type=0x76 SYS, type=0x65 VENC_ENCODE)
 * ================================================================ */
#define IOC_GET_CHIP_INFO      0x80047670
#define IOC_GET_VERSION        0x80047652  /* READ, returns version (NOT stop!) */
#define IOC_SET_ENCODE_STATE   0x40047654  /* WRITE, set encode_state (0=reset/stop) */
#define IOC_SET_LIMITS         0x40047673  /* WRITE, set resource limits - MUST before FORMAT */
#define IOC_SET_SRCBUF_FORMAT  0x40047687  /* WRITE, 40 bytes struct! */
#define IOC_SET_SRCBUF_TYPE    0x40047683  /* 4x int */
#define IOC_SET_FRAME_INTERVAL 0x40046533
#define IOC_SET_BITRATE        0x40046538
#define IOC_GETSET_H264_CFG    0xc0046540
#define IOC_SET_H264_CFG       0x4004653f
#define IOC_START_STREAM       0xc004652a
#define IOC_FORCE_IDR          0x4004653c
#define IOC_GET_STREAM         0x80046537

#define DEVICE "/dev/gk_video"

/* ================================================================
 * struct srcbuf_format_t - 40 bytes for ioctl 0x40047687
 * Discovered by reversing media.ko (sys_encode_guard_task+0x290)
 * ================================================================ */
struct srcbuf_format_t {
    uint16_t main_width;      /* 0:  must be multiple of 16 */
    uint16_t main_height;     /* 2:  must be even */
    uint16_t ch_mode_0;       /* 4:  channel group 0 mode */
    uint16_t sub1_w;          /* 6:  sub1 width  <= main_width */
    uint16_t sub1_h;          /* 8:  sub1 height <= main_height */
    uint16_t main_w_dup;      /* 10: main width repeat (validated <= main_width) */
    uint16_t main_h_dup;      /* 12: main height repeat */
    uint16_t ch_mode_1;       /* 14 */
    uint16_t sub2_w;          /* 16 */
    uint16_t sub2_h;          /* 18 */
    uint16_t main_w_dup2;     /* 20: validated <= main_width */
    uint16_t main_h_dup2;     /* 22 */
    uint16_t ch_mode_2;       /* 24 */
    uint16_t sub3_w;          /* 26 */
    uint16_t sub3_h;          /* 28 */
    uint16_t main_w_dup3;     /* 30: validated <= main_width */
    uint16_t main_h_dup3;     /* 32 */
    uint16_t ch_mode_3;       /* 34 */
    uint8_t  interlace_scan;  /* 36 */
    uint8_t  pad[3];          /* 37-39 */
} __attribute__((packed));   /* 40 bytes total */

/* ================================================================ */

static int gfd = -1;

static void try_stop_stream(void) {
    /* 0x40047654 = SET_ENCODE_STATE - value 0 = stop/reset */
    uint32_t v = 0;
    int r = ioctl(gfd, IOC_SET_ENCODE_STATE, &v);
    printf("[RESET_STATE] ioctl(0x40047654, 0) ret=%d errno=%d\n", r, errno);
    usleep(100000);
}

static void set_resource_limits(void) {
    /* 0x40047673 = SET_LIMITS - must be called before SET_SRCBUF_FORMAT
     * Pass zeroed buffer to let driver use defaults */
    uint8_t limits[128];
    memset(limits, 0, sizeof(limits));
    int r = ioctl(gfd, IOC_SET_LIMITS, limits);
    printf("[SET_LIMITS] ioctl(0x40047673) ret=%d errno=%d\n", r, errno);
}

static int set_srcbuf_format(int interlace)
{
    struct srcbuf_format_t fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.main_width    = 688;  fmt.main_height   = 576;
    fmt.ch_mode_0     = 0;
    fmt.sub1_w        = 688;  fmt.sub1_h        = 576;
    fmt.main_w_dup    = 688;  fmt.main_h_dup    = 576;
    fmt.ch_mode_1     = 0;
    fmt.sub2_w        = 688;  fmt.sub2_h        = 576;
    fmt.main_w_dup2   = 688;  fmt.main_h_dup2   = 576;
    fmt.ch_mode_2     = 0;
    fmt.sub3_w        = 688;  fmt.sub3_h        = 576;
    fmt.main_w_dup3   = 688;  fmt.main_h_dup3   = 576;
    fmt.ch_mode_3     = 0;
    fmt.interlace_scan = (uint8_t)interlace;

    printf("[SET_SRCBUF_FORMAT] interlace=%d struct_size=%zu\n",
           interlace, sizeof(fmt));

    /* Print first 8 bytes for debug */
    uint16_t *p = (uint16_t*)&fmt;
    printf("  bytes: %04x %04x %04x %04x\n", p[0], p[1], p[2], p[3]);

    int ret = ioctl(gfd, IOC_SET_SRCBUF_FORMAT, &fmt);
    printf("[SET_SRCBUF_FORMAT] → ret=%d errno=%d (%s)\n",
           ret, errno, strerror(errno));
    return ret;
}

int main(int argc, char **argv)
{
    const char *outfile = argc > 1 ? argv[1] : "/tmp/d1_out.h264";
    GADI_ERR err;
    int ret;

    printf("=== D1 Capture v2 ===\n");
    printf("Output: %s\n\n", outfile);

    /* ── SDK Init Chain ── */
    printf("[SDK] gadi_sys_init...\n");
    err = gadi_sys_init();
    printf("[SDK] sys_init: %s\n\n", err ? "FAIL" : "OK");
    if (err) return 1;

    printf("[SDK] vi_init + vi_open...\n");
    err = gadi_vi_init();
    printf("[SDK] vi_init: %s\n", err ? "FAIL" : "OK");
    GADI_SYS_HandleT vi_handle = gadi_vi_open(&err);
    printf("[SDK] vi_open: %s handle=%p\n\n", err ? "FAIL" : "OK", vi_handle);
    if (err) return 1;

    printf("[SDK] vout_init + vout_open...\n");
    err = gadi_vout_init();
    GADI_SYS_HandleT vo_handle = gadi_vout_open(&err);
    printf("[SDK] vout: %s handle=%p\n\n", err ? "FAIL" : "OK", vo_handle);
    if (err) return 1;

    printf("[SDK] venc_init + venc_open...\n");
    err = gadi_venc_init();
    printf("[SDK] venc_init: %s\n", err ? "FAIL" : "OK");

    GADI_VENC_OpenParamsT op;
    memset(&op, 0, sizeof(op));
    op.viHandle   = vi_handle;
    op.voutHandle = vo_handle;

    GADI_SYS_HandleT venc_handle = gadi_venc_open(&op, &err);
    printf("[SDK] venc_open: %s handle=%p err=%d\n\n",
           err ? "FAIL" : "OK", venc_handle, err);
    if (err) return 1;

    /* ── Get BSB (bitstream buffer) via SDK ── */
    printf("[SDK] map_bsb...\n");
    err = gadi_venc_map_bsb(venc_handle);
    printf("[SDK] map_bsb: %s\n\n", err ? "FAIL" : "OK");
    if (err) {
        fprintf(stderr, "FATAL: map_bsb failed err=%d\n", err);
        return 1;
    }

    /* ── Switch to raw fd for VENC config ── */
    printf("[RAW] Opening /dev/gk_video...\n");
    gfd = open(DEVICE, O_RDWR);
    if (gfd < 0) { perror("open"); return 1; }
    printf("[RAW] fd=%d\n\n", gfd);

    /* ── Chip info ── */
    {
        uint32_t ci[2] = {0};
        ioctl(gfd, IOC_GET_CHIP_INFO, ci);
        printf("[INFO] chip_type=%d\n\n", ci[0]);
    }

    /* ── Stop/Reset encoder state ── */
    printf("[RESET] Resetting encode state...\n");
    try_stop_stream();

    /* ── Set resource limits (required before SET_SRCBUF_FORMAT) ── */
    printf("[LIMITS] Setting resource limits...\n");
    set_resource_limits();

    /* ── Key ioctl: SET_SRCBUF_FORMAT ── */
    printf("[CONFIG] SET_SRCBUF_FORMAT (40-byte struct)...\n");
    ret = set_srcbuf_format(0);  /* try progressive first */
    if (ret < 0) {
        printf("[CONFIG] Progressive failed, trying interlaced...\n");
        ret = set_srcbuf_format(1);
        if (ret < 0) {
            fprintf(stderr, "FATAL: SET_SRCBUF_FORMAT failed.\n");
            fprintf(stderr, "Did Sofia run for 25+s? sys_state needs VI init.\n");
            goto out;
        }
    }

    /* ── SET_SRCBUF_TYPE: channel 0 = H264 ── */
    printf("[CONFIG] SET_SRCBUF_TYPE...\n");
    {
        uint32_t types[4] = {1, 0, 0, 0};
        ret = ioctl(gfd, IOC_SET_SRCBUF_TYPE, types);
        printf("[CONFIG] SET_SRCBUF_TYPE ret=%d\n\n", ret);
    }

    /* ── H264 config ── */
    printf("[CONFIG] SET_H264_CFG...\n");
    {
        uint32_t cfg[9] = {0};
        cfg[0] = 0;   /* channel */
        cfg[1] = 77;  /* profile: main */
        cfg[2] = 40;  /* level: 4.0 */
        cfg[3] = 15;  /* gop_n */
        cfg[4] = 15;  /* gop_idr */
        ret = ioctl(gfd, IOC_SET_H264_CFG, cfg);
        printf("[CONFIG] SET_H264_CFG ret=%d\n\n", ret);
    }

    /* ── START_STREAM ── */
    printf("[START] Starting stream...\n");
    {
        uint32_t start[2] = {0, 0};
        ret = ioctl(gfd, IOC_START_STREAM, start);
        printf("[START] ret=%d\n\n", ret);
        if (ret < 0) {
            fprintf(stderr, "START_STREAM failed, trying with factory state...\n");
        }
    }
    usleep(300000);

    /* ── FORCE IDR ── */
    {
        uint32_t ch = 0;
        ret = ioctl(gfd, IOC_FORCE_IDR, &ch);
        printf("[IDR] force_idr ret=%d\n\n", ret);
    }
    usleep(100000);

    /* ── CAPTURE LOOP ── */
    printf("[CAPTURE] Starting → %s\n", outfile);
    FILE *fout = fopen(outfile, "wb");
    if (!fout) { perror("fopen"); goto stop; }

    uint8_t *sps_buf = NULL, *pps_buf = NULL;
    size_t   sps_len = 0,     pps_len = 0;
    int sps_fresh = 0, pps_fresh = 0;
    long long total_bytes = 0;
    int frames = 0, idrs = 0, errors = 0;

    for (int i = 0; i < 600 && errors < 30; i++) {
        GADI_VENC_StreamT st;
        memset(&st, 0, sizeof(st));

        ret = gadi_venc_get_stream(venc_handle, 0xFF, &st);
        if (ret < 0) { errors++; usleep(10000); continue; }
        errors = 0;

        if (!st.size || !st.addr) { usleep(5000); continue; }

        /* Main stream only: stream_id == 0 */
        if (st.stream_id != 0) { usleep(1000); continue; }

        uint8_t *data = st.addr;
        uint32_t  sz  = st.size;

        /* Parse NAL type */
        int nal = -1;
        if (sz >= 5 && data[0]==0 && data[1]==0 && data[2]==0 && data[3]==1)
            nal = data[4] & 0x1f;

        if (nal == 7) { /* SPS */
            free(sps_buf);
            sps_buf = malloc(sz); memcpy(sps_buf, data, sz); sps_len = sz;
            sps_fresh = 1; pps_fresh = 0; continue;
        }
        if (nal == 8) { /* PPS */
            free(pps_buf);
            pps_buf = malloc(sz); memcpy(pps_buf, data, sz); pps_len = sz;
            pps_fresh = 1; continue;
        }
        if (nal == 5) { /* IDR - write SPS+PPS first */
            if (sps_buf && sps_fresh) {
                fwrite(sps_buf, 1, sps_len, fout);
                total_bytes += sps_len;
                sps_fresh = 0;
            }
            if (pps_buf && pps_fresh) {
                fwrite(pps_buf, 1, pps_len, fout);
                total_bytes += pps_len;
                pps_fresh = 0;
            }
            idrs++;
        }
        if (nal == 5 || nal == 1) { /* IDR or P-slice */
            fwrite(data, 1, sz, fout);
            total_bytes += sz; frames++;
            if (frames % 25 == 0)
                printf("  [%d frames] %lld bytes, %d IDRs (sid=%d nal=0x%02x sz=%u)\n",
                       frames, total_bytes, idrs, st.stream_id, nal, sz);
        }
        usleep(3000);
    }

    printf("\n[CAPTURE] Done: %d frames, %lld bytes, %d IDRs\n",
           frames, total_bytes, idrs);
    fclose(fout);
    free(sps_buf); free(pps_buf);

    if (frames == 0) {
        fprintf(stderr, "\nNO FRAMES CAPTURED!\n");
        fprintf(stderr, "Possible causes:\n");
        fprintf(stderr, "  1. Stream ID not 0 (try 0xFF or 2)\n");
        fprintf(stderr, "  2. BSB buffer offset issue\n");
        fprintf(stderr, "  3. VENC not producing output\n");
    }

stop:
    try_stop_stream();
out:
    if (gfd >= 0) close(gfd);
    return 0;
}
