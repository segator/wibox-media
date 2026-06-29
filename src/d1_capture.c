/*
 * d1_capture.c - GK7102S D1 H.264 capture
 *
 * DISCOVERED via media.ko reverse engineering:
 * - ioctl 0x40047687 handler copies 40 bytes from user (NOT 4!)
 * - Struct must have main_width*main_height validated against
 *   sys_state->max_width (offset 0xf8) and max_height (0xfa)
 * - These are set by VI initialization (Sofia warmup sets them)
 * - Encoding must be STOPPED (sys_state[0xd0]==0) before reconfiguring
 *
 * Prerequisites:
 *   1. Run Sofia for 25-30s (sets sys_state max_width=688, max_height=576)
 *   2. Kill Sofia
 *   3. Run this program immediately
 *
 * Usage: ./d1_capture [output.h264]
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

/* ================================================================
 * IOCTL definitions (from media.ko reverse engineering)
 * ================================================================ */

/* type=0x76 (SYS): */
#define IOC_GET_CHIP_INFO      0x80047670  /* READ, 4 bytes → int chip_type */
#define IOC_GET_VER            0x80047652  /* READ, stop stream? actually STOP */
#define IOC_SYS_SET_LIMITS     0x40047673  /* WRITE, resource limits struct */
#define IOC_SET_SRCBUF_FORMAT  0x40047687  /* WRITE, 40 bytes struct - OUR TARGET */
#define IOC_SET_SRCBUF_TYPE    0x40047683  /* WRITE, 4x int channel types */

/* type=0x65 (VENC_ENCODE): */
#define IOC_SET_FRAME_INTERVAL 0x40046533  /* WRITE, frame interval struct */
#define IOC_SET_BITRATE        0x40046538  /* WRITE, bitrate struct */
#define IOC_GETSET_H264_CFG    0xc0046540  /* RDWR, h264 config */
#define IOC_SET_H264_CFG       0x4004653f  /* WRITE, h264 config */
#define IOC_START_STREAM       0xc004652a  /* RDWR, start encoder */
#define IOC_STOP_STREAM        0x80047652  /* READ, stop encoder */
#define IOC_FORCE_IDR          0x4004653c  /* WRITE, force IDR frame */
#define IOC_GET_STREAM         0x80046537  /* READ, get encoded stream */

#define DEVICE "/dev/gk_video"
#define BSB_SIZE (4 * 1024 * 1024)  /* 4MB bitstream buffer */

/* ================================================================
 * Struct for ioctl 0x40047687 (SET_SRCBUF_FORMAT)
 * Discovered by reversing media.ko sys_encode_guard_task+0x290
 *
 * The driver copies 40 bytes from userspace.
 * Layout derived from Sofia's FUN_0026e800 (gadi_venc_set_channels_params):
 *
 *   [0 ]  main_width      - total VI width (must be multiple of 16)
 *   [2 ]  main_height     - total VI height (must be even)
 *   [4 ]  ch_mode_0       - channel group 0 mode (must be 0..2 if sys_state[20]==1)
 *   [6 ]  sub1_width      - sub-channel 1 encode width
 *   [8 ]  sub1_height     - sub-channel 1 encode height
 *   [10]  main_w_dup      - must be <= main_width (validated!)
 *   [12]  main_h_dup      - must be <= main_height (validated!)
 *   [14]  ch_mode_1
 *   [16]  sub2_width      - sub-channel 2 encode width
 *   [18]  sub2_height
 *   [20]  main_w_dup2     - validated <= main_width
 *   [22]  main_h_dup2
 *   [24]  ch_mode_2
 *   [26]  sub3_width
 *   [28]  sub3_height
 *   [30]  main_w_dup3     - validated <= main_width
 *   [32]  main_h_dup3
 *   [34]  ch_mode_3
 *   [36]  interlace_scan  - 0=progressive, 1=interlaced
 *   [37..39] padding
 *
 * ================================================================ */
struct srcbuf_format_t {
    uint16_t main_width;      /* 0: must be multiple of 16 */
    uint16_t main_height;     /* 2: must be even */
    uint16_t ch_mode_0;       /* 4: 0..2 */
    uint16_t sub1_w;          /* 6: <= main_width */
    uint16_t sub1_h;          /* 8: <= main_height */
    uint16_t main_w_dup;      /* 10: must be <= main_width */
    uint16_t main_h_dup;      /* 12: must be <= main_height */
    uint16_t ch_mode_1;       /* 14 */
    uint16_t sub2_w;          /* 16: <= main_width */
    uint16_t sub2_h;          /* 18 */
    uint16_t main_w_dup2;     /* 20: <= main_width */
    uint16_t main_h_dup2;     /* 22 */
    uint16_t ch_mode_2;       /* 24 */
    uint16_t sub3_w;          /* 26 */
    uint16_t sub3_h;          /* 28 */
    uint16_t main_w_dup3;     /* 30: <= main_width */
    uint16_t main_h_dup3;     /* 32 */
    uint16_t ch_mode_3;       /* 34 */
    uint8_t  interlace_scan;  /* 36 */
    uint8_t  pad[3];          /* 37-39 */
} __attribute__((packed));   /* 40 bytes */

/* GET_STREAM result struct (from earlier working captures) */
struct stream_info_t {
    uint32_t stream_id;
    uint32_t frame_type;   /* 0=P, 1=IDR, 2=SPS, 3=PPS */
    uint32_t pts_hi;
    uint32_t pts_lo;
    uint32_t addr;
    uint32_t size;
    uint32_t seq;
    uint32_t flags;
    uint32_t reserved[8];
};

/* ================================================================
 * SDK functions (from libadi.a R10973)
 * Only GET/READ functions work reliably; SET functions crash with SEGV
 * ================================================================ */
extern int gadi_venc_init(void *arg);
extern int gadi_venc_open(int channel, int *fd);
extern int gadi_venc_map_bsb(int fd, void **bsb_ptr, size_t *bsb_size);
extern int gadi_venc_get_stream(int fd, int stream_id, struct stream_info_t *info);

static int gfd = -1;         /* raw /dev/gk_video fd */
static void *bsb_ptr = NULL;
static size_t bsb_size = 0;
static int venc_fd = -1;

/* ----------------------------------------------------------------- */
static int raw_stop_stream(void)
{
    uint32_t val = 0;
    int ret = ioctl(gfd, IOC_STOP_STREAM, &val);
    printf("[STOP_STREAM] ret=%d (errno=%d)\n", ret, errno);
    return ret;
}

static int raw_set_srcbuf_format(void)
{
    struct srcbuf_format_t fmt;
    memset(&fmt, 0, sizeof(fmt));

    /* Main channel: 688x576 PAL D1 */
    fmt.main_width   = 688;   /* 0x02b0 - multiple of 16 */
    fmt.main_height  = 576;   /* 0x0240 - even */
    fmt.ch_mode_0    = 0;     /* progressive/disabled mode */

    /* Sub-channel 1: same as main (single-stream config) */
    fmt.sub1_w       = 688;
    fmt.sub1_h       = 576;
    fmt.main_w_dup   = 688;   /* must be <= main_width */
    fmt.main_h_dup   = 576;
    fmt.ch_mode_1    = 0;

    /* Sub-channel 2 */
    fmt.sub2_w       = 688;
    fmt.sub2_h       = 576;
    fmt.main_w_dup2  = 688;
    fmt.main_h_dup2  = 576;
    fmt.ch_mode_2    = 0;

    /* Sub-channel 3 */
    fmt.sub3_w       = 688;
    fmt.sub3_h       = 576;
    fmt.main_w_dup3  = 688;
    fmt.main_h_dup3  = 576;
    fmt.ch_mode_3    = 0;

    fmt.interlace_scan = 0;   /* 0=progressive, try 1 if this fails */

    printf("[SET_SRCBUF_FORMAT] struct size=%zu, main=%dx%d\n",
           sizeof(fmt), fmt.main_width, fmt.main_height);

    int ret = ioctl(gfd, IOC_SET_SRCBUF_FORMAT, &fmt);
    printf("[SET_SRCBUF_FORMAT] ret=%d (errno=%d: %s)\n",
           ret, errno, strerror(errno));
    return ret;
}

static int raw_set_srcbuf_type(void)
{
    uint32_t types[4] = {1, 0, 0, 0};  /* channel 0 = H264 (type=1), others off */
    int ret = ioctl(gfd, IOC_SET_SRCBUF_TYPE, types);
    printf("[SET_SRCBUF_TYPE] ret=%d (errno=%d)\n", ret, errno);
    return ret;
}

static int raw_set_h264_config(int channel)
{
    /* H264 config struct - works with ioctl 0x4004653f */
    struct {
        uint32_t channel;
        uint32_t profile;   /* 66=baseline, 77=main, 100=high */
        uint32_t level;     /* 30=3.0, 40=4.0, 41=4.1 */
        uint32_t gop_n;
        uint32_t gop_idr;
        uint32_t reserved[4];
    } h264cfg = {0};

    h264cfg.channel = channel;
    h264cfg.profile = 77;   /* main */
    h264cfg.level   = 40;
    h264cfg.gop_n   = 15;
    h264cfg.gop_idr = 15;

    int ret = ioctl(gfd, IOC_SET_H264_CFG, &h264cfg);
    printf("[SET_H264_CFG ch%d] ret=%d (errno=%d)\n", channel, ret, errno);
    return ret;
}

static int raw_start_stream(int channel)
{
    struct {
        uint32_t channel;
        uint32_t reserved;
    } arg = {channel, 0};

    int ret = ioctl(gfd, IOC_START_STREAM, &arg);
    printf("[START_STREAM ch%d] ret=%d (errno=%d)\n", channel, ret, errno);
    return ret;
}

static int raw_force_idr(int channel)
{
    uint32_t ch = channel;
    int ret = ioctl(gfd, IOC_FORCE_IDR, &ch);
    printf("[FORCE_IDR ch%d] ret=%d (errno=%d)\n", channel, ret, errno);
    return ret;
}

/* ================================================================
 * Main capture loop
 * ================================================================ */
int main(int argc, char **argv)
{
    const char *outfile = argc > 1 ? argv[1] : "/tmp/d1_output.h264";
    int ret;

    printf("=== GK7102S D1 Capture (688x576 H.264) ===\n");
    printf("Output: %s\n", outfile);

    /* Step 1: Open raw device fd */
    gfd = open(DEVICE, O_RDWR);
    if (gfd < 0) {
        perror("open /dev/gk_video");
        return 1;
    }
    printf("[OPEN] /dev/gk_video fd=%d\n", gfd);

    /* Step 2: Get chip info to verify communication */
    {
        uint32_t chip_info[2] = {0};
        ret = ioctl(gfd, IOC_GET_CHIP_INFO, chip_info);
        printf("[CHIP_INFO] ret=%d chip_type=%d\n", ret, chip_info[0]);
    }

    /* Step 3: Stop any existing stream (ensures sys_state[0xd0] = 0) */
    raw_stop_stream();
    usleep(100000);  /* 100ms */

    /* Step 4: Init SDK (only for map_bsb) */
    printf("[SDK] gadi_venc_init...\n");
    ret = gadi_venc_init(NULL);
    printf("[SDK] gadi_venc_init ret=%d\n", ret);

    ret = gadi_venc_open(0, &venc_fd);
    printf("[SDK] gadi_venc_open ret=%d venc_fd=%d\n", ret, venc_fd);

    ret = gadi_venc_map_bsb(venc_fd, &bsb_ptr, &bsb_size);
    printf("[SDK] gadi_venc_map_bsb ret=%d bsb_ptr=%p size=%zu\n",
           ret, bsb_ptr, bsb_size);

    if (ret < 0 || bsb_ptr == NULL) {
        fprintf(stderr, "FATAL: Cannot map BSB. Aborting.\n");
        goto out;
    }

    /* Step 5: SET_SRCBUF_FORMAT - THE KEY IOCTL (40-byte struct!) */
    ret = raw_set_srcbuf_format();
    if (ret < 0) {
        fprintf(stderr, "FATAL: SET_SRCBUF_FORMAT failed (errno=%d: %s)\n",
                errno, strerror(errno));
        fprintf(stderr, "Hint: Ensure Sofia ran for 25+s before killing it.\n");
        fprintf(stderr, "Hint: Try interlace_scan=1 for CVBS input.\n");
        /* Try with interlace_scan=1 */
        fprintf(stderr, "Retrying with interlace_scan=1...\n");
        struct srcbuf_format_t fmt2;
        memset(&fmt2, 0, sizeof(fmt2));
        fmt2.main_width = fmt2.main_w_dup = fmt2.main_w_dup2 = fmt2.main_w_dup3 = 688;
        fmt2.main_height = fmt2.main_h_dup = fmt2.main_h_dup2 = fmt2.main_h_dup3 = 576;
        fmt2.sub1_w = fmt2.sub2_w = fmt2.sub3_w = 688;
        fmt2.sub1_h = fmt2.sub2_h = fmt2.sub3_h = 576;
        fmt2.interlace_scan = 1;
        ret = ioctl(gfd, IOC_SET_SRCBUF_FORMAT, &fmt2);
        printf("[SET_SRCBUF_FORMAT interlaced] ret=%d (errno=%d: %s)\n",
               ret, errno, strerror(errno));
        if (ret < 0) goto out;
    }

    /* Step 6: SET_SRCBUF_TYPE - channel 0 = H264 */
    ret = raw_set_srcbuf_type();
    if (ret < 0) {
        fprintf(stderr, "SET_SRCBUF_TYPE failed\n");
        goto out;
    }

    /* Step 7: Configure H264 encoder */
    raw_set_h264_config(0);

    /* Step 8: START_STREAM */
    ret = raw_start_stream(0);
    if (ret < 0) {
        fprintf(stderr, "START_STREAM failed: errno=%d\n", errno);
        goto out;
    }
    usleep(200000);  /* 200ms warmup */

    /* Step 9: FORCE_IDR to get clean keyframe */
    raw_force_idr(0);
    usleep(100000);

    /* Step 10: Capture frames */
    printf("[CAPTURE] Starting capture to %s...\n", outfile);
    FILE *fout = fopen(outfile, "wb");
    if (!fout) {
        perror("fopen output");
        goto stop;
    }

    int sps_written = 0, pps_written = 0;
    int64_t total_bytes = 0;
    int frames = 0;
    int idr_count = 0;
    int errors = 0;

    /* Buffer for SPS+PPS to prepend before first IDR */
    uint8_t *sps_data = NULL; size_t sps_len = 0;
    uint8_t *pps_data = NULL; size_t pps_len = 0;

    /* Capture for ~10 seconds at 25fps = 250 frames */
    for (int i = 0; i < 500 && errors < 20; i++) {
        struct stream_info_t st;
        memset(&st, 0, sizeof(st));

        ret = gadi_venc_get_stream(venc_fd, 0xFF, &st);
        if (ret < 0) {
            errors++;
            usleep(10000);
            continue;
        }
        errors = 0;  /* reset on success */

        if (st.size == 0 || st.addr == 0) {
            usleep(5000);
            continue;
        }

        /* Only capture stream 0 (main D1 channel) */
        if (st.stream_id != 0) {
            usleep(1000);
            continue;
        }

        uint8_t *data = (uint8_t*)bsb_ptr + (st.addr % bsb_size);
        uint32_t sz = st.size;

        /* Detect NAL type */
        int nal_type = -1;
        if (sz >= 5 && data[0]==0 && data[1]==0 && data[2]==0 && data[3]==1)
            nal_type = data[4] & 0x1f;

        /* SPS (7=0x47 in Annex B) */
        if (nal_type == 7) {
            sps_data = realloc(sps_data, sz);
            memcpy(sps_data, data, sz);
            sps_len = sz;
            sps_written = 0;  /* write again before next IDR */
            pps_written = 0;
            continue;
        }

        /* PPS (8=0x48) */
        if (nal_type == 8) {
            pps_data = realloc(pps_data, sz);
            memcpy(pps_data, data, sz);
            pps_len = sz;
            pps_written = 0;
            continue;
        }

        /* IDR (5=0x45): prepend SPS+PPS */
        if (nal_type == 5) {
            if (!sps_written && sps_data) {
                fwrite(sps_data, 1, sps_len, fout);
                total_bytes += sps_len;
                sps_written = 1;
            }
            if (!pps_written && pps_data) {
                fwrite(pps_data, 1, pps_len, fout);
                total_bytes += pps_len;
                pps_written = 1;
            }
            idr_count++;
        }

        /* Write P-slice or IDR */
        if (nal_type == 5 || nal_type == 1) {
            fwrite(data, 1, sz, fout);
            total_bytes += sz;
            frames++;
            if (frames % 25 == 0)
                printf("[CAPTURE] %d frames, %lld bytes, %d IDRs "
                       "(stream_id=%d, nal=0x%02x, sz=%u)\n",
                       frames, (long long)total_bytes, idr_count,
                       st.stream_id, nal_type, sz);
        }

        usleep(5000);  /* ~200fps max polling rate */
    }

    printf("[CAPTURE] Done: %d frames, %lld bytes, %d IDRs\n",
           frames, (long long)total_bytes, idr_count);
    fclose(fout);
    free(sps_data);
    free(pps_data);

stop:
    raw_stop_stream();

out:
    if (venc_fd >= 0) close(venc_fd);
    if (gfd >= 0) close(gfd);
    return 0;
}
