#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "adi_types.h"
#include "adi_sys.h"
#include "adi_venc.h"
#include "adi_vi.h"
#include "adi_vout.h"

int main() {
    GADI_ERR err;

    printf("=== sip_media GADI init ===\n");

    /* 1. SYS */
    err = gadi_sys_init();
    printf("[sys_init]  %s\n", err?"FAIL":"OK"); if(err) return 1;

    /* 2. VI */
    gadi_vi_init();
    GADI_SYS_HandleT vi = gadi_vi_open(&err);
    printf("[vi_open]   %s h=%p err=%d\n", err?"FAIL":"OK", vi, err); if(err) return 1;

    /* 3. VOUT */
    gadi_vout_init();
    GADI_SYS_HandleT vo = gadi_vout_open(&err);
    printf("[vout_open] %s h=%p\n", err?"FAIL":"OK", vo);

    GADI_VOUT_SettingParamsT vp = { GADI_VOUT_A, GADI_VOUT_RESOLUTION_576I, GADI_VOUT_DEVICE_CVBS };
    gadi_vout_set_params(vo, &vp);

    /* 4. VENC */
    gadi_venc_init();
    GADI_VENC_OpenParamsT op = { vi, vo };
    GADI_SYS_HandleT venc = gadi_venc_open(&op, &err);
    printf("[venc_open] %s h=%p err=%d\n", err?"FAIL":"OK", venc, err); if(err) return 1;

    /* 5. Configure VENC channels (3 channels like Sofia) */
    GADI_VENC_ChannelsParamsT ch;
    memset(&ch, 0, sizeof(ch));
    ch.chan1Type   = 1; /* encode */
    ch.chan1Width  = 688;
    ch.chan1Height = 576;
    ch.chan2Type   = 0; /* off */
    ch.chan3Type   = 0;
    ch.chan4Type   = 0;
    err = gadi_venc_set_channels_params(venc, &ch);
    printf("[venc_chan] %s\n", err?"FAIL":"OK");

    /* 6. Configure stream 0: H.264, CBR, 688x576 */
    GADI_VENC_StreamFormatT sf;
    memset(&sf, 0, sizeof(sf));
    sf.streamId    = GADI_VENC_STREAM_FIRST;
    sf.encodeType  = GADI_VENC_TYPE_H264;
    sf.channelId   = GADI_VENC_CHANNEL_1;
    sf.width       = 688;
    sf.height      = 576;
    sf.fps         = GADI_VENC_FPS_25;
    err = gadi_venc_set_stream_format(venc, &sf);
    printf("[venc_fmt]  %s\n", err?"FAIL":"OK");

    /* 7. H.264 config: CBR 512kbps, GOP 25 */
    GADI_VENC_H264ConfigT h264;
    memset(&h264, 0, sizeof(h264));
    h264.streamId   = GADI_VENC_STREAM_FIRST;
    h264.gopM       = 1;
    h264.gopN       = 25;
    h264.idrInterval = 1;
    h264.brcMode    = GADI_VENC_CBR_MODE;
    h264.cbrAvgBps  = 512000;
    h264.adaptQp    = 2;
    h264.qpMinI     = 18;
    h264.qpMinP     = 22;
    h264.qpIWeight  = 1;
    h264.qpPWeight  = 1;
    err = gadi_venc_set_h264_config(venc, &h264);
    printf("[venc_h264] %s\n", err?"FAIL":"OK");

    /* 8. Start encoding */
    err = gadi_venc_start_stream(venc, GADI_VENC_STREAM_FIRST);
    printf("[venc_start] %s\n", err?"FAIL":"OK");
    if (err) return 1;

    /* 9. Encode a few frames */
    printf("\n=== Encoding 10 frames ===\n");
    int i;
    for (i = 0; i < 10; i++) {
        GADI_VENC_StreamT stream;
        memset(&stream, 0, sizeof(stream));
        stream.stream_id = GADI_VENC_STREAM_FIRST;
        err = gadi_venc_get_stream(venc, GADI_VENC_STREAM_FIRST, &stream);
        if (err == 0 && stream.size > 0) {
            printf("  frame %d: %d bytes type=%d\n", i, stream.size, stream.pic_type);
        } else {
            printf("  frame %d: wait... (err=%d)\n", i, err);
            usleep(40000); /* wait 40ms for frame */
        }
    }

    /* 10. Stop and cleanup */
    gadi_venc_stop_stream(venc, GADI_VENC_STREAM_FIRST);
    printf("[venc_stop] OK\n");

    gadi_venc_close(venc);
    gadi_vout_close(vo);
    gadi_vi_close(vi);
    gadi_venc_exit();
    gadi_vi_exit();
    gadi_vout_exit();
    gadi_sys_exit();

    printf("=== CLEAN SHUTDOWN ===\n");
    return 0;
}
