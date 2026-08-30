#include "sip_sdp.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

static int parse_payload_type(const char *sdp_content, const char *media_name,
                              const char *codec_name)
{
    char marker[32];
    const char *media;
    const char *line;

    snprintf(marker, sizeof(marker), "m=%s ", media_name);
    media = strstr(sdp_content, marker);
    if (!media) return 0;

    line = media;
    while (line && *line) {
        const char *next = strstr(line, "\n");
        int payload_type = 0;
        char codec[32];

        if (line != media && strncmp(line, "m=", 2) == 0) break;
        if (sscanf(line, "a=rtpmap:%d %31[^/\r\n]", &payload_type, codec) == 2 &&
            strcasecmp(codec, codec_name) == 0) {
            return payload_type;
        }
        if (!next) break;
        line = next + 1;
    }
    return 0;
}

int sip_sdp_build(const char *local_ip, int local_rtp_port,
                  int local_video_rtp_port, int video_payload_type,
                  char *out, size_t out_size)
{
    int len;
    int written;

    if (!local_ip || !local_ip[0] || local_rtp_port <= 0 ||
        !out || out_size == 0) {
        return -1;
    }

    len = snprintf(out, out_size,
                   "v=0\r\n"
                   "o=wibox 123456 654321 IN IP4 %s\r\n"
                   "s=Wibox Media Session\r\n"
                   "c=IN IP4 %s\r\n"
                   "t=0 0\r\n"
                   "m=audio %d RTP/AVP 8 101\r\n"
                   "a=rtpmap:8 PCMA/8000\r\n"
                   "a=rtpmap:101 telephone-event/8000\r\n"
                   "a=fmtp:101 0-16\r\n"
                   "a=sendrecv\r\n",
                   local_ip, local_ip, local_rtp_port);
    if (len < 0 || (size_t)len >= out_size) return -1;

    if (local_video_rtp_port > 0) {
        written = snprintf(out + len, out_size - (size_t)len,
                           "m=video %d RTP/AVP %d\r\n"
                           "a=rtpmap:%d H264/90000\r\n"
                           "a=fmtp:%d packetization-mode=1;profile-level-id=42e01e\r\n"
                           "a=sendonly\r\n",
                           local_video_rtp_port, video_payload_type,
                           video_payload_type, video_payload_type);
        if (written < 0 || (size_t)written >= out_size - (size_t)len) return -1;
    }
    return 0;
}

int sip_sdp_parse(const char *sdp_content, int *remote_rtp_port,
                  int *remote_dtmf_payload_type,
                  int *remote_video_rtp_port,
                  int *remote_video_payload_type)
{
    const char *media_line;

    if (!sdp_content || !remote_rtp_port || !remote_video_rtp_port) return -1;

    *remote_rtp_port = 8000;
    if (remote_dtmf_payload_type) *remote_dtmf_payload_type = 101;
    *remote_video_rtp_port = 0;
    if (remote_video_payload_type) *remote_video_payload_type = 0;

    media_line = strstr(sdp_content, "m=audio ");
    if (media_line) {
        int port;
        char protocol[32] = "";
        if (sscanf(media_line, "m=audio %d %31s", &port, protocol) == 2 &&
            strstr(protocol, "RTP") != NULL && port > 0 && port <= 65535) {
            *remote_rtp_port = port;
        }
    }

    media_line = strstr(sdp_content, "m=video ");
    if (media_line) {
        int port;
        char protocol[32] = "";
        if (sscanf(media_line, "m=video %d %31s", &port, protocol) == 2 &&
            strstr(protocol, "RTP") != NULL && port > 0 && port <= 65535) {
            *remote_video_rtp_port = port;
        }
    }

    if (remote_dtmf_payload_type) {
        int payload = parse_payload_type(sdp_content, "audio", "telephone-event");
        if (payload > 0) *remote_dtmf_payload_type = payload;
    }
    if (remote_video_payload_type) {
        *remote_video_payload_type = parse_payload_type(sdp_content, "video", "H264");
    }
    return 0;
}
