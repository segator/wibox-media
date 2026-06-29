#include "video_worker.h"

#include <stdio.h>

#define main video_bridge_main
#include "../video_rtp_bridge/video_rtp_bridge.c"
#undef main

int video_worker_run(const char* remote_ip, int remote_port,
                     int local_port, int payload_type,
                     const char* dumpfile) {
    char remote_port_arg[16];
    char local_port_arg[16];
    char payload_type_arg[16];
    char* argv[6];
    int argc = 5;

    snprintf(remote_port_arg, sizeof(remote_port_arg), "%d", remote_port);
    snprintf(local_port_arg, sizeof(local_port_arg), "%d", local_port);
    snprintf(payload_type_arg, sizeof(payload_type_arg), "%d", payload_type);

    argv[0] = "wibox-media-daemon-video";
    argv[1] = (char*)remote_ip;
    argv[2] = remote_port_arg;
    argv[3] = local_port_arg;
    argv[4] = payload_type_arg;
    argv[5] = (char*)dumpfile;
    if (dumpfile) {
        argc = 6;
    }

    return video_bridge_main(argc, argv);
}
