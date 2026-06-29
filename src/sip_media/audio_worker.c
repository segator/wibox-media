#include "audio_worker.h"

#define config_init_defaults audio_bridge_config_init_defaults
#define config_load audio_bridge_config_load
#define config_print audio_bridge_config_print
#include "../audio_bridge/config.c"
#undef THIS_FILE

#include "../audio_bridge/ap.c"

#define main audio_bridge_main
#include "../audio_bridge/audio_bridge.c"
#undef main
#undef config_init_defaults
#undef config_load
#undef config_print

int audio_worker_run(void) {
    char* argv[] = { "wibox-media-daemon-audio", NULL };
    return audio_bridge_main(1, argv);
}
