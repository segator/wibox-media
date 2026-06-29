#include <stdio.h>
#include <stdlib.h>
#include "adi_types.h"
#include "adi_sys.h"
#include "adi_venc.h"
#include "adi_vi.h"
#include "adi_vout.h"

int main() {
    GADI_ERR err;

    printf("=== GADI PoC v3 (full chain) ===\n\n");

    err = gadi_sys_init();
    printf("[1] sys_init:    %s\n", err?"FAIL":"OK");
    if (err) return 1;

    /* VI */
    err = gadi_vi_init();
    printf("[2] vi_init:     %s\n", err?"FAIL":"OK");
    GADI_SYS_HandleT vi = gadi_vi_open(&err);
    printf("[3] vi_open:     %s handle=%p\n", err?"FAIL":"OK", vi);
    if (err) return 1;

    /* VOUT */
    err = gadi_vout_init();
    printf("[4] vout_init:   %s\n", err?"FAIL":"OK");
    GADI_SYS_HandleT vo = gadi_vout_open(&err);
    printf("[5] vout_open:   %s handle=%p\n", err?"FAIL":"OK", vo);
    if (err) return 1;

    /* Configure VOUT — PAL 576I, CVBS */
    GADI_VOUT_SettingParamsT vp;
    vp.voutChannel = GADI_VOUT_A;
    vp.resoluMode  = GADI_VOUT_RESOLUTION_576I;
    vp.deviceType  = GADI_VOUT_DEVICE_CVBS;
    err = gadi_vout_set_params(vo, &vp);
    printf("[6] vout_params: %s\n", err?"FAIL":"OK");

    /* VENC */
    err = gadi_venc_init();
    printf("[7] venc_init:   %s\n", err?"FAIL":"OK");
    GADI_VENC_OpenParamsT op = { vi, vo };
    GADI_SYS_HandleT venc = gadi_venc_open(&op, &err);
    printf("[8] venc_open:   %s handle=%p err=%d\n", err?"FAIL":"OK", venc, err);

    printf("\n=== DONE ===\n");
    return err ? 1 : 0;
}
