/*
 * video_rtp_bridge.c - GK7102S D1 H.264 RTP sender
 *
 * SDK init order: sys_init → vi_init+open → vout_init+open → venc_init+open+map_bsb
 * Then raw fd ioctls for VENC config (SET_SRCBUF_FORMAT, etc.)
 *
 * Captures stream_id==0 as 688x576 H.264 and sends it as RTP/H264.
 *
 * Prerequisites:
 *   1. Sofia warmup once after boot (about 30s), then kill Sofia.
 *   2. Kill Sofia
 *   3. Start the call path:
 *      printf "\xfb\x14\x01\x20" > /dev/ttySGK1
 *   4. Run: video_rtp_bridge <remote_ip> <remote_port> [local_port] [payload_type]
 *   5. Stop the call path:
 *      printf "\xfb\x14\x00\x1f" > /dev/ttySGK1
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
#include <signal.h>
#include <time.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
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
#define IOC_GET_LIMITS         0x80047674  /* READ, Sofia reads then writes back */
#define IOC_SET_LIMITS         0x40047673  /* WRITE, set resource limits - MUST before FORMAT */
#define IOC_SYS_0x7B           0x4004767b  /* WRITE, Sofia calls this with NULL before limits */
#define IOC_SET_SRCBUF_FORMAT  0x40047687  /* WRITE, 40 bytes struct! */
#define IOC_SET_SRCBUF_TYPE    0x40047683  /* 4x int */
#define IOC_SET_FRAME_INTERVAL 0x40046533
#define IOC_SET_ENCODE_FORMAT  0x40046528
#define IOC_SET_BITRATE        0x40046538
#define IOC_GETSET_H264_CFG    0xc0046540
#define IOC_SET_H264_CFG       0x4004653f
#define IOC_QUERY_STREAM       0xc004652a
#define IOC_START_ENCODE       0x40046541
#define IOC_FORCE_IDR          0x4004653c
#define IOC_GET_H264_QP_CFG    0xc0046545
#define IOC_SET_H264_QP_CFG    0x40046544
#define IOC_GET_STREAM         0x80046537
/* VI source capability lookup. Sofia calls this before SET_SRCBUF_FORMAT. */
#define IOC_VI_SOURCE_CAPS     0x80047305

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
static volatile sig_atomic_t get_stream_timed_out = 0;
static volatile sig_atomic_t stop_requested = 0;

struct rtp_sender {
    int fd;
    struct sockaddr_in remote;
    uint16_t seq;
    uint32_t timestamp;
    uint32_t ssrc;
    uint8_t payload_type;
};

static void on_alarm(int sig)
{
    (void)sig;
    get_stream_timed_out = 1;
}

static void on_stop(int sig)
{
    (void)sig;
    stop_requested = 1;
}

static int get_stream_with_timeout(GADI_SYS_HandleT venc, int stream_id,
                                   GADI_VENC_StreamT *st)
{
    int ret;
    get_stream_timed_out = 0;
    alarm(1);
    ret = gadi_venc_get_stream(venc, stream_id, st);
    alarm(0);
    if (get_stream_timed_out) {
        errno = ETIMEDOUT;
        return -1;
    }
    return ret;
}

static void try_stop_stream(void) {
    /* 0x40047654 = SET_ENCODE_STATE - value 0 = stop/reset */
    uint32_t v = 0;
    int r = ioctl(gfd, IOC_SET_ENCODE_STATE, &v);
    printf("[RESET_STATE] ioctl(0x40047654, 0) ret=%d errno=%d\n", r, errno);
    usleep(100000);
}

static void set_resource_limits(void) {
    uint8_t limits[128];
    int r;

    memset(limits, 0, sizeof(limits));
    r = ioctl(gfd, IOC_GET_LIMITS, limits);
    printf("[GET_LIMITS] ioctl(0x80047674) ret=%d errno=%d bytes=%02x %02x %02x %02x\n",
           r, errno, limits[0], limits[1], limits[2], limits[3]);

    r = ioctl(gfd, IOC_SET_LIMITS, limits);
    printf("[SET_LIMITS] ioctl(0x40047673) ret=%d errno=%d bytes=%02x %02x %02x %02x\n",
           r, errno, limits[0], limits[1], limits[2], limits[3]);
}

static int get_vi_caps_once(int fd)
{
    uint32_t caps[6] = {0}; /* Sofia returns 24 bytes in traces for this ioctl */
    int r = ioctl(fd, IOC_VI_SOURCE_CAPS, caps);
    printf("[VI_CAPS] ioctl(0x80047305) ret=%d errno=%d\n", r, errno);
    if (r == 0) {
        printf("[VI_CAPS] values: ");
        for (unsigned i = 0; i < 6; i++) {
            printf("0x%08x ", caps[i]);
        }
        printf("\n");
    }
    return r;
}

static int set_srcbuf_format(void)
{
    struct srcbuf_format_t fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.main_width = 688;       fmt.main_height = 576;
    fmt.ch_mode_0 = 1;
    fmt.sub1_w = 352;           fmt.sub1_h = 300;
    fmt.main_w_dup = 688;       fmt.main_h_dup = 576;
    fmt.ch_mode_1 = 1;
    fmt.sub2_w = 352;           fmt.sub2_h = 288;
    fmt.main_w_dup2 = 688;      fmt.main_h_dup2 = 576;
    fmt.ch_mode_2 = 1;
    fmt.sub3_w = 0;             fmt.sub3_h = 0;
    fmt.main_w_dup3 = 688;      fmt.main_h_dup3 = 576;
    fmt.ch_mode_3 = 0;
    fmt.interlace_scan = 1;

    printf("[SET_SRCBUF_FORMAT] Sofia D1 layout struct_size=%zu\n", sizeof(fmt));
    uint16_t *p = (uint16_t *)&fmt;
    printf("  bytes: %04x %04x %04x %04x %04x %04x %04x %04x\n",
           p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
    int ret = ioctl(gfd, IOC_SET_SRCBUF_FORMAT, &fmt);
    printf("[SET_SRCBUF_FORMAT] -> ret=%d errno=%d (%s)\n",
           ret, errno, strerror(errno));
    return ret;
}

static void dump_bytes(const char *tag, const uint8_t *buf, size_t len)
{
    size_t n = len < 80 ? len : 80;
    printf("%s", tag);
    for (size_t i = 0; i < n; i++) {
        if ((i % 16) == 0) printf("\n  %04zu:", i);
        printf(" %02x", buf[i]);
    }
    printf("\n");
}

static int rtp_sender_open(struct rtp_sender *rtp, const char *remote_ip,
                           int remote_port, int local_port, int payload_type)
{
    struct sockaddr_in local;
    memset(rtp, 0, sizeof(*rtp));
    rtp->fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (rtp->fd < 0) {
        perror("socket");
        return -1;
    }

    memset(&local, 0, sizeof(local));
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = INADDR_ANY;
    local.sin_port = htons((uint16_t)local_port);
    if (bind(rtp->fd, (struct sockaddr *)&local, sizeof(local)) < 0) {
        perror("bind");
        close(rtp->fd);
        rtp->fd = -1;
        return -1;
    }

    memset(&rtp->remote, 0, sizeof(rtp->remote));
    rtp->remote.sin_family = AF_INET;
    rtp->remote.sin_port = htons((uint16_t)remote_port);
    if (inet_pton(AF_INET, remote_ip, &rtp->remote.sin_addr) != 1) {
        fprintf(stderr, "Invalid remote IP: %s\n", remote_ip);
        close(rtp->fd);
        rtp->fd = -1;
        return -1;
    }

    rtp->seq = (uint16_t)(time(NULL) & 0xffff);
    rtp->timestamp = (uint32_t)time(NULL) * 90000U;
    rtp->ssrc = 0x57425630U; /* "WBV0" */
    rtp->payload_type = (uint8_t)payload_type;
    return 0;
}

static int rtp_send_packet(struct rtp_sender *rtp, const uint8_t *payload,
                           size_t payload_len, int marker)
{
    uint8_t pkt[1400];
    if (payload_len + 12 > sizeof(pkt)) {
        return -1;
    }

    pkt[0] = 0x80;
    pkt[1] = (marker ? 0x80 : 0x00) | (rtp->payload_type & 0x7f);
    pkt[2] = (uint8_t)(rtp->seq >> 8);
    pkt[3] = (uint8_t)(rtp->seq);
    pkt[4] = (uint8_t)(rtp->timestamp >> 24);
    pkt[5] = (uint8_t)(rtp->timestamp >> 16);
    pkt[6] = (uint8_t)(rtp->timestamp >> 8);
    pkt[7] = (uint8_t)(rtp->timestamp);
    pkt[8] = (uint8_t)(rtp->ssrc >> 24);
    pkt[9] = (uint8_t)(rtp->ssrc >> 16);
    pkt[10] = (uint8_t)(rtp->ssrc >> 8);
    pkt[11] = (uint8_t)(rtp->ssrc);
    memcpy(pkt + 12, payload, payload_len);

    rtp->seq++;
    if (sendto(rtp->fd, pkt, payload_len + 12, 0,
               (struct sockaddr *)&rtp->remote, sizeof(rtp->remote)) < 0) {
        return -1;
    }
    return 0;
}

static int rtp_send_nal(struct rtp_sender *rtp, const uint8_t *nal,
                        size_t nal_len, int marker)
{
    const size_t max_payload = 1200;
    uint8_t fu[1202];
    size_t pos;

    if (nal_len == 0) {
        return 0;
    }
    if (nal_len <= max_payload) {
        return rtp_send_packet(rtp, nal, nal_len, marker);
    }

    fu[0] = (nal[0] & 0xe0) | 28; /* FU-A */
    pos = 1;
    while (pos < nal_len) {
        size_t chunk = nal_len - pos;
        int start = (pos == 1);
        int end;
        if (chunk > max_payload - 2) {
            chunk = max_payload - 2;
        }
        end = (pos + chunk == nal_len);
        fu[1] = (start ? 0x80 : 0x00) | (end ? 0x40 : 0x00) | (nal[0] & 0x1f);
        memcpy(fu + 2, nal + pos, chunk);
        if (rtp_send_packet(rtp, fu, chunk + 2, marker && end) < 0) {
            return -1;
        }
        pos += chunk;
    }
    return 0;
}

static const uint8_t *find_start_code(const uint8_t *p, const uint8_t *end, int *sc_len)
{
    while (p + 3 < end) {
        if (p[0] == 0 && p[1] == 0 && p[2] == 1) {
            *sc_len = 3;
            return p;
        }
        if (p + 4 < end && p[0] == 0 && p[1] == 0 && p[2] == 0 && p[3] == 1) {
            *sc_len = 4;
            return p;
        }
        p++;
    }
    return NULL;
}

static int rtp_send_annexb(struct rtp_sender *rtp, const uint8_t *data, size_t len)
{
    const uint8_t *end = data + len;
    const uint8_t *sc;
    int sc_len;
    int sent = 0;

    sc = find_start_code(data, end, &sc_len);
    if (!sc) {
        return rtp_send_nal(rtp, data, len, 1);
    }

    while (sc) {
        const uint8_t *nal = sc + sc_len;
        const uint8_t *next = find_start_code(nal, end, &sc_len);
        const uint8_t *nal_end = next ? next : end;
        while (nal_end > nal && nal_end[-1] == 0) {
            nal_end--;
        }
        if (nal_end > nal) {
            if (rtp_send_nal(rtp, nal, (size_t)(nal_end - nal), next == NULL) < 0) {
                return -1;
            }
            sent++;
        }
        sc = next;
    }
    return sent;
}

static void set_frame_interval_stream(int stream_id)
{
    struct {
        uint32_t mask;
        uint8_t  numerator;
        uint8_t  denominator;
        uint8_t  pad[2];
    } fi;
    memset(&fi, 0, sizeof(fi));
    fi.mask = 1U << stream_id;
    fi.numerator = 60;
    fi.denominator = 60;
    int r = ioctl(gfd, IOC_SET_FRAME_INTERVAL, &fi);
    printf("[SET_FRAME_INTERVAL:%d] ret=%d errno=%d\n", stream_id, r, errno);
}

static void set_encode_format_stream(int stream_id, int channel_id,
                                     uint16_t width, uint16_t height,
                                     uint32_t fps)
{
    uint8_t fmt[128];
    memset(fmt, 0, sizeof(fmt));
    *(uint32_t *)(fmt + 0) = 1U << stream_id;
    fmt[4] = 1;                      /* encodeType: H264 */
    fmt[5] = (uint8_t)channel_id;
    fmt[6] = 0;                      /* flipRotate */
    *(uint16_t *)(fmt + 8) = width;
    *(uint16_t *)(fmt + 10) = height;
    *(uint16_t *)(fmt + 12) = 0;     /* xOffset */
    *(uint16_t *)(fmt + 14) = 0;     /* yOffset */
    *(uint32_t *)(fmt + 16) = fps;
    fmt[20] = 0;                     /* keepAspRat */

    int r = ioctl(gfd, IOC_SET_ENCODE_FORMAT, fmt);
    printf("[SET_ENCODE_FORMAT:%d] ret=%d errno=%d ch=%d %ux%u fps=%u\n",
           stream_id, r, errno, channel_id, width, height, fps);
}

static void set_bitrate_stream(int stream_id, uint32_t cbr, uint32_t min, uint32_t max)
{
    uint8_t br[128];
    memset(br, 0, sizeof(br));
    *(uint32_t *)(br + 0) = 1U << stream_id;
    *(uint32_t *)(br + 4) = 0;
    *(uint32_t *)(br + 8) = cbr;
    *(uint32_t *)(br + 12) = min;
    *(uint32_t *)(br + 16) = max;
    int r = ioctl(gfd, IOC_SET_BITRATE, br);
    printf("[SET_BITRATE:%d] ret=%d errno=%d cbr=%u min=%u max=%u\n",
           stream_id, r, errno, cbr, min, max);
}

static void set_h264_config_stream(int stream_id, uint32_t cbr, uint32_t min, uint32_t max)
{
    uint8_t cfg[512];
    memset(cfg, 0, sizeof(cfg));
    *(uint32_t *)(cfg + 0) = 1U << stream_id;
    int ret = ioctl(gfd, IOC_GETSET_H264_CFG, cfg);
    printf("[H264:%d] GET ret=%d errno=%d\n", stream_id, ret, errno);
    if (ret != 0) return;

    dump_bytes("[H264] before", cfg, 80);

    *(uint16_t *)(cfg + 44) = 1;      /* gopM */
    *(uint16_t *)(cfg + 46) = 25;     /* gopN */
    cfg[48] = 1;                      /* idrInterval: Sofia default */
    cfg[49] = 0;                      /* gopModel */
    cfg[50] = 4;                      /* internal field set by Sofia wrapper */
    cfg[54] = 0;                      /* GK SDK default: produces D1 stream_id 0 reliably */
    *(uint32_t *)(cfg + 4) = cbr;     /* selected bitrate copied by wrapper */
    cfg[110] = 0;                     /* reEncMode */

    ret = ioctl(gfd, IOC_SET_H264_CFG, cfg);
    printf("[H264:%d] SET ret=%d errno=%d gop=%u/%u idr=%u profile=%u\n",
           stream_id, ret, errno, *(uint16_t *)(cfg + 44),
           *(uint16_t *)(cfg + 46), cfg[48], cfg[54]);
    dump_bytes("[H264] after", cfg, 80);

    set_bitrate_stream(stream_id, cbr, min, max);
}

static void set_h264_qp_stream(int stream_id)
{
    uint8_t qp[32];
    memset(qp, 0, sizeof(qp));
    *(uint32_t *)(qp + 0) = 1U << stream_id;
    int ret = ioctl(gfd, IOC_GET_H264_QP_CFG, qp);
    printf("[H264_QP:%d] GET ret=%d errno=%d\n", stream_id, ret, errno);
    if (ret != 0) return;

    qp[4] = 0x17;  /* qpMinOnI */
    qp[5] = 0x33;  /* qpMaxOnI */
    qp[6] = 0x23;  /* qpMinOnP */
    qp[7] = 0x33;  /* qpMaxOnP */
    qp[8] = 3;     /* qpIWeight */
    qp[9] = 5;     /* qpPWeight */
    qp[10] = 2;    /* adaptQp */
    ret = ioctl(gfd, IOC_SET_H264_QP_CFG, qp);
    printf("[H264_QP:%d] SET ret=%d errno=%d\n", stream_id, ret, errno);
}

static void query_stream0(const char *tag)
{
    uint32_t q[8];
    memset(q, 0, sizeof(q));
    q[0] = 1;
    int r = ioctl(gfd, IOC_QUERY_STREAM, q);
    printf("[QUERY_STREAM %s] ret=%d errno=%d q=%08x %08x %08x %08x\n",
           tag, r, errno, q[0], q[1], q[2], q[3]);
}

int main(int argc, char **argv)
{
    const char *remote_ip;
    int remote_port;
    int local_port;
    int payload_type;
    const char *dumpfile;
    FILE *fout = NULL;
    struct rtp_sender rtp;
    GADI_ERR err;
    int ret;

    if (argc < 3) {
        fprintf(stderr,
                "Usage: %s <remote_ip> <remote_video_port> [local_video_port] [payload_type] [dump.h264]\n",
                argv[0]);
        return 1;
    }

    remote_ip = argv[1];
    remote_port = atoi(argv[2]);
    local_port = argc > 3 ? atoi(argv[3]) : 8002;
    payload_type = argc > 4 ? atoi(argv[4]) : 96;
    dumpfile = argc > 5 ? argv[5] : NULL;

    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    {
        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = on_alarm;
        sigemptyset(&sa.sa_mask);
        sigaction(SIGALRM, &sa, NULL);
    }
    signal(SIGTERM, on_stop);
    signal(SIGINT, on_stop);

    printf("=== WiBox D1 Video RTP Bridge ===\n");
    printf("Remote: %s:%d\n", remote_ip, remote_port);
    printf("Local RTP port: %d\n", local_port);
    printf("Payload type: %d\n", payload_type);
    if (dumpfile) printf("Debug dump: %s\n", dumpfile);
    printf("\n");

    if (remote_port <= 0 || local_port <= 0 || payload_type <= 0 || payload_type > 127) {
        fprintf(stderr, "Invalid RTP arguments\n");
        return 1;
    }
    if (rtp_sender_open(&rtp, remote_ip, remote_port, local_port, payload_type) < 0) {
        return 1;
    }
    if (dumpfile) {
        fout = fopen(dumpfile, "wb");
        if (!fout) perror("fopen dump");
    }

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

    {
        GADI_VENC_DspMapInfoT dspInfo;
        memset(&dspInfo, 0, sizeof(dspInfo));
        err = gadi_venc_map_dsp(venc_handle, &dspInfo);
        printf("[SDK] map_dsp: %s addr=%p length=%u\n\n",
               err ? "FAIL" : "OK", dspInfo.addr, dspInfo.length);
    }

    /* ── Switch to raw fd for VENC config ── */
    printf("[RAW] Opening /dev/gk_video...\n");
    gfd = open(DEVICE, O_RDWR);
    if (gfd < 0) { perror("open"); return 1; }
    printf("[RAW] fd=%d\n\n", gfd);

    /* Sofia does this before SET_SRCBUF_FORMAT; populate sys_state[0xdc]. */
    get_vi_caps_once(gfd);

    /* ── Chip info ── */
    {
        uint32_t ci[2] = {0};
        ioctl(gfd, IOC_GET_CHIP_INFO, ci);
        printf("[INFO] chip_type=%d\n\n", ci[0]);
    }

    {
        ret = ioctl(gfd, IOC_SYS_0x7B, 0);
        printf("[SYS_0x7B] ioctl(0x4004767b) ret=%d errno=%d\n", ret, errno);
    }

    /* ── Stop/Reset encoder state ── */
    printf("[RESET] Resetting encode state...\n");
    try_stop_stream();

    /* ── Set resource limits (required before SET_SRCBUF_FORMAT) ── */
    printf("[LIMITS] Setting resource limits...\n");
    set_resource_limits();

    /* ── Key ioctl: SET_SRCBUF_FORMAT ── */
    printf("[CONFIG] SET_SRCBUF_FORMAT (Sofia D1 40-byte struct)...\n");
    ret = set_srcbuf_format();
    if (ret < 0) {
        fprintf(stderr, "FATAL: SET_SRCBUF_FORMAT failed.\n");
        fprintf(stderr, "Did Sofia run for 25+s? sys_state needs VI init.\n");
        goto out;
    }

    /* ── SET_SRCBUF_TYPE: channel 0 = H264 ── */
    printf("[CONFIG] SET_SRCBUF_TYPE...\n");
    {
        uint32_t types[4] = {1, 1, 1, 0};
        ret = ioctl(gfd, IOC_SET_SRCBUF_TYPE, types);
        printf("[CONFIG] SET_SRCBUF_TYPE ret=%d\n\n", ret);
    }

    /* ── H264 config: Sofia configures streams 0, 1 and 2 before start. ── */
    printf("[CONFIG] Streams 0/1/2 encode + H264 defaults...\n");
    set_frame_interval_stream(0);
    set_encode_format_stream(0, 0, 688, 576, 25);
    set_h264_config_stream(0, 0xc0000, 0x80000, 0x140000);

    set_frame_interval_stream(1);
    set_encode_format_stream(1, 1, 352, 300, 25);
    set_h264_config_stream(1, 0x40000, 0x40000, 0x80000);

    set_frame_interval_stream(2);
    set_encode_format_stream(2, 2, 352, 288, 25);
    set_h264_config_stream(2, 0x40000, 0x40000, 0x80000);
    printf("\n");

    err = gadi_vi_enable(vi_handle, 1);
    printf("[SDK] vi_enable(1): %s err=%d\n", err ? "FAIL" : "OK", err);
    usleep(300000);

    /* ── START_STREAM ── */
    printf("[START] Starting streams...\n");
    {
        query_stream0("before");
        const uint32_t masks[] = {1, 2, 4};
        for (int m = 0; m < 3; m++) {
            ret = -1;
            for (int attempt = 0; attempt < 10; attempt++) {
                errno = 0;
                ret = ioctl(gfd, IOC_START_ENCODE, masks[m]);
                printf("[START] mask=0x%x attempt=%d ret=%d errno=%d\n",
                       masks[m], attempt + 1, ret, errno);
                if (ret == 0) break;
                usleep(200000);
            }
        }
        query_stream0("after");
        printf("\n");
        if (ret < 0) {
            fprintf(stderr, "START_STREAM failed, trying with factory state...\n");
        }
    }
    usleep(300000);

    /* ── FORCE IDR ── */
    {
        uint32_t ch = 1; /* Sofia passes a stream mask: 1 << streamId */
        ret = ioctl(gfd, IOC_FORCE_IDR, ch);
        printf("[IDR] force_idr ret=%d\n\n", ret);
    }
    usleep(100000);

    /* ── CAPTURE LOOP ── */
    printf("[CAPTURE] Starting RTP stream\n");

    uint8_t *sps_buf = NULL, *pps_buf = NULL;
    size_t   sps_len = 0,     pps_len = 0;
    int sps_fresh = 0, pps_fresh = 0;
    long long total_bytes = 0;
    int frames = 0, idrs = 0, errors = 0;
    int seen_sid[16] = {0};
    int seen_nal[32] = {0};

    for (int i = 0; !stop_requested && errors < 30; i++) {
        GADI_VENC_StreamT st;
        memset(&st, 0, sizeof(st));

        ret = get_stream_with_timeout(venc_handle, 0xFF, &st);
        if (ret < 0) { errors++; usleep(10000); continue; }
        errors = 0;

        if (!st.size || !st.addr) { usleep(5000); continue; }

        uint8_t *data = st.addr;
        uint32_t  sz  = st.size;

        if ((i % 50) == 0) {
            uint32_t force = 1;
            ioctl(gfd, IOC_FORCE_IDR, force);
        }

        /* Parse NAL type */
        int nal = -1;
        if (sz >= 5 && data[0]==0 && data[1]==0 && data[2]==0 && data[3]==1)
            nal = data[4] & 0x1f;
        else if (sz >= 4 && data[0]==0 && data[1]==0 && data[2]==1)
            nal = data[3] & 0x1f;

        if (st.stream_id < 16) seen_sid[st.stream_id]++;
        if (nal >= 0 && nal < 32) seen_nal[nal]++;
        if (i < 40 || nal == 5 || nal == 7 || nal == 8) {
            printf("  [pkt %03d] sid=%u nal=0x%02x sz=%u addr=%p\n",
                   i, st.stream_id, nal, sz, st.addr);
        }

        /* Main stream only: stream_id == 0 */
        if (st.stream_id != 0) { usleep(1000); continue; }

        if (nal == 7) { /* SPS */
            free(sps_buf);
            sps_buf = malloc(sz); memcpy(sps_buf, data, sz); sps_len = sz;
            sps_fresh = 1; pps_fresh = 0;
        }
        if (nal == 8) { /* PPS */
            free(pps_buf);
            pps_buf = malloc(sz); memcpy(pps_buf, data, sz); pps_len = sz;
            pps_fresh = 1;
        }
        if (nal == 5) { /* IDR - write SPS+PPS first */
            if (fout && sps_buf && sps_fresh) {
                fwrite(sps_buf, 1, sps_len, fout);
                total_bytes += sps_len;
                sps_fresh = 0;
            }
            if (fout && pps_buf && pps_fresh) {
                fwrite(pps_buf, 1, pps_len, fout);
                total_bytes += pps_len;
                pps_fresh = 0;
            }
            idrs++;
        }
        if (st.stream_id == 0) {
            if (rtp_send_annexb(&rtp, data, sz) < 0) {
                printf("[RTP] send failed errno=%d (%s)\n", errno, strerror(errno));
            }
            rtp.timestamp += 3600; /* 90 kHz / 25 fps */
            if (fout) {
                fwrite(data, 1, sz, fout);
                fflush(fout);
            }
            total_bytes += sz;
            if (nal == 5 || nal == 1 || nal < 0) frames++;
            if (frames % 25 == 0)
                printf("  [%d frames] %lld bytes, %d IDRs (sid=%d nal=0x%02x sz=%u)\n",
                       frames, total_bytes, idrs, st.stream_id, nal, sz);
        }
        usleep(3000);
    }

    printf("\n[CAPTURE] Done: %d frames, %lld bytes, %d IDRs\n",
           frames, total_bytes, idrs);
    printf("[CAPTURE] stream_ids:");
    for (int s = 0; s < 16; s++) if (seen_sid[s]) printf(" %d=%d", s, seen_sid[s]);
    printf("\n[CAPTURE] nal_types:");
    for (int n = 0; n < 32; n++) if (seen_nal[n]) printf(" %d=%d", n, seen_nal[n]);
    printf("\n");
    if (fout) fclose(fout);
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
    if (rtp.fd >= 0) close(rtp.fd);
    return 0;
}
