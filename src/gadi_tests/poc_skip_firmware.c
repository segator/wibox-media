#include <stdio.h>
#include <stdlib.h>
#include "adi_types.h"
#include "adi_sys.h"
#include "adi_venc.h"
#include "adi_vi.h"

int main() {
    GADI_ERR err;

    printf("=== GADI PoC v2 (skip firmware) ===\n");

    err = gadi_sys_init();
    printf("[1] sys_init: %s (err=%d)\n", err?"FAIL":"OK", err);
    if (err) return 1;

    /* Skip firmware — try VI/VENC without it */
    printf("[2] firmware: SKIPPED\n");

    /* Init VI */
    err = gadi_vi_init();
    printf("[3] vi_init: %s (err=%d)\n", err?"FAIL":"OK", err);
    if (err) return 1;

    GADI_SYS_HandleT viHandle = gadi_vi_open(&err);
    printf("[4] vi_open: %s handle=%p (err=%d)\n", err?"FAIL":"OK", viHandle, err);
    if (err) return 1;

    /* Init VENC */
    err = gadi_venc_init();
    printf("[5] venc_init: %s (err=%d)\n", err?"FAIL":"OK", err);
    if (err) return 1;

    GADI_VENC_OpenParamsT openParams = { viHandle, NULL };
    GADI_SYS_HandleT vencHandle = gadi_venc_open(&openParams, &err);
    printf("[6] venc_open: %s handle=%p (err=%d)\n", err?"FAIL":"OK", vencHandle, err);

    printf("\n=== DONE ===\n");
    return err ? 1 : 0;
}
