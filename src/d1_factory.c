/*
 * d1_factory.c - D1 capture using Sofia factory config (NO set_srcbuf_format needed)
 *
 * Approach: Sofia sets up encoders during warmup. We just restart stream 0 (D1 main)
 * and capture it. stream_id=0 = type0 encoder = D1 688x576.
 *
 * Prerequisites:
 *   1. Sofia warmup 25-30s (sets up ALL encoders including channel 0 = D1 688x576)
 *   2. Kill Sofia
 *   3. Run this program IMMEDIATELY
 *
 * Key: filter by stream_id==0 for main D1 stream (previous sessions used sid==2 = sub)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <errno.h>
#include <signal.h>
#include "adi_types.h"
#include "adi_sys.h"
#include "adi_vi.h"
#include "adi_vout.h"
#include "adi_venc.h"

#define DEVICE "/dev/gk_video"

/* Stream start/stop ioctls */
#define IOC_START_STREAM   0xc004652a
#define IOC_STOP_STREAM    0x40047654
#define IOC_FORCE_IDR      0x4004653c
#define IOC_GET_CHIP_INFO  0x80047670

static volatile sig_atomic_t get_stream_timed_out = 0;

static void on_alarm(int sig)
{
    (void)sig;
    get_stream_timed_out = 1;
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

int main(int argc, char **argv)
{
    const char *outfile = argc > 1 ? argv[1] : "/tmp/d1_factory.h264";
    GADI_ERR err;
    int ret;

    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    {
        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = on_alarm;
        sigemptyset(&sa.sa_mask);
        sigaction(SIGALRM, &sa, NULL);
    }

    printf("=== D1 Factory Capture ===\n");
    printf("Output: %s\n\n", outfile);

    /* SDK Init */
    err = gadi_sys_init();
    printf("[SDK] sys_init: %s\n", err?"FAIL":"OK");
    if (err) return 1;

    err = gadi_vi_init();
    GADI_SYS_HandleT vi = gadi_vi_open(&err);
    printf("[SDK] vi: %s handle=%p\n", err?"FAIL":"OK", vi);
    if (err) return 1;

    err = gadi_vout_init();
    GADI_SYS_HandleT vo = gadi_vout_open(&err);
    printf("[SDK] vout: %s handle=%p\n", err?"FAIL":"OK", vo);
    if (err) return 1;

    err = gadi_venc_init();
    GADI_VENC_OpenParamsT op;
    memset(&op, 0, sizeof(op));
    op.viHandle = vi; op.voutHandle = vo;
    GADI_SYS_HandleT venc = gadi_venc_open(&op, &err);
    printf("[SDK] venc: %s handle=%p\n", err?"FAIL":"OK", venc);
    if (err) return 1;

    err = gadi_venc_map_bsb(venc);
    printf("[SDK] map_bsb: %s\n\n", err?"FAIL":"OK");
    if (err) return 1;

    /* Raw fd for ioctl operations */
    int gfd = open(DEVICE, O_RDWR);
    printf("[RAW] fd=%d\n", gfd);

    /* Chip info */
    {
        uint32_t ci[2] = {0};
        ioctl(gfd, IOC_GET_CHIP_INFO, ci);
        printf("[INFO] chip_type=%d\n\n", ci[0]);
    }

    /* Force encoder state to idle before restart, then start stream channel 0. */
    {
        uint32_t stop = 0;
        int stop_ret = ioctl(gfd, IOC_STOP_STREAM, &stop);
        printf("[STOP_STREAM] ret=%d errno=%d\n", stop_ret, errno);
    }

    printf("[START] Starting main stream (channel 0)...\n");
    {
        uint32_t s[2] = {0, 0};  /* channel 0 */
        ret = ioctl(gfd, IOC_START_STREAM, s);
        printf("[START_STREAM] ret=%d errno=%d\n", ret, errno);
        if (ret < 0) {
            printf("[START_STREAM] retrying channel=0 after reset delay...\n");
            usleep(200000);
            ret = ioctl(gfd, IOC_START_STREAM, s);
            printf("[START_STREAM] retry ret=%d errno=%d\n", ret, errno);
        }
    }
    usleep(500000);

    /* Force IDR */
    {
        uint32_t ch = 0;
        ret = ioctl(gfd, IOC_FORCE_IDR, &ch);
        printf("[IDR] force_idr ret=%d\n\n", ret);
    }
    usleep(100000);

    /* Capture */
    printf("[CAPTURE] Starting... Target: stream_id=0 (D1 main)\n");
    FILE *fout = fopen(outfile, "wb");
    if (!fout) { perror("fopen"); return 1; }

    uint8_t *sps_buf=NULL, *pps_buf=NULL;
    size_t sps_len=0, pps_len=0;
    int sps_fresh=0, pps_fresh=0;
    long long total=0;
    int frames=0, idrs=0, errors=0;
    int stream_ids_seen[8] = {0};
    int sid0_frames = 0;
    int pass_frames = 0;
    int invalid_addr_count = 0;
    int wildcard_mode = 0;

    for (int pass = 0; pass < 2; pass++) {
        printf("\n[CAPTURE] Pass %d: stream_id=%s\n", pass + 1,
               wildcard_mode ? "0xFF" : "0");

        errors = 0;
        pass_frames = 0;
        for (int i=0; i<800 && errors<30; i++) {
            GADI_VENC_StreamT st;
            memset(&st, 0, sizeof(st));
            ret = get_stream_with_timeout(venc, wildcard_mode ? 0xFF : 0, &st);
            if (ret < 0) { errors++; usleep(10000); continue; }
            errors = 0;
            if (!st.size || !st.addr) { usleep(5000); continue; }
            if ((uintptr_t)st.addr < 0x1000) {
                invalid_addr_count++;
                if (invalid_addr_count <= 8) {
                    printf("[WARN] suspicious stream addr=%p size=%u sid=%d\n",
                           st.addr, st.size, st.stream_id);
                }
                usleep(5000);
                continue;
            }
            if (st.size > 4 * 1024 * 1024) {
                printf("[WARN] oversized NAL size=%u sid=%d\n", st.size, st.stream_id);
                usleep(5000);
                continue;
            }

            if (st.stream_id < 8) stream_ids_seen[st.stream_id]++;

            if (wildcard_mode == 0 && st.stream_id != 0) {
                continue;
            }
            if (st.stream_id == 0) {
                sid0_frames++;
                pass_frames++;
            }

            uint8_t *data = st.addr;
            uint32_t sz = st.size;
            int nal = -1;
            if (sz >= 5 && data[0]==0 && data[1]==0 && data[2]==0 && data[3]==1)
                nal = data[4] & 0x1f;

            if (nal == 7) {
                free(sps_buf); sps_buf=malloc(sz); memcpy(sps_buf,data,sz); sps_len=sz;
                sps_fresh=1; pps_fresh=0; continue;
            }
            if (nal == 8) {
                free(pps_buf); pps_buf=malloc(sz); memcpy(pps_buf,data,sz); pps_len=sz;
                pps_fresh=1; continue;
            }
            if (nal == 5) {
                if (sps_buf && sps_fresh) { fwrite(sps_buf,1,sps_len,fout); total+=sps_len; sps_fresh=0; }
                if (pps_buf && pps_fresh) { fwrite(pps_buf,1,pps_len,fout); total+=pps_len; pps_fresh=0; }
                idrs++;
            }
            if (nal == 5 || nal == 1) {
                if (st.stream_id != 0) continue;
                fwrite(data,1,sz,fout); total+=sz; frames++;
                if (frames % 25 == 0)
                    printf("  [%d] %lld bytes, %d IDRs (sid=%d nal=0x%02x sz=%u)\n",
                           frames, total, idrs, st.stream_id, nal, sz);
            }
            usleep(3000);
        }

        if (wildcard_mode == 1) {
            break;
        }

        if (pass_frames > 0) {
            sid0_frames = pass_frames;
            break;
        }

        printf("[CAPTURE] No stream_id=0 frames. Switching to wildcard + filtering.\n");
        wildcard_mode = 1;
    }

    printf("\n[CAPTURE] Done: %d frames, %lld bytes, %d IDRs\n", frames, total, idrs);
    printf("Stream IDs seen: ");
    for (int k=0; k<8; k++) if (stream_ids_seen[k]) printf("sid%d=%d ", k, stream_ids_seen[k]);
    printf("\n");
    printf("Start mode was: %s\n", wildcard_mode ? "0xFF wildcard" : "direct sid 0");

    if (sid0_frames == 0) {
        printf("No D1 (stream_id=0) frames yet. Re-run with same binary to inspect raw stream mix above.\n");
    }

    fclose(fout);
    free(sps_buf); free(pps_buf);

    if (frames == 0) {
        printf("\nNO D1 FRAMES! Stream IDs seen above might indicate correct stream.\n");
        printf("Try with stream_id filter = 1 or 2 in next run.\n");
    }

    close(gfd);
    return 0;
}
