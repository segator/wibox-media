#include "prometheus.h"
#include "mqtt.h"

#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define PROM_FILE "prometheus"

#ifndef WIBOX_VERSION
#define WIBOX_VERSION "dev-unknown"
#endif
#ifndef WIBOX_COMMIT
#define WIBOX_COMMIT "unknown"
#endif
#ifndef WIBOX_BUILD_TIMESTAMP
#define WIBOX_BUILD_TIMESTAMP "unknown"
#endif

typedef struct {
    pthread_t thread;
    int running;
    int listen_fd;
    int port;
    time_t start_time;

    int call_active;
    int sip_call_active;
    int video_active;
    int video_enabled;
    int ringing;

    unsigned long rings_total;
    unsigned long door_unlocks_total;
    unsigned long calls_started_total;
    unsigned long video_sessions_started_total;
    unsigned long uart_frames_total;
    unsigned long uart_unknown_frames_total;
    unsigned long uart_alarm_reports_total;
    unsigned long uart_hangups_total;
    unsigned long uart_stop_rings_total;
    unsigned long uart_resets_total;
    unsigned long uart_push_state_total;
    unsigned long uart_f1_total;
    time_t last_ring;
    time_t last_unlock;
} prometheus_state_t;

static prometheus_state_t prom_state = {
    .listen_fd = -1
};
static pthread_mutex_t prom_mutex = PTHREAD_MUTEX_INITIALIZER;

static void append_label_escaped(char* out, size_t out_size, const char* value) {
    size_t pos = strlen(out);
    size_t i;

    if (pos >= out_size) {
        return;
    }

    for (i = 0; value && value[i] && pos + 2 < out_size; i++) {
        char c = value[i];
        if (c == '\\' || c == '"') {
            out[pos++] = '\\';
            out[pos++] = c;
        } else if (c == '\n' || c == '\r') {
            out[pos++] = ' ';
        } else {
            out[pos++] = c;
        }
    }
    out[pos] = '\0';
}

static int get_wifi_rssi(void) {
    FILE* fp;
    char line[128];
    int rssi = -999;

    fp = popen("/usr/sbin/wpa_cli -i wlan0 signal_poll 2>/dev/null || /sbin/wpa_cli -i wlan0 signal_poll 2>/dev/null || wpa_cli -i wlan0 signal_poll 2>/dev/null", "r");
    if (!fp) {
        return rssi;
    }

    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "RSSI=", 5) == 0) {
            rssi = atoi(line + 5);
            break;
        }
    }
    pclose(fp);
    return rssi;
}

static void build_metrics(char* body, size_t body_size) {
    prometheus_state_t snapshot;
    time_t now = time(NULL);
    int wifi_rssi = get_wifi_rssi();

    pthread_mutex_lock(&prom_mutex);
    snapshot = prom_state;
    pthread_mutex_unlock(&prom_mutex);

    body[0] = '\0';

    strncat(body, "# HELP wibox_info Build metadata for the running daemon.\n", body_size - strlen(body) - 1);
    strncat(body, "# TYPE wibox_info gauge\n", body_size - strlen(body) - 1);
    strncat(body, "wibox_info{version=\"", body_size - strlen(body) - 1);
    append_label_escaped(body, body_size, WIBOX_VERSION);
    strncat(body, "\",commit=\"", body_size - strlen(body) - 1);
    append_label_escaped(body, body_size, WIBOX_COMMIT);
    strncat(body, "\",build_timestamp=\"", body_size - strlen(body) - 1);
    append_label_escaped(body, body_size, WIBOX_BUILD_TIMESTAMP);
    strncat(body, "\"} 1\n", body_size - strlen(body) - 1);

    snprintf(body + strlen(body), body_size - strlen(body),
             "# HELP wibox_uptime_seconds Daemon uptime in seconds.\n"
             "# TYPE wibox_uptime_seconds counter\n"
             "wibox_uptime_seconds %ld\n"
             "# HELP wibox_health Exporter health, 1 when the daemon can serve metrics.\n"
             "# TYPE wibox_health gauge\n"
             "wibox_health 1\n"
             "# HELP wibox_mqtt_connected MQTT broker connection state.\n"
             "# TYPE wibox_mqtt_connected gauge\n"
             "wibox_mqtt_connected %d\n"
             "# HELP wibox_call_active Intercom media call path active.\n"
             "# TYPE wibox_call_active gauge\n"
             "wibox_call_active %d\n"
             "# HELP wibox_sip_call_active SIP call active.\n"
             "# TYPE wibox_sip_call_active gauge\n"
             "wibox_sip_call_active %d\n"
             "# HELP wibox_video_active H.264 RTP video worker active.\n"
             "# TYPE wibox_video_active gauge\n"
             "wibox_video_active %d\n"
             "# HELP wibox_video_enabled Video enabled by runtime configuration.\n"
             "# TYPE wibox_video_enabled gauge\n"
             "wibox_video_enabled %d\n"
             "# HELP wibox_ringing Panel ringing state.\n"
             "# TYPE wibox_ringing gauge\n"
             "wibox_ringing %d\n",
             (long)(now - snapshot.start_time),
             mqtt_is_connected(),
             snapshot.call_active,
             snapshot.sip_call_active,
             snapshot.video_active,
             snapshot.video_enabled,
             snapshot.ringing);

    snprintf(body + strlen(body), body_size - strlen(body),
             "# HELP wibox_rings_total Total ring events detected by the daemon.\n"
             "# TYPE wibox_rings_total counter\n"
             "wibox_rings_total %lu\n"
             "# HELP wibox_calls_started_total Total SIP calls that reached established state.\n"
             "# TYPE wibox_calls_started_total counter\n"
             "wibox_calls_started_total %lu\n"
             "# HELP wibox_video_sessions_started_total Total video RTP sessions started.\n"
             "# TYPE wibox_video_sessions_started_total counter\n"
             "wibox_video_sessions_started_total %lu\n"
             "# HELP wibox_door_unlocks_total Total successful door unlock commands.\n"
             "# TYPE wibox_door_unlocks_total counter\n"
             "wibox_door_unlocks_total %lu\n"
             "# HELP wibox_last_ring_timestamp_seconds Last ring event Unix timestamp, 0 if unknown.\n"
             "# TYPE wibox_last_ring_timestamp_seconds gauge\n"
             "wibox_last_ring_timestamp_seconds %ld\n"
             "# HELP wibox_last_unlock_timestamp_seconds Last successful unlock Unix timestamp, 0 if unknown.\n"
             "# TYPE wibox_last_unlock_timestamp_seconds gauge\n"
             "wibox_last_unlock_timestamp_seconds %ld\n"
             "# HELP wibox_wifi_rssi_dbm WiFi RSSI from wpa_cli signal_poll, -999 if unavailable.\n"
             "# TYPE wibox_wifi_rssi_dbm gauge\n"
             "wibox_wifi_rssi_dbm %d\n",
             snapshot.rings_total,
             snapshot.calls_started_total,
             snapshot.video_sessions_started_total,
             snapshot.door_unlocks_total,
             (long)snapshot.last_ring,
             (long)snapshot.last_unlock,
             wifi_rssi);

    snprintf(body + strlen(body), body_size - strlen(body),
             "# HELP wibox_uart_frames_total Total recognized UART frames received from the Fermax panel.\n"
             "# TYPE wibox_uart_frames_total counter\n"
             "wibox_uart_frames_total %lu\n"
             "# HELP wibox_uart_unknown_frames_total Total unknown UART frames received from the Fermax panel.\n"
             "# TYPE wibox_uart_unknown_frames_total counter\n"
             "wibox_uart_unknown_frames_total %lu\n"
             "# HELP wibox_uart_alarm_reports_total Total panel alarm/ring UART reports.\n"
             "# TYPE wibox_uart_alarm_reports_total counter\n"
             "wibox_uart_alarm_reports_total %lu\n"
             "# HELP wibox_uart_hangups_total Total panel hangup UART reports.\n"
             "# TYPE wibox_uart_hangups_total counter\n"
             "wibox_uart_hangups_total %lu\n"
             "# HELP wibox_uart_stop_rings_total Total physical-intercom stop-ring UART reports.\n"
             "# TYPE wibox_uart_stop_rings_total counter\n"
             "wibox_uart_stop_rings_total %lu\n"
             "# HELP wibox_uart_resets_total Total panel reset UART reports.\n"
             "# TYPE wibox_uart_resets_total counter\n"
             "wibox_uart_resets_total %lu\n"
             "# HELP wibox_uart_push_state_total Total call-forward state UART reports.\n"
             "# TYPE wibox_uart_push_state_total counter\n"
             "wibox_uart_push_state_total %lu\n"
             "# HELP wibox_uart_f1_total Total F1 function commands sent by the daemon.\n"
             "# TYPE wibox_uart_f1_total counter\n"
             "wibox_uart_f1_total %lu\n",
             snapshot.uart_frames_total,
             snapshot.uart_unknown_frames_total,
             snapshot.uart_alarm_reports_total,
             snapshot.uart_hangups_total,
             snapshot.uart_stop_rings_total,
             snapshot.uart_resets_total,
             snapshot.uart_push_state_total,
             snapshot.uart_f1_total);
}

static void send_response(int fd, int status, const char* status_text,
                          const char* content_type, const char* body) {
    char header[256];
    size_t body_len = strlen(body);

    snprintf(header, sizeof(header),
             "HTTP/1.1 %d %s\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %lu\r\n"
             "Connection: close\r\n"
             "\r\n",
             status, status_text, content_type, (unsigned long)body_len);
    send(fd, header, strlen(header), MSG_NOSIGNAL);
    send(fd, body, body_len, MSG_NOSIGNAL);
}

static void handle_client(int fd) {
    char request[256];
    char body[8192];
    ssize_t n;

    n = recv(fd, request, sizeof(request) - 1, 0);
    if (n <= 0) {
        return;
    }
    request[n] = '\0';

    if (strncmp(request, "GET /metrics ", 13) == 0 ||
        strncmp(request, "GET /metrics?", 13) == 0) {
        build_metrics(body, sizeof(body));
        send_response(fd, 200, "OK", "text/plain; version=0.0.4; charset=utf-8", body);
    } else if (strncmp(request, "GET /healthz ", 13) == 0) {
        send_response(fd, 200, "OK", "text/plain; charset=utf-8", "ok\n");
    } else {
        send_response(fd, 404, "Not Found", "text/plain; charset=utf-8", "not found\n");
    }
}

static void* prometheus_thread(void* arg) {
    (void)arg;

    while (prom_state.running) {
        fd_set rfds;
        struct timeval tv;
        int ready;

        FD_ZERO(&rfds);
        FD_SET(prom_state.listen_fd, &rfds);
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        ready = select(prom_state.listen_fd + 1, &rfds, NULL, NULL, &tv);
        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        if (ready > 0 && FD_ISSET(prom_state.listen_fd, &rfds)) {
            int client = accept(prom_state.listen_fd, NULL, NULL);
            if (client >= 0) {
                handle_client(client);
                close(client);
            }
        }
    }

    return NULL;
}

int prometheus_start(int port) {
    struct sockaddr_in addr;
    int opt = 1;

    if (port <= 0 || port > 65535) {
        printf("%s: invalid port %d\n", PROM_FILE, port);
        return -1;
    }
    if (prom_state.running) {
        return 0;
    }

    memset(&prom_state, 0, sizeof(prom_state));
    prom_state.listen_fd = -1;
    prom_state.port = port;
    prom_state.start_time = time(NULL);

    prom_state.listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (prom_state.listen_fd < 0) {
        printf("%s: socket failed: %s\n", PROM_FILE, strerror(errno));
        return -1;
    }
    setsockopt(prom_state.listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((unsigned short)port);

    if (bind(prom_state.listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        printf("%s: bind port %d failed: %s\n", PROM_FILE, port, strerror(errno));
        close(prom_state.listen_fd);
        prom_state.listen_fd = -1;
        return -1;
    }
    if (listen(prom_state.listen_fd, 4) < 0) {
        printf("%s: listen failed: %s\n", PROM_FILE, strerror(errno));
        close(prom_state.listen_fd);
        prom_state.listen_fd = -1;
        return -1;
    }

    prom_state.running = 1;
    if (pthread_create(&prom_state.thread, NULL, prometheus_thread, NULL) != 0) {
        printf("%s: failed to create thread: %s\n", PROM_FILE, strerror(errno));
        prom_state.running = 0;
        close(prom_state.listen_fd);
        prom_state.listen_fd = -1;
        return -1;
    }

    printf("%s: exporter listening on 0.0.0.0:%d\n", PROM_FILE, port);
    return 0;
}

void prometheus_stop(void) {
    if (!prom_state.running) {
        return;
    }

    prom_state.running = 0;
    if (prom_state.listen_fd >= 0) {
        shutdown(prom_state.listen_fd, SHUT_RDWR);
    }
    pthread_join(prom_state.thread, NULL);
    if (prom_state.listen_fd >= 0) {
        close(prom_state.listen_fd);
        prom_state.listen_fd = -1;
    }
}

static void set_int_metric(int* field, int value) {
    pthread_mutex_lock(&prom_mutex);
    *field = value ? 1 : 0;
    pthread_mutex_unlock(&prom_mutex);
}

void prometheus_set_call_active(int active) {
    set_int_metric(&prom_state.call_active, active);
}

void prometheus_set_sip_call_active(int active) {
    set_int_metric(&prom_state.sip_call_active, active);
}

void prometheus_set_video_active(int active) {
    set_int_metric(&prom_state.video_active, active);
}

void prometheus_set_video_enabled(int enabled) {
    set_int_metric(&prom_state.video_enabled, enabled);
}

void prometheus_set_ringing(int active) {
    set_int_metric(&prom_state.ringing, active);
}

void prometheus_inc_ring(void) {
    pthread_mutex_lock(&prom_mutex);
    prom_state.rings_total++;
    prom_state.last_ring = time(NULL);
    pthread_mutex_unlock(&prom_mutex);
}

void prometheus_inc_door_unlock(void) {
    pthread_mutex_lock(&prom_mutex);
    prom_state.door_unlocks_total++;
    prom_state.last_unlock = time(NULL);
    pthread_mutex_unlock(&prom_mutex);
}

void prometheus_inc_call_started(void) {
    pthread_mutex_lock(&prom_mutex);
    prom_state.calls_started_total++;
    pthread_mutex_unlock(&prom_mutex);
}

void prometheus_inc_video_started(void) {
    pthread_mutex_lock(&prom_mutex);
    prom_state.video_sessions_started_total++;
    pthread_mutex_unlock(&prom_mutex);
}

static void inc_ulong_metric(unsigned long* field) {
    pthread_mutex_lock(&prom_mutex);
    (*field)++;
    pthread_mutex_unlock(&prom_mutex);
}

void prometheus_inc_uart_frame(void) {
    inc_ulong_metric(&prom_state.uart_frames_total);
}

void prometheus_inc_uart_unknown_frame(void) {
    inc_ulong_metric(&prom_state.uart_unknown_frames_total);
}

void prometheus_inc_uart_alarm_report(void) {
    inc_ulong_metric(&prom_state.uart_alarm_reports_total);
}

void prometheus_inc_uart_hangup(void) {
    inc_ulong_metric(&prom_state.uart_hangups_total);
}

void prometheus_inc_uart_stop_ring(void) {
    inc_ulong_metric(&prom_state.uart_stop_rings_total);
}

void prometheus_inc_uart_reset(void) {
    inc_ulong_metric(&prom_state.uart_resets_total);
}

void prometheus_inc_uart_push_state(void) {
    inc_ulong_metric(&prom_state.uart_push_state_total);
}

void prometheus_inc_uart_f1(void) {
    inc_ulong_metric(&prom_state.uart_f1_total);
}
