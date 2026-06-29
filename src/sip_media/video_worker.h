#ifndef VIDEO_WORKER_H
#define VIDEO_WORKER_H

int video_worker_run(const char* remote_ip, int remote_port,
                     int local_port, int payload_type,
                     const char* dumpfile);

#endif
