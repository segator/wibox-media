#include "rtsp_stream.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

#define MAX_RTSP_CLIENTS 2
#define RTSP_REQUEST_MAX 2048
#define RTSP_RX_BUFFER_MAX 4096
#define RTP_PACKET_MAX 2048
#define DEFAULT_RTSP_PORT 8554
/* Accepted-socket timeouts so a stuck/idle peer cannot block the server: recv
 * wakes the worker periodically, send bounds how long a broadcast can stall. */
#define RTSP_CLIENT_RCVTIMEO_SEC 5
#define RTSP_CLIENT_SNDTIMEO_SEC 2
/* A client that never reaches PLAY is dropped after this many recv timeouts so
 * two silent connections cannot permanently hold both slots. */
#define RTSP_HANDSHAKE_MAX_TIMEOUTS 3

typedef struct {
    int fd;
    int active;
    int playing;
    int has_video;
    int has_audio;
    int video_channel;
    int audio_channel;
    int thread_running;   /* slot owned by a live worker; block reuse until it exits */
    unsigned int session_id;
    pthread_t thread;
    pthread_mutex_t send_mutex;
} rtsp_client_t;

static pthread_t listener_thread;
static pthread_t video_reader_thread;
static pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
static rtsp_client_t clients[MAX_RTSP_CLIENTS];
static int server_fd = -1;
static int video_pipe_read_fd = -1;
static int video_pipe_write_fd = -1;
static int rtsp_port = DEFAULT_RTSP_PORT;
static char rtsp_local_ip[64] = "127.0.0.1";
static int rtsp_video_enabled = 1;
static char rtsp_auth_basic[256];
static volatile int running = 0;
static unsigned long video_pipe_packets = 0;
static unsigned long video_broadcast_packets = 0;
static unsigned long video_broadcast_sent = 0;
static rtsp_stream_client_callback_t client_callback = NULL;
static void *client_callback_user_data = NULL;
static int last_video_client_count = -1;
static int last_audio_client_count = -1;

static int count_video_clients_locked(void)
{
    int i;
    int count = 0;

    for (i = 0; i < MAX_RTSP_CLIENTS; i++) {
        if (clients[i].active && clients[i].playing && clients[i].has_video) {
            count++;
        }
    }
    return count;
}

static int count_audio_clients_locked(void)
{
    int i;
    int count = 0;

    for (i = 0; i < MAX_RTSP_CLIENTS; i++) {
        if (clients[i].active && clients[i].playing && clients[i].has_audio) {
            count++;
        }
    }
    return count;
}

static void notify_client_count(void)
{
    rtsp_stream_client_callback_t callback;
    void *user_data;
    int video_count;
    int audio_count;

    pthread_mutex_lock(&clients_mutex);
    video_count = count_video_clients_locked();
    audio_count = count_audio_clients_locked();
    if (video_count == last_video_client_count &&
        audio_count == last_audio_client_count) {
        pthread_mutex_unlock(&clients_mutex);
        return;
    }
    last_video_client_count = video_count;
    last_audio_client_count = audio_count;
    callback = client_callback;
    user_data = client_callback_user_data;
    pthread_mutex_unlock(&clients_mutex);

    if (callback) {
        callback(video_count, audio_count, user_data);
    }
}

/* Mark a client inactive and wake its worker via shutdown(), but do NOT close()
 * the fd here. The owning worker thread closes its own fd on exit (see
 * client_thread_func), so the fd number cannot be recycled by accept() while
 * that thread might still recv()/send() on it. Idempotent. */
static void close_client_locked(int idx)
{
    if (idx < 0 || idx >= MAX_RTSP_CLIENTS) {
        return;
    }
    if (clients[idx].fd >= 0) {
        shutdown(clients[idx].fd, SHUT_RDWR);
    }
    clients[idx].active = 0;
    clients[idx].playing = 0;
    clients[idx].has_video = 0;
    clients[idx].has_audio = 0;
}

static ssize_t send_locked(rtsp_client_t *client, const void *buf, size_t len)
{
    const unsigned char *p = (const unsigned char *)buf;
    size_t off = 0;

    if (!client || client->fd < 0 || !buf) {
        return -1;
    }

    pthread_mutex_lock(&client->send_mutex);
    while (off < len) {
        ssize_t wr = send(client->fd, p + off, len - off, MSG_NOSIGNAL);
        if (wr > 0) {
            off += (size_t)wr;
            continue;
        }
        if (wr < 0 && errno == EINTR) {
            continue;
        }
        pthread_mutex_unlock(&client->send_mutex);
        return -1;
    }
    pthread_mutex_unlock(&client->send_mutex);
    return (ssize_t)off;
}

static int send_interleaved(rtsp_client_t *client, int channel,
                            const unsigned char *rtp_packet, size_t len)
{
    unsigned char header[4];
    size_t off;

    if (!client || client->fd < 0 || !rtp_packet || len == 0 || len > 0xffff) {
        return -1;
    }

    header[0] = '$';
    header[1] = (unsigned char)(channel & 0xff);
    header[2] = (unsigned char)((len >> 8) & 0xff);
    header[3] = (unsigned char)(len & 0xff);

    pthread_mutex_lock(&client->send_mutex);

    off = 0;
    while (off < sizeof(header)) {
        ssize_t wr = send(client->fd, header + off, sizeof(header) - off, MSG_NOSIGNAL);
        if (wr > 0) {
            off += (size_t)wr;
            continue;
        }
        if (wr < 0 && errno == EINTR) {
            continue;
        }
        goto fail;
    }

    off = 0;
    while (off < len) {
        ssize_t wr = send(client->fd, rtp_packet + off, len - off, MSG_NOSIGNAL);
        if (wr > 0) {
            off += (size_t)wr;
            continue;
        }
        if (wr < 0 && errno == EINTR) {
            continue;
        }
        goto fail;
    }

    pthread_mutex_unlock(&client->send_mutex);
    return 0;

fail:
    pthread_mutex_unlock(&client->send_mutex);
    return -1;
}

static const char *find_header(const char *req, const char *name)
{
    size_t name_len = strlen(name);
    const char *p = req;

    while (p && *p) {
        const char *line_end = strstr(p, "\r\n");
        size_t line_len = line_end ? (size_t)(line_end - p) : strlen(p);
        if (line_len > name_len && strncasecmp(p, name, name_len) == 0 &&
            p[name_len] == ':') {
            p += name_len + 1;
            while (*p == ' ' || *p == '\t') {
                p++;
            }
            return p;
        }
        if (!line_end) {
            break;
        }
        p = line_end + 2;
    }
    return NULL;
}

static const char *find_case_substr(const char *haystack, const char *needle)
{
    size_t needle_len;

    if (!haystack || !needle) {
        return NULL;
    }
    needle_len = strlen(needle);
    if (needle_len == 0) {
        return haystack;
    }
    while (*haystack) {
        if (strncasecmp(haystack, needle, needle_len) == 0) {
            return haystack;
        }
        haystack++;
    }
    return NULL;
}

static char base64_char(unsigned int v)
{
    static const char table[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    return table[v & 0x3f];
}

static int base64_encode_small(const unsigned char *data, size_t len,
                               char *out, size_t out_size)
{
    size_t encoded_len = ((len + 2) / 3) * 4;
    size_t i;
    size_t pos = 0;

    if (!data || !out || out_size == 0 || encoded_len + 1 > out_size) {
        return -1;
    }

    for (i = 0; i < len; i += 3) {
        unsigned int b0 = data[i];
        unsigned int b1 = (i + 1 < len) ? data[i + 1] : 0;
        unsigned int b2 = (i + 2 < len) ? data[i + 2] : 0;

        out[pos++] = base64_char(b0 >> 2);
        out[pos++] = base64_char(((b0 & 0x03) << 4) | (b1 >> 4));
        out[pos++] = (i + 1 < len) ? base64_char(((b1 & 0x0f) << 2) | (b2 >> 6)) : '=';
        out[pos++] = (i + 2 < len) ? base64_char(b2 & 0x3f) : '=';
    }
    out[pos] = '\0';
    return 0;
}

static void configure_auth(const char *user, const char *pass)
{
    char raw[144];
    char encoded[192];

    rtsp_auth_basic[0] = '\0';
    if ((!user || !user[0]) && (!pass || !pass[0])) {
        return;
    }

    snprintf(raw, sizeof(raw), "%s:%s", user ? user : "", pass ? pass : "");
    if (base64_encode_small((const unsigned char *)raw, strlen(raw),
                            encoded, sizeof(encoded)) == 0) {
        snprintf(rtsp_auth_basic, sizeof(rtsp_auth_basic), "Basic %s", encoded);
    }
}

static int header_value_equals(const char *value, const char *expected)
{
    size_t len;

    if (!value || !expected) {
        return 0;
    }
    len = strcspn(value, "\r\n");
    while (len > 0 && (value[len - 1] == ' ' || value[len - 1] == '\t')) {
        len--;
    }
    return strlen(expected) == len && strncmp(value, expected, len) == 0;
}

static void set_fd_nonblocking(int fd)
{
    int flags;

    if (fd < 0) {
        return;
    }
    flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }
}

static int request_authorized(const char *req)
{
    const char *auth;

    if (!rtsp_auth_basic[0]) {
        return 1;
    }
    auth = find_header(req, "Authorization");
    return header_value_equals(auth, rtsp_auth_basic);
}

static int parse_cseq(const char *req)
{
    const char *p = find_header(req, "CSeq");
    return p ? atoi(p) : 1;
}

static int parse_interleaved_channel(const char *req, int fallback)
{
    const char *p = find_header(req, "Transport");
    const char *i;

    if (!p) {
        return fallback;
    }
    i = find_case_substr(p, "interleaved=");
    if (!i) {
        return fallback;
    }
    return atoi(i + 12);
}

static void rtsp_send_simple(rtsp_client_t *client, int cseq, int code,
                             const char *reason, const char *extra_headers,
                             const char *body, const char *content_type)
{
    char response[4096];
    size_t body_len = body ? strlen(body) : 0;
    int n;

    n = snprintf(response, sizeof(response),
                 "RTSP/1.0 %d %s\r\n"
                 "CSeq: %d\r\n"
                 "%s"
                 "%s%s%s"
                 "Content-Length: %zu\r\n"
                 "\r\n",
                 code, reason, cseq,
                 extra_headers ? extra_headers : "",
                 body_len > 0 ? "Content-Type: " : "",
                 body_len > 0 ? (content_type ? content_type : "application/sdp") : "",
                 body_len > 0 ? "\r\n" : "",
                 body_len);

    if (n < 0 || (size_t)n >= sizeof(response)) {
        return;
    }
    send_locked(client, response, (size_t)n);
    if (body_len > 0) {
        send_locked(client, body, body_len);
    }
}

static void rtsp_send_unauthorized(rtsp_client_t *client, int cseq)
{
    rtsp_send_simple(client, cseq, 401, "Unauthorized",
                     "WWW-Authenticate: Basic realm=\"WiBox RTSP\"\r\n",
                     NULL, NULL);
}

static void handle_options(rtsp_client_t *client, int cseq)
{
    rtsp_send_simple(client, cseq, 200, "OK",
                     "Public: OPTIONS, DESCRIBE, SETUP, PLAY, PAUSE, GET_PARAMETER, TEARDOWN\r\n",
                     NULL, NULL);
}

static void handle_describe(rtsp_client_t *client, int cseq)
{
    char extra[512];
    char video_sdp[512] = "";
    char sdp[1536];

    snprintf(extra, sizeof(extra),
             "Content-Base: rtsp://%s:%d/live/\r\n",
             rtsp_local_ip, rtsp_port);

    if (rtsp_video_enabled) {
        snprintf(video_sdp, sizeof(video_sdp),
                 "m=video 0 RTP/AVP 96\r\n"
                 "a=rtpmap:96 H264/90000\r\n"
                 "a=fmtp:96 packetization-mode=1;profile-level-id=42e01e\r\n"
                 "a=control:trackID=0\r\n");
    }

    snprintf(sdp, sizeof(sdp),
             "v=0\r\n"
             "o=- 0 0 IN IP4 %s\r\n"
             "s=WiBox Media\r\n"
             "c=IN IP4 0.0.0.0\r\n"
             "t=0 0\r\n"
             "a=control:*\r\n"
             "%s"
             "m=audio 0 RTP/AVP 8\r\n"
             "a=rtpmap:8 PCMA/8000\r\n"
             "a=control:trackID=1\r\n",
             rtsp_local_ip, video_sdp);

    rtsp_send_simple(client, cseq, 200, "OK", extra, sdp, "application/sdp");
}

static void handle_setup(rtsp_client_t *client, int cseq, const char *req,
                         const char *uri)
{
    int is_audio = strstr(uri, "trackID=1") != NULL;
    int is_video = strstr(uri, "trackID=0") != NULL || !is_audio;
    int rtp_channel = parse_interleaved_channel(req, is_audio ? 2 : 0);
    char extra[512];

    if (is_video && !rtsp_video_enabled) {
        rtsp_send_simple(client, cseq, 404, "Not Found", NULL, NULL, NULL);
        return;
    }

    if (is_audio) {
        client->has_audio = 1;
        client->audio_channel = rtp_channel;
    } else if (is_video) {
        client->has_video = 1;
        client->video_channel = rtp_channel;
    }

    printf("rtsp: SETUP session=%08x uri=%s media=%s channel=%d\n",
           client->session_id, uri, is_audio ? "audio" : "video", rtp_channel);

    snprintf(extra, sizeof(extra),
             "Session: %08x\r\n"
             "Transport: RTP/AVP/TCP;unicast;interleaved=%d-%d\r\n",
             client->session_id, rtp_channel, rtp_channel + 1);
    rtsp_send_simple(client, cseq, 200, "OK", extra, NULL, NULL);
}

static void handle_play(rtsp_client_t *client, int cseq)
{
    char extra[256];

    client->playing = 1;
    printf("rtsp: PLAY session=%08x video=%d audio=%d vchan=%d achan=%d\n",
           client->session_id, client->has_video, client->has_audio,
           client->video_channel, client->audio_channel);
    snprintf(extra, sizeof(extra),
             "Session: %08x\r\n"
             "Range: npt=0.000-\r\n",
             client->session_id);
    rtsp_send_simple(client, cseq, 200, "OK", extra, NULL, NULL);
    notify_client_count();
}

static void handle_pause(rtsp_client_t *client, int cseq)
{
    char extra[64];

    client->playing = 0;
    snprintf(extra, sizeof(extra), "Session: %08x\r\n", client->session_id);
    rtsp_send_simple(client, cseq, 200, "OK", extra, NULL, NULL);
    notify_client_count();
}

static const char *find_rtsp_header_end(const char *buf, size_t len)
{
    size_t i;

    if (!buf || len < 4) {
        return NULL;
    }
    for (i = 0; i + 3 < len; i++) {
        if (buf[i] == '\r' && buf[i + 1] == '\n' &&
            buf[i + 2] == '\r' && buf[i + 3] == '\n') {
            return buf + i + 4;
        }
    }
    return NULL;
}

static void consume_rx_bytes(char *rxbuf, size_t *rxlen, size_t consumed)
{
    if (!rxbuf || !rxlen || consumed == 0) {
        return;
    }
    if (consumed >= *rxlen) {
        *rxlen = 0;
        return;
    }
    memmove(rxbuf, rxbuf + consumed, *rxlen - consumed);
    *rxlen -= consumed;
}

static int read_rtsp_request(rtsp_client_t *client, int fd, char *buf, size_t size,
                             char *rxbuf, size_t *rxlen)
{
    int idle_timeouts = 0;

    if (!buf || size == 0 || !rxbuf || !rxlen) {
        return -1;
    }

    while (running) {
        const char *header_end;

        while (*rxlen > 0 && (rxbuf[0] == '\r' || rxbuf[0] == '\n')) {
            consume_rx_bytes(rxbuf, rxlen, 1);
        }

        if (*rxlen >= 4 && rxbuf[0] == '$') {
            size_t payload_len = ((size_t)(unsigned char)rxbuf[2] << 8) |
                                 (size_t)(unsigned char)rxbuf[3];
            size_t frame_len = 4U + payload_len;
            if (*rxlen >= frame_len) {
                consume_rx_bytes(rxbuf, rxlen, frame_len);
                continue;
            }
        }

        header_end = find_rtsp_header_end(rxbuf, *rxlen);
        if (header_end) {
            size_t req_len = (size_t)(header_end - rxbuf);
            if (req_len + 1 > size) {
                return -1;
            }
            memcpy(buf, rxbuf, req_len);
            buf[req_len] = '\0';
            consume_rx_bytes(rxbuf, rxlen, req_len);
            return (int)req_len;
        }

        if (*rxlen >= RTSP_RX_BUFFER_MAX) {
            return -1;
        }

        {
            ssize_t rd = recv(fd, rxbuf + *rxlen,
                              RTSP_RX_BUFFER_MAX - *rxlen, 0);
            if (rd > 0) {
                *rxlen += (size_t)rd;
                idle_timeouts = 0;
                continue;
            }
            if (rd < 0 && errno == EINTR) {
                continue;
            }
            if (rd < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                /* SO_RCVTIMEO fired. A client that reached PLAY only receives
                 * interleaved RTP and legitimately sends nothing, so keep it.
                 * A pre-PLAY client that stays silent is dropped after a bounded
                 * number of timeouts so it cannot hold a slot indefinitely. */
                if (client && client->playing) {
                    continue;
                }
                if (++idle_timeouts >= RTSP_HANDSHAKE_MAX_TIMEOUTS) {
                    return -1;
                }
                continue;
            }
            return -1;
        }
    }
    return -1;
}

static void *client_thread_func(void *arg)
{
    int idx = (int)(intptr_t)arg;
    rtsp_client_t *client = &clients[idx];
    char req[RTSP_REQUEST_MAX];
    char rxbuf[RTSP_RX_BUFFER_MAX];
    size_t rxlen = 0;

    printf("rtsp: client connected slot=%d\n", idx);

    while (running && client->active) {
        char method[32] = {0};
        char uri[512] = {0};
        int cseq;

        if (read_rtsp_request(client, client->fd, req, sizeof(req), rxbuf, &rxlen) <= 0) {
            break;
        }

        if (sscanf(req, "%31s %511s", method, uri) != 2) {
            printf("rtsp: invalid request from slot=%d\n", idx);
            break;
        }
        cseq = parse_cseq(req);

        if (strcasecmp(method, "OPTIONS") != 0 && !request_authorized(req)) {
            printf("rtsp: unauthorized %s session=%08x\n", method, client->session_id);
            rtsp_send_unauthorized(client, cseq);
            continue;
        }

        if (strcasecmp(method, "OPTIONS") == 0) {
            printf("rtsp: OPTIONS session=%08x\n", client->session_id);
            handle_options(client, cseq);
        } else if (strcasecmp(method, "DESCRIBE") == 0) {
            printf("rtsp: DESCRIBE session=%08x uri=%s\n", client->session_id, uri);
            handle_describe(client, cseq);
        } else if (strcasecmp(method, "SETUP") == 0) {
            handle_setup(client, cseq, req, uri);
        } else if (strcasecmp(method, "PLAY") == 0) {
            handle_play(client, cseq);
        } else if (strcasecmp(method, "PAUSE") == 0) {
            printf("rtsp: PAUSE session=%08x\n", client->session_id);
            handle_pause(client, cseq);
        } else if (strcasecmp(method, "GET_PARAMETER") == 0) {
            rtsp_send_simple(client, cseq, 200, "OK", NULL, NULL, NULL);
        } else if (strcasecmp(method, "TEARDOWN") == 0) {
            printf("rtsp: TEARDOWN session=%08x\n", client->session_id);
            rtsp_send_simple(client, cseq, 200, "OK", NULL, NULL, NULL);
            break;
        } else {
            rtsp_send_simple(client, cseq, 405, "Method Not Allowed", NULL, NULL, NULL);
        }
    }

    pthread_mutex_lock(&clients_mutex);
    close_client_locked(idx);
    /* This worker is the sole closer of its fd. send_mutex guards against any
     * in-flight send() on the same fd before it is closed. */
    if (clients[idx].fd >= 0) {
        pthread_mutex_lock(&clients[idx].send_mutex);
        close(clients[idx].fd);
        clients[idx].fd = -1;
        pthread_mutex_unlock(&clients[idx].send_mutex);
    }
    clients[idx].thread_running = 0;
    pthread_mutex_unlock(&clients_mutex);
    notify_client_count();
    printf("rtsp: client disconnected slot=%d\n", idx);
    return NULL;
}

static int allocate_client(int fd)
{
    int i;

    pthread_mutex_lock(&clients_mutex);
    for (i = 0; i < MAX_RTSP_CLIENTS; i++) {
        /* thread_running guards against reusing a slot whose previous worker has
         * been marked inactive but has not exited yet (and may still touch fd). */
        if (!clients[i].active && !clients[i].thread_running) {
            clients[i].fd = fd;
            clients[i].active = 1;
            clients[i].playing = 0;
            clients[i].has_video = 0;
            clients[i].has_audio = 0;
            clients[i].video_channel = 0;
            clients[i].audio_channel = 2;
            clients[i].thread_running = 1;
            clients[i].session_id = (unsigned int)time(NULL) ^ (unsigned int)getpid() ^ (unsigned int)i;
            pthread_mutex_unlock(&clients_mutex);
            return i;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    return -1;
}

static void *listener_thread_func(void *arg)
{
    struct sockaddr_in addr;
    int yes = 1;
    (void)arg;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        printf("rtsp: socket failed errno=%d\n", errno);
        running = 0;
        return NULL;
    }

    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((uint16_t)rtsp_port);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        printf("rtsp: bind port %d failed errno=%d\n", rtsp_port, errno);
        close(server_fd);
        server_fd = -1;
        running = 0;
        return NULL;
    }
    if (listen(server_fd, 4) < 0) {
        printf("rtsp: listen failed errno=%d\n", errno);
        close(server_fd);
        server_fd = -1;
        running = 0;
        return NULL;
    }

    printf("rtsp: listening on rtsp://%s:%d/live video=%d auth=%d\n",
           rtsp_local_ip, rtsp_port, rtsp_video_enabled, rtsp_auth_basic[0] ? 1 : 0);

    while (running) {
        int fd = accept(server_fd, NULL, NULL);
        int idx;

        if (fd < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (!running) {
                break;
            }
            usleep(100000);
            continue;
        }

        {
            struct timeval rcv = { RTSP_CLIENT_RCVTIMEO_SEC, 0 };
            struct timeval snd = { RTSP_CLIENT_SNDTIMEO_SEC, 0 };
            setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &rcv, sizeof(rcv));
            setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &snd, sizeof(snd));
        }

        idx = allocate_client(fd);
        if (idx < 0) {
            close(fd);
            continue;
        }
        if (pthread_create(&clients[idx].thread, NULL, client_thread_func,
                           (void *)(intptr_t)idx) != 0) {
            /* No worker will run for this slot, so close the fd here and release
             * the slot (thread_running was set optimistically by allocate_client). */
            pthread_mutex_lock(&clients_mutex);
            close_client_locked(idx);
            if (clients[idx].fd >= 0) {
                close(clients[idx].fd);
                clients[idx].fd = -1;
            }
            clients[idx].thread_running = 0;
            pthread_mutex_unlock(&clients_mutex);
            continue;
        }
        pthread_detach(clients[idx].thread);
    }

    return NULL;
}

static int read_full(int fd, unsigned char *buf, size_t len)
{
    size_t off = 0;

    while (running && off < len) {
        ssize_t rd = read(fd, buf + off, len - off);
        if (rd > 0) {
            off += (size_t)rd;
            continue;
        }
        if (rd < 0 && errno == EINTR) {
            continue;
        }
        if (rd < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            usleep(10000);
            continue;
        }
        return -1;
    }
    return off == len ? 0 : -1;
}

static void broadcast_video_rtp(const unsigned char *rtp_packet, size_t len)
{
    int i;
    int ready = 0;
    int sent = 0;
    int closed = 0;

    pthread_mutex_lock(&clients_mutex);
    for (i = 0; i < MAX_RTSP_CLIENTS; i++) {
        if (!clients[i].active || !clients[i].playing || !clients[i].has_video) {
            continue;
        }
        ready++;
        if (send_interleaved(&clients[i], clients[i].video_channel, rtp_packet, len) < 0) {
            close_client_locked(i);
            closed = 1;
        } else {
            sent++;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    if (closed) {
        notify_client_count();
    }

    video_broadcast_packets++;
    video_broadcast_sent += (unsigned long)sent;
    if (video_broadcast_packets <= 5) {
        unsigned int pt = len > 1 ? (rtp_packet[1] & 0x7f) : 0;
        unsigned int seq = len > 3 ? (((unsigned int)rtp_packet[2] << 8) | rtp_packet[3]) : 0;
        printf("rtsp: video packet #%lu len=%u pt=%u seq=%u ready=%d sent=%d total_sent=%lu\n",
               video_broadcast_packets, (unsigned int)len, pt, seq,
               ready, sent, video_broadcast_sent);
    }
}

static void *video_reader_thread_func(void *arg)
{
    unsigned char header[2];
    unsigned char packet[RTP_PACKET_MAX];
    (void)arg;

    while (running) {
        unsigned int len;

        if (read_full(video_pipe_read_fd, header, sizeof(header)) < 0) {
            if (running) usleep(10000);
            continue;
        }
        len = ((unsigned int)header[0] << 8) | header[1];
        if (len == 0 || len > sizeof(packet)) {
            printf("rtsp: invalid video pipe packet len=%u max=%u\n",
                   len, (unsigned int)sizeof(packet));
            continue;
        }
        if (read_full(video_pipe_read_fd, packet, len) < 0) {
            continue;
        }
        video_pipe_packets++;
        if (video_pipe_packets <= 5) {
            printf("rtsp: pipe packet #%lu len=%u\n", video_pipe_packets, len);
        }
        broadcast_video_rtp(packet, len);
    }
    return NULL;
}

int rtsp_stream_start(int port, const char *local_ip, int video_enabled,
                      const char *auth_user, const char *auth_pass)
{
    int i;
    int pipefd[2];

    rtsp_video_enabled = video_enabled ? 1 : 0;
    configure_auth(auth_user, auth_pass);

    if (running) {
        notify_client_count();
        return 0;
    }
    if (port <= 0 || port > 65535) {
        port = DEFAULT_RTSP_PORT;
    }

    for (i = 0; i < MAX_RTSP_CLIENTS; i++) {
        memset(&clients[i], 0, sizeof(clients[i]));
        clients[i].fd = -1;
        pthread_mutex_init(&clients[i].send_mutex, NULL);
    }

    if (pipe(pipefd) < 0) {
        printf("rtsp: pipe failed errno=%d\n", errno);
        return -1;
    }
    video_pipe_read_fd = pipefd[0];
    video_pipe_write_fd = pipefd[1];
    set_fd_nonblocking(video_pipe_read_fd);

    if (local_ip && local_ip[0]) {
        strncpy(rtsp_local_ip, local_ip, sizeof(rtsp_local_ip) - 1);
        rtsp_local_ip[sizeof(rtsp_local_ip) - 1] = '\0';
    }
    rtsp_port = port;
    signal(SIGPIPE, SIG_IGN);
    running = 1;

    if (pthread_create(&video_reader_thread, NULL, video_reader_thread_func, NULL) != 0) {
        running = 0;
        close(video_pipe_read_fd);
        close(video_pipe_write_fd);
        video_pipe_read_fd = -1;
        video_pipe_write_fd = -1;
        return -1;
    }

    if (pthread_create(&listener_thread, NULL, listener_thread_func, NULL) != 0) {
        running = 0;
        close(video_pipe_read_fd);
        close(video_pipe_write_fd);
        video_pipe_read_fd = -1;
        video_pipe_write_fd = -1;
        pthread_join(video_reader_thread, NULL);
        return -1;
    }

    usleep(100000);
    if (!running) {
        if (server_fd >= 0) {
            close(server_fd);
            server_fd = -1;
        }
        close(video_pipe_read_fd);
        close(video_pipe_write_fd);
        video_pipe_read_fd = -1;
        video_pipe_write_fd = -1;
        pthread_join(listener_thread, NULL);
        pthread_join(video_reader_thread, NULL);
        return -1;
    }

    return 0;
}

void rtsp_stream_stop(void)
{
    int i;

    if (!running) {
        return;
    }
    running = 0;
    if (server_fd >= 0) {
        shutdown(server_fd, SHUT_RDWR);
        close(server_fd);
        server_fd = -1;
    }
    if (video_pipe_read_fd >= 0) {
        close(video_pipe_read_fd);
        video_pipe_read_fd = -1;
    }
    if (video_pipe_write_fd >= 0) {
        close(video_pipe_write_fd);
        video_pipe_write_fd = -1;
    }

    pthread_mutex_lock(&clients_mutex);
    for (i = 0; i < MAX_RTSP_CLIENTS; i++) {
        close_client_locked(i);
    }
    last_video_client_count = -1;
    last_audio_client_count = -1;
    pthread_mutex_unlock(&clients_mutex);

    /* Detached worker threads close their own fds on exit; give them a bounded
     * window to observe the shutdown so sockets are not leaked past stop. */
    {
        int w;
        for (w = 0; w < 200; w++) {
            int busy = 0;
            pthread_mutex_lock(&clients_mutex);
            for (i = 0; i < MAX_RTSP_CLIENTS; i++) {
                if (clients[i].thread_running) busy = 1;
            }
            pthread_mutex_unlock(&clients_mutex);
            if (!busy) break;
            usleep(10000);
        }
    }

    pthread_join(listener_thread, NULL);
    pthread_join(video_reader_thread, NULL);
}

void rtsp_stream_set_video_enabled(int enabled)
{
    int i;
    int changed;

    pthread_mutex_lock(&clients_mutex);
    changed = rtsp_video_enabled != (enabled ? 1 : 0);
    rtsp_video_enabled = enabled ? 1 : 0;
    if (changed) {
        for (i = 0; i < MAX_RTSP_CLIENTS; i++) {
            close_client_locked(i);
        }
        last_video_client_count = -1;
        last_audio_client_count = -1;
    }
    pthread_mutex_unlock(&clients_mutex);

    if (changed) {
        printf("rtsp: video track %s; clients will reconnect\n",
               rtsp_video_enabled ? "enabled" : "disabled");
        notify_client_count();
    }
}

int rtsp_stream_get_video_pipe_fd(void)
{
    return (running && rtsp_video_enabled) ? video_pipe_write_fd : -1;
}

int rtsp_stream_get_video_client_count(void)
{
    int count;

    pthread_mutex_lock(&clients_mutex);
    count = count_video_clients_locked();
    pthread_mutex_unlock(&clients_mutex);
    return count;
}

int rtsp_stream_get_audio_client_count(void)
{
    int count;

    pthread_mutex_lock(&clients_mutex);
    count = count_audio_clients_locked();
    pthread_mutex_unlock(&clients_mutex);
    return count;
}

void rtsp_stream_set_client_callback(rtsp_stream_client_callback_t callback,
                                     void *user_data)
{
    pthread_mutex_lock(&clients_mutex);
    client_callback = callback;
    client_callback_user_data = user_data;
    last_video_client_count = -1;
    last_audio_client_count = -1;
    pthread_mutex_unlock(&clients_mutex);
    notify_client_count();
}

void rtsp_stream_send_audio_rtp(const unsigned char *rtp_packet, size_t len)
{
    int i;
    int closed = 0;

    if (!running || !rtp_packet || len == 0 || len > 0xffff) {
        return;
    }

    pthread_mutex_lock(&clients_mutex);
    for (i = 0; i < MAX_RTSP_CLIENTS; i++) {
        if (!clients[i].active || !clients[i].playing || !clients[i].has_audio) {
            continue;
        }
        if (send_interleaved(&clients[i], clients[i].audio_channel, rtp_packet, len) < 0) {
            close_client_locked(i);
            closed = 1;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    if (closed) {
        notify_client_count();
    }
}
