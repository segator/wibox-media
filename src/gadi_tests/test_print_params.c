#include <stdio.h>
#include <string.h>
#include "adi_types.h"
#include "adi_sys.h"
#include "adi_venc.h"
#include "adi_vi.h"
#include "adi_vout.h"
int main() {
  GADI_ERR err;
  gadi_sys_init();
  gadi_vi_init(); GADI_SYS_HandleT vi = gadi_vi_open(&err);
  gadi_vout_init(); GADI_SYS_HandleT vo = gadi_vout_open(&err);
  gadi_vout_set_params(vo, &(GADI_VOUT_SettingParamsT){GADI_VOUT_A,GADI_VOUT_RESOLUTION_576I,GADI_VOUT_DEVICE_CVBS});
  gadi_venc_init();
  GADI_VENC_OpenParamsT op = {vi, vo};
  GADI_SYS_HandleT venc = gadi_venc_open(&op, &err);
  printf("open: %s\n", err?"FAIL":"OK");

  /* Print params */
  gadi_venc_print_params(venc);

  /* Get channels params */
  GADI_VENC_ChannelsParamsT ch;
  memset(&ch, 0, sizeof(ch));
  err = gadi_venc_get_channels_params(venc, &ch);
  printf("get_chan: err=%d sizeof(ch)=%d\n", err, (int)sizeof(ch));
  printf("  chan1=%d w=%d h=%d\n", ch.chan1Type, ch.chan1Width, ch.chan1Height);

  gadi_venc_close(venc); gadi_venc_exit();
  gadi_vout_close(vo); gadi_vout_exit();
  gadi_vi_close(vi); gadi_vi_exit();
  gadi_sys_exit();
  return 0;
}
