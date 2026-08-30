#include <pjsip.h>           // Core SIP protocol handling
#include <pjlib-util.h>      // PJSIP utilities
#include <pjlib.h>           // PJSIP base library
#include <stdio.h>           // Standard I/O
#include <stdlib.h>          // Standard library
#include <unistd.h>          // UNIX standard functions
#include <signal.h>          // Signal handling
#include <string.h>          // String functions
#include <sys/socket.h>      // Socket functions
#include <netinet/in.h>      // Internet address family
#include <arpa/inet.h>       // Internet operations
#include <termios.h>         // Serial raw mode
#include <sys/wait.h>        // Child process cleanup
#include <sys/reboot.h>      // System reboot
#include <fcntl.h>           // File control
#include <pthread.h>         // POSIX threads
#include <ifaddrs.h>         // For getifaddrs()
#include <errno.h>           // Error numbers
#include <unistd.h>          // For access()
#include <sys/stat.h>        // For mkfifo()
#include <sys/time.h>        // For gettimeofday()
#include <ctype.h>           // Character classification

#include "sip_calling.h"     // Our unified SIP calling module
#include "intercom.h"        // Our communication with the intercom
#include "config.h"          // Configuration management
#include "mqtt.h"            // MQTT/Home Assistant integration
#include "prometheus.h"      // Prometheus metrics exporter
#include "audio_hw.h"        // Direct GADI audio hardware access
#include "video_worker.h"    // In-daemon D1 H.264 RTP worker
#include "rtsp_stream.h"     // Optional RTSP camera stream
#include "hardware_watchdog.h" // Main-loop-aware hardware watchdog
#include "call_session.h"    // Correlated call lifecycle telemetry
#include "runtime_config.h"  // Runtime value validation and normalization
#include "uart_protocol.h"   // Fermax UART frame definitions and parsing

#define THIS_FILE "wibox-media-daemon"
#define CONFIG_FILE "/mnt/mtd/sip_media.conf"
#define LEGACY_CONFIG_FILE "/mnt/mtd/sip.conf"
#define SNAPSHOT_PATH "/tmp/wibox-snapshot.jpg"
#define SNAPSHOT_LOG_PATH "/tmp/wibox-snapshot-worker.log"
#define AUDIO_LINE_MUTE_MAX_MS 3000
#ifndef WIFI_AP_REQUEST_PATH
#define WIFI_AP_REQUEST_PATH "/mnt/mtd/wifi_ap_requested"
#endif

#define RTP_PAYLOAD_DTMF 101    // Common DTMF payload type
#define DTMF_EVENT_0     0
#define DTMF_EVENT_1     1
#define DTMF_EVENT_2     2
#define DTMF_EVENT_3     3
#define DTMF_EVENT_4     4
#define DTMF_EVENT_5     5
#define DTMF_EVENT_6     6
#define DTMF_EVENT_7     7
#define DTMF_EVENT_8     8
#define DTMF_EVENT_9     9
#define DTMF_EVENT_STAR  10
#define DTMF_EVENT_HASH  11

// Global variables
static pjsip_endpoint *sip_endpt;     // Main SIP endpoint
static pj_pool_t *pool;               // Memory pool for PJSIP
static pj_bool_t quit_flag = PJ_FALSE; // Application shutdown flag
static int rtp_socket = -1;           // UDP socket for audio (RTP)
static pj_thread_t *audio_input_thread;   // Thread handle for AI -> RTP
static pj_thread_t *audio_output_thread;  // Thread handle for RTP -> AO
static pthread_mutex_t audio_lifecycle_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t audio_state_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t audio_mute_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t video_lifecycle_mutex = PTHREAD_MUTEX_INITIALIZER;
static int audio_engine_running = 0;
static int audio_sip_rtp_active = 0;
static unsigned long long audio_input_mute_until_ms = 0;
static pid_t video_bridge_pid = -1;
static int video_bridge_has_sip_rtp = 0;
static int video_bridge_control_fd = -1;
static pj_bool_t call_active = PJ_FALSE; // Is there an active audio session?
static pj_mutex_t *call_active_mutex; // Mutex to protect call_active
static struct sockaddr_in remote_rtp_addr; // Where to send audio packets
static int current_dtmf_payload_type = RTP_PAYLOAD_DTMF;
static pthread_mutex_t snapshot_mutex = PTHREAD_MUTEX_INITIALIZER;
static int snapshot_active = 0;
static int simulated_ding_panel_context_active = 0;
static pthread_mutex_t reboot_mutex = PTHREAD_MUTEX_INITIALIZER;
static int reboot_requested = 0;
static pthread_mutex_t intercom_command_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t intercom_reopen_state_mutex = PTHREAD_MUTEX_INITIALIZER;
static long long intercom_last_close_ms = 0;
static char intercom_last_close_reason[64] = "none";
static char local_ip_addr[64] = "0.0.0.0";
static call_session_t current_call_session;
static int call_session_ready = 0;
static unsigned long long wifi_button_long_started_ms = 0;

#define WIFI_BUTTON_LONG_SEQUENCE_MAX_MS 10000ULL

typedef struct {
    int open_panel_context;
    unsigned int delay_ms;
    const char* reason;
    int start_rtsp_after;
} snapshot_request_t;

// Configuration
static wibox_config_t app_config;

typedef struct {
    pj_thread_desc desc;
    pj_thread_t *thread;
} pj_external_thread_t;

static unsigned long long now_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return ((unsigned long long)tv.tv_sec * 1000ULL) + ((unsigned long long)tv.tv_usec / 1000ULL);
}

static uint32_t create_call_boot_nonce(void) {
    uint32_t nonce = 0;
    int fd = open("/dev/urandom", O_RDONLY);

    if (fd >= 0) {
        if (read(fd, &nonce, sizeof(nonce)) != (ssize_t)sizeof(nonce)) {
            nonce = 0;
        }
        close(fd);
    }
    if (nonce == 0) {
        nonce = (uint32_t)time(NULL) ^ (uint32_t)getpid() ^ (uint32_t)now_ms();
    }
    return nonce ? nonce : 1;
}

static void publish_call_trace_event(const call_session_event_t* event,
                                     int new_session) {
    if (!event) {
        return;
    }
    if (new_session) {
        mqtt_publish_call_id(event->call_id);
    }
    mqtt_publish_call_event(event);
    PJ_LOG(3,(THIS_FILE,
              "Call trace id=%s seq=%u event=%s source=%s route=%s state=%s reason=%s terminal=%d",
              event->call_id, event->sequence, event->event_type,
              event->source, event->route, event->media_state,
              event->reason, event->terminal));
    if (event->terminal) {
        mqtt_publish_call_id(NULL);
    }
}

static int call_trace_start(const char* source, const char* event_type,
                            const char* media_state, const char* route,
                            const char* reason) {
    call_session_event_t event;
    int rc;

    if (!call_session_ready) {
        return -1;
    }
    rc = call_session_start(&current_call_session, source, event_type,
                            media_state, route, reason, time(NULL), &event);
    if (rc > 0) {
        publish_call_trace_event(&event, 1);
    }
    return rc;
}

static void call_trace_record(const char* event_type, const char* media_state,
                              const char* route, const char* reason,
                              int terminal) {
    call_session_event_t event;

    if (!call_session_ready) {
        return;
    }
    if (call_session_record(&current_call_session, event_type, media_state,
                            route, reason, terminal, time(NULL), &event) > 0) {
        publish_call_trace_event(&event, 0);
    }
}

static void mute_audio_input_for_ms(int duration_ms, const char* reason)
{
    unsigned long long until;

    if (duration_ms <= 0) {
        return;
    }
    if (duration_ms > AUDIO_LINE_MUTE_MAX_MS) {
        duration_ms = AUDIO_LINE_MUTE_MAX_MS;
    }

    until = now_ms() + (unsigned int)duration_ms;
    pthread_mutex_lock(&audio_mute_mutex);
    if (until > audio_input_mute_until_ms) {
        audio_input_mute_until_ms = until;
    }
    pthread_mutex_unlock(&audio_mute_mutex);

    PJ_LOG(3,(THIS_FILE, "Muting audio input for %dms reason=%s",
              duration_ms, reason ? reason : "unknown"));
}

static int audio_input_is_muted(void)
{
    int muted;

    pthread_mutex_lock(&audio_mute_mutex);
    muted = now_ms() < audio_input_mute_until_ms;
    pthread_mutex_unlock(&audio_mute_mutex);
    return muted;
}

static pthread_key_t pj_external_thread_key;
static pthread_once_t pj_external_thread_key_once = PTHREAD_ONCE_INIT;

// DING monitoring
static int ding_pipe_fd = -1;
static pthread_t ding_monitor_thread;
static pj_bool_t ding_monitoring_active = PJ_FALSE;
static pthread_mutex_t ringing_timeout_mutex = PTHREAD_MUTEX_INITIALIZER;
static unsigned int ringing_timeout_generation = 0;

typedef struct {
    unsigned int generation;
    int timeout_seconds;
} ringing_timeout_request_t;

// Intercom serial monitoring
static int serial_fd = -1;
static pthread_t serial_monitor_thread;
static pj_bool_t serial_monitoring_active = PJ_FALSE;

// For DTMF duplicate detection
static unsigned char last_dtmf_event = 255;  // Invalid event number
static unsigned int last_dtmf_timestamp = 0;
static time_t last_dtmf_time = 0;

// Function declarations
static pj_bool_t on_rx_request(pjsip_rx_data *rdata);
static pj_bool_t on_rx_response(pjsip_rx_data *rdata);
static void signal_handler(int sig);
static int setup_rtp_socket(void);
static void start_audio_session(const char* remote_ip, int remote_port);
static void stop_audio_session(void);
static int ensure_audio_engine_running(const char* reason);
static void maybe_stop_audio_engine(const char* reason);
static void stop_audio_engine(const char* reason);
static int get_audio_engine_running(void);
static int get_audio_sip_rtp_active(void);
static int get_audio_sip_target(struct sockaddr_in *target);
static void clear_audio_sip_target(void);
static void generate_error_audio(unsigned char* buffer, int size);
static void* ding_monitor_thread_func(void* arg);
static int start_ding_monitoring(void);
static void stop_ding_monitoring(void);
static void handle_ding_trigger(const char* source);
static void handle_uart_frame(const unsigned char frame[4]);
static void handle_audio_test_control(const char* message);
static void handle_video_test_control(const char* message);
static void* serial_monitor_thread_func(void* arg);
static int start_serial_monitoring(void);
static void stop_serial_monitoring(void);
static int configure_serial_raw(int fd);

// Network recovery functions
static int test_rtp_socket_health(void);
static int recreate_rtp_socket(void);
static void refresh_local_ip(void);
static int ensure_rtp_socket_ready(void);
static void send_nat_keepalive(void);

// Thread-safe call_active access functions
static pj_bool_t get_call_active_status(void);
static void set_call_active_status(pj_bool_t active);

// Unified SIP calling callbacks
static void on_call_state_change(sip_call_state_t old_state, sip_call_state_t new_state, void* user_data);
static void on_audio_ready(const char* remote_ip, int remote_rtp_port,
                           int remote_video_rtp_port, void* user_data);
static void start_video_session(const char* remote_ip, int remote_video_port);
static int start_rtsp_preview_session(const char* reason);
static int start_video_worker(const char* remote_ip, int remote_video_port,
                              int payload_type, const char* reason);
static int attach_video_worker_rtp(const char* remote_ip, int remote_video_port,
                                   int payload_type);
static int clear_video_worker_rtp(void);
static int clear_video_worker_rtsp(void);
static int force_video_worker_idr(const char* reason);
static void populate_video_encoder_tuning(video_encoder_tuning_t* tuning);
static void release_sip_video_or_stop(const char* reason);
static void close_video_control_fd(void);
static void stop_video_session(void);
static int start_rtsp_service(void);
static void stop_rtsp_service(void);
static void on_rtsp_client_count_change(int video_clients, int audio_clients, void* user_data);
static int ensure_pj_thread_registered(const char *name);
static void unlock_door(const char* source);
static int ensure_intercom_call_open(const char* reason);
static void clear_intercom_call_state(const char* reason);
static int close_intercom_call(const char* reason);
static void mqtt_open_door_callback(void* user_data);
static void mqtt_trigger_f1_callback(void* user_data);
static void mqtt_reboot_device_callback(void* user_data);
static void mqtt_take_snapshot_callback(void* user_data);
static void mqtt_simulate_ding_callback(void* user_data);
static void mqtt_simulate_handset_answered_callback(void* user_data);
static int start_snapshot_capture(int open_panel_context, unsigned int delay_ms,
                                  const char* reason, int start_rtsp_after);
static void* snapshot_thread_func(void* arg);
static void publish_snapshot_button_availability(void);
static void mqtt_set_video_enabled_callback(int enabled, void* user_data);
static void mqtt_set_video_bitrate_callback(int bitrate_kbps, void* user_data);
static void mqtt_set_sip_outgoing_call_enabled_callback(int enabled, void* user_data);
static void mqtt_set_hangup_on_door_unlock_callback(int enabled, void* user_data);
static void mqtt_set_outgoing_call_target_callback(const char* target_uri, void* user_data);
static void mqtt_set_outgoing_call_timeout_callback(int timeout_seconds, void* user_data);
static void mqtt_set_ring_snapshot_delay_callback(int delay_ms, void* user_data);
static void mqtt_set_call_forward_enabled_callback(int enabled, void* user_data);
static void mqtt_set_rtsp_enabled_callback(int enabled, void* user_data);
static void invalidate_ringing_timeout(const char* reason);
static void schedule_ringing_timeout(int timeout_seconds);
static void* ringing_timeout_thread_func(void* arg);
static void handle_simulated_ding_trigger(void);
static void handle_ding_trigger(const char* source);

// Module to handle incoming requests and responses
static pjsip_module mod_wibox = {
    NULL, NULL,                     // Linked list pointers
    { "mod-wibox", 9 },            // Module name
    -1,                            // Module ID (auto-assigned)
    PJSIP_MOD_PRIORITY_APPLICATION, // Priority level
    NULL,                          // load() callback
    NULL,                          // start() callback
    NULL,                          // stop() callback
    NULL,                          // unload() callback
    &on_rx_request,                // Handle incoming SIP requests
    &on_rx_response,               // Handle SIP responses
    NULL,                          // Handle outgoing requests
    NULL,                          // Handle outgoing responses
    NULL,                          // Handle transaction state changes
};

// Thread-safe call_active access functions
static pj_bool_t get_call_active_status(void) {
    pj_bool_t active;
    pj_mutex_lock(call_active_mutex);
    active = call_active;
    pj_mutex_unlock(call_active_mutex);
    return active;
}

static void set_call_active_status(pj_bool_t active) {
    pj_mutex_lock(call_active_mutex);
    call_active = active;
    pj_mutex_unlock(call_active_mutex);
}

static void set_audio_engine_running(int running) {
    pthread_mutex_lock(&audio_state_mutex);
    audio_engine_running = running ? 1 : 0;
    pthread_mutex_unlock(&audio_state_mutex);
}

static int get_audio_engine_running(void) {
    int running;

    pthread_mutex_lock(&audio_state_mutex);
    running = audio_engine_running;
    pthread_mutex_unlock(&audio_state_mutex);
    return running;
}

static int get_audio_sip_rtp_active(void) {
    int active;

    pthread_mutex_lock(&audio_state_mutex);
    active = audio_sip_rtp_active;
    pthread_mutex_unlock(&audio_state_mutex);
    return active;
}

static int get_audio_sip_target(struct sockaddr_in *target) {
    int active;

    pthread_mutex_lock(&audio_state_mutex);
    active = audio_sip_rtp_active;
    if (active && target) {
        *target = remote_rtp_addr;
    }
    pthread_mutex_unlock(&audio_state_mutex);
    return active;
}

static void clear_audio_sip_target(void) {
    pthread_mutex_lock(&audio_state_mutex);
    audio_sip_rtp_active = 0;
    memset(&remote_rtp_addr, 0, sizeof(remote_rtp_addr));
    pthread_mutex_unlock(&audio_state_mutex);
}

static int get_interface_ip(const char* ifname, char* ip_str, size_t len) {
    struct ifaddrs *ifaddrs_ptr = NULL;
    struct ifaddrs *ifa = NULL;
    int found = 0;

    if (!ifname || !ip_str || len == 0) {
        return 0;
    }

    ip_str[0] = '\0';

    if (getifaddrs(&ifaddrs_ptr) == -1) {
        return 0;
    }

    for (ifa = ifaddrs_ptr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET &&
            strcmp(ifa->ifa_name, ifname) == 0) {
            struct sockaddr_in* addr = (struct sockaddr_in*)ifa->ifa_addr;
            inet_ntop(AF_INET, &addr->sin_addr, ip_str, len);
            found = ip_str[0] != '\0';
            break;
        }
    }

    freeifaddrs(ifaddrs_ptr);
    return found;
}

static void get_local_ip(char* ip_str, size_t len) {
    struct ifaddrs *ifaddrs_ptr = NULL;
    struct ifaddrs *ifa = NULL;
    int attempt;

    if (!ip_str || len == 0) {
        return;
    }

    // WiBox media always runs over WiFi. During boot, eth0 can have a factory
    // static address before wlan0 DHCP finishes, so wait briefly for wlan0.
    for (attempt = 0; attempt < 20; attempt++) {
        if (get_interface_ip("wlan0", ip_str, len)) {
            return;
        }
        usleep(500000);
    }

    strcpy(ip_str, "127.0.0.1");

    if (getifaddrs(&ifaddrs_ptr) == -1) {
        return;
    }

    for (ifa = ifaddrs_ptr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET &&
            strcmp(ifa->ifa_name, "lo") != 0) {
            struct sockaddr_in* addr = (struct sockaddr_in*)ifa->ifa_addr;
            inet_ntop(AF_INET, &addr->sin_addr, ip_str, len);
            break;
        }
    }
    freeifaddrs(ifaddrs_ptr);
}

// Network recovery functions
static int test_rtp_socket_health(void) {
    if (rtp_socket < 0) {
        return 0;  // Socket not initialized
    }

    // Test 1: Basic loopback test (catches socket issues)
    struct sockaddr_in test_addr;
    char test_packet[12] = {0};

    memset(&test_addr, 0, sizeof(test_addr));
    test_addr.sin_family = AF_INET;
    test_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    test_addr.sin_port = htons(1234);  // Dummy port

    ssize_t result = sendto(rtp_socket, test_packet, sizeof(test_packet), MSG_DONTWAIT,
                           (struct sockaddr*)&test_addr, sizeof(test_addr));

    if (result < 0) {
        if (errno == ENETUNREACH || errno == ENETDOWN || errno == EHOSTUNREACH) {
            PJ_LOG(2,(THIS_FILE, "RTP socket loopback test failed: %s", strerror(errno)));
            return 0;  // Network issue detected
        }
    }

    // Test 2: Try to send to current default gateway (catches routing issues)
    char gateway_cmd[] = "ip route show default | awk '{print $3}' | head -1";
    FILE *fp = popen(gateway_cmd, "r");
    if (fp) {
        char gateway_ip[16];
        if (fgets(gateway_ip, sizeof(gateway_ip), fp)) {
            // Remove newline
            char *newline = strchr(gateway_ip, '\n');
            if (newline) *newline = '\0';

            if (strlen(gateway_ip) > 0) {
                memset(&test_addr, 0, sizeof(test_addr));
                test_addr.sin_family = AF_INET;
                test_addr.sin_port = htons(53);  // DNS port (likely filtered, but tests routing)

                if (inet_pton(AF_INET, gateway_ip, &test_addr.sin_addr) == 1) {
                    result = sendto(rtp_socket, test_packet, sizeof(test_packet), MSG_DONTWAIT,
                                   (struct sockaddr*)&test_addr, sizeof(test_addr));

                    if (result < 0) {
                        if (errno == ENETUNREACH || errno == ENETDOWN || errno == EHOSTUNREACH) {
                            PJ_LOG(2,(THIS_FILE, "RTP socket gateway test failed (%s): %s", gateway_ip, strerror(errno)));
                            pclose(fp);
                            return 0;  // Routing issue detected
                        }
                    } else {
                        PJ_LOG(4,(THIS_FILE, "RTP socket gateway test passed (%s)", gateway_ip));
                    }
                }
            }
        }
        pclose(fp);
    }

    return 1;  // Socket appears healthy
}

static void refresh_local_ip(void) {
    char new_local_ip[16];
    get_local_ip(new_local_ip, sizeof(new_local_ip));

    // Skip invalid results so a transient network-down state does not overwrite a
    // good address with 0.0.0.0. sip_calling_set_local_ip is a no-op if unchanged
    // and logs when the advertised SIP/SDP address actually moves (e.g. DHCP).
    if (new_local_ip[0] && strcmp(new_local_ip, "0.0.0.0") != 0) {
        sip_calling_set_local_ip(new_local_ip);
    }
}

static int recreate_rtp_socket(void) {
    PJ_LOG(3,(THIS_FILE, "Recreating RTP socket due to network issues (NAT/firewall recovery)"));

    // Close existing socket
    if (rtp_socket >= 0) {
        close(rtp_socket);
        rtp_socket = -1;
    }

    // Try to clear network state caches
    PJ_LOG(3,(THIS_FILE, "Attempting to refresh network state"));
    system("ip neigh flush all >/dev/null 2>&1 || arp -d -a >/dev/null 2>&1 || true");

    // Clear connection tracking state if possible (helps with firewalls)
    system("echo 1 > /proc/sys/net/netfilter/nf_conntrack_tcp_timeout_established 2>/dev/null || true");
    system("conntrack -F 2>/dev/null || true");  // Clear conntrack table

    // Longer delay to let network stack settle after state clearing
    usleep(200000);  // 200ms

    // Refresh local IP address (might not change, but good to verify)
    refresh_local_ip();

    // Create new socket
    rtp_socket = setup_rtp_socket();
    if (rtp_socket < 0) {
        PJ_LOG(1,(THIS_FILE, "Failed to recreate RTP socket"));
        return -1;
    }

    PJ_LOG(3,(THIS_FILE, "RTP socket recreated successfully with fresh network state"));
    return 0;
}

static int ensure_rtp_socket_ready(void) {
    // First check if socket exists
    if (rtp_socket < 0) {
        PJ_LOG(2,(THIS_FILE, "RTP socket not initialized, creating new one"));
        return recreate_rtp_socket();
    }

    // Test socket health
    if (!test_rtp_socket_health()) {
        PJ_LOG(2,(THIS_FILE, "RTP socket health check failed, recreating"));
        return recreate_rtp_socket();
    }

    return 0;  // Socket is ready
}

// Send NAT keep-alive packet during active calls to maintain NAT bindings
static void send_nat_keepalive(void) {
    struct sockaddr_in target;

    if (rtp_socket < 0 || !get_audio_sip_target(&target)) {
        return;  // Only while a SIP RTP target is attached.
    }

    // Send minimal RTP packet to maintain NAT binding
    // This prevents NAT timeouts during long silent periods
    unsigned char keepalive_packet[12] = {
        0x80, 0x08, 0x00, 0x00,  // RTP header: V=2, PT=8 (PCMA), seq=0
        0x00, 0x00, 0x00, 0x00,  // timestamp=0
        0x00, 0x00, 0x00, 0x01   // SSRC=1
    };

    ssize_t result = sendto(rtp_socket, keepalive_packet, sizeof(keepalive_packet), MSG_DONTWAIT,
                           (struct sockaddr*)&target, sizeof(target));

    if (result < 0) {
        PJ_LOG(4,(THIS_FILE, "NAT keep-alive failed: %s", strerror(errno)));
    } else {
        PJ_LOG(5,(THIS_FILE, "NAT keep-alive sent"));
    }
}

static void handle_dtmf_event(unsigned char event, unsigned char volume, unsigned short duration) {
    char dtmf_char;

    switch (event) {
        case DTMF_EVENT_0: dtmf_char = '0'; break;
        case DTMF_EVENT_1: dtmf_char = '1'; break;
        case DTMF_EVENT_2: dtmf_char = '2'; break;
        case DTMF_EVENT_3: dtmf_char = '3'; break;
        case DTMF_EVENT_4: dtmf_char = '4'; break;
        case DTMF_EVENT_5: dtmf_char = '5'; break;
        case DTMF_EVENT_6: dtmf_char = '6'; break;
        case DTMF_EVENT_7: dtmf_char = '7'; break;
        case DTMF_EVENT_8: dtmf_char = '8'; break;
        case DTMF_EVENT_9: dtmf_char = '9'; break;
        case DTMF_EVENT_STAR: dtmf_char = '*'; break;
        case DTMF_EVENT_HASH: dtmf_char = '#'; break;
        default: dtmf_char = '?'; break;
    }

    PJ_LOG(3,(THIS_FILE, "DTMF DETECTED: '%c' (event=%d, volume=%d, duration=%d)",
              dtmf_char, event, volume, duration));

    if (dtmf_char == '#') {
        unlock_door("dtmf");
    }
}

static pj_bool_t handle_sip_info_dtmf(pjsip_rx_data *rdata) {
    const char *body;
    int body_len;
    char digit = 0;
    int i;

    if (!rdata->msg_info.msg->body || !rdata->msg_info.msg->body->data) {
        PJ_LOG(3,(THIS_FILE, "SIP INFO without body"));
        pjsip_endpt_respond_stateless(sip_endpt, rdata, 200, NULL, NULL, NULL);
        return PJ_TRUE;
    }

    body = (const char *)rdata->msg_info.msg->body->data;
    body_len = (int)rdata->msg_info.msg->body->len;
    PJ_LOG(3,(THIS_FILE, "SIP INFO body:\n%.*s", body_len, body));

    for (i = 0; i < body_len; i++) {
        if (body[i] == '#' || body[i] == '*') {
            digit = body[i];
            break;
        }
    }
    if (!digit) {
        const char *signal = strstr(body, "Signal=");
        if (!signal) signal = strstr(body, "Signal: ");
        if (!signal) signal = strstr(body, "DTMF ");
        if (!signal) signal = strstr(body, "digit=");
        if (signal) {
            const char *p;
            for (p = signal; p < body + body_len; p++) {
                if (*p == '#' || *p == '*' || (*p >= '0' && *p <= '9')) {
                    digit = *p;
                    break;
                }
            }
        }
    }

    if (digit) {
        unsigned char event;
        if (digit == '#') event = DTMF_EVENT_HASH;
        else if (digit == '*') event = DTMF_EVENT_STAR;
        else event = (unsigned char)(digit - '0');
        PJ_LOG(3,(THIS_FILE, "SIP INFO DTMF detected: '%c'", digit));
        handle_dtmf_event(event, 0, 0);
    } else {
        PJ_LOG(3,(THIS_FILE, "SIP INFO had no DTMF digit"));
    }

    pjsip_endpt_respond_stateless(sip_endpt, rdata, 200, NULL, NULL, NULL);
    return PJ_TRUE;
}

static pj_bool_t parse_rtp_dtmf_event(unsigned char* rtp_packet, ssize_t packet_len) {
    unsigned char payload_type;
    int csrc_count;
    int header_len;
    unsigned int rtp_timestamp;
    unsigned char event;
    unsigned char flags_volume;
    unsigned char end_bit;
    unsigned char volume;
    unsigned short duration;
    time_t now;

    if (packet_len < 16) return PJ_FALSE;  // Too short for RTP + DTMF event

    payload_type = rtp_packet[1] & 0x7F;
    if (payload_type != current_dtmf_payload_type) {
        return PJ_FALSE;
    }

    csrc_count = rtp_packet[0] & 0x0F;
    header_len = 12 + (csrc_count * 4);
    if (rtp_packet[0] & 0x10) {
        int ext_words;
        if (packet_len < header_len + 4) return PJ_TRUE;
        ext_words = (rtp_packet[header_len + 2] << 8) | rtp_packet[header_len + 3];
        header_len += 4 + ext_words * 4;
    }
    if (packet_len < header_len + 4) return PJ_TRUE;

    rtp_timestamp = (rtp_packet[4] << 24) | (rtp_packet[5] << 16) |
                    (rtp_packet[6] << 8) | rtp_packet[7];

    event = rtp_packet[header_len];
    flags_volume = rtp_packet[header_len + 1];
    duration = (rtp_packet[header_len + 2] << 8) | rtp_packet[header_len + 3];
    end_bit = (flags_volume & 0x80) >> 7;
    volume = flags_volume & 0x3F;

    PJ_LOG(3,(THIS_FILE, "RTP DTMF packet: event=%u end=%u volume=%u duration=%u ts=%u",
              event, end_bit, volume, duration, rtp_timestamp));

    now = time(NULL);
    if (event == last_dtmf_event &&
        rtp_timestamp == last_dtmf_timestamp &&
        (now - last_dtmf_time) < 2) {
        return PJ_TRUE;
    }

    last_dtmf_event = event;
    last_dtmf_timestamp = rtp_timestamp;
    last_dtmf_time = now;
    handle_dtmf_event(event, volume, duration);

    return PJ_TRUE;
}

// Unified SIP calling callback implementations
static void on_call_state_change(sip_call_state_t old_state, sip_call_state_t new_state, void* user_data) {
    (void)user_data;
    // Handle call establishment (both incoming and outgoing)
    if (new_state == SIP_CALL_STATE_ESTABLISHED && old_state != SIP_CALL_STATE_ESTABLISHED) {
        invalidate_ringing_timeout("sip-established");
        if (simulated_ding_panel_context_active) {
            PJ_LOG(3,(THIS_FILE, "Call established - simulated DING panel context already active"));
        } else if (get_call_active_status()) {
            PJ_LOG(3,(THIS_FILE, "Call established - intercom line already active, not sending START_CALL"));
        } else {
            PJ_LOG(3,(THIS_FILE, "Call established - sending START_CALL to intercom"));
            mute_audio_input_for_ms(app_config.audio_line_mute_ms, "intercom-start");
            ensure_intercom_call_open("sip-established");
        }
        set_call_active_status(PJ_TRUE);
        mqtt_publish_call_active(1);
        mqtt_publish_sip_call_active(1);
        mqtt_publish_ringing(0);
        mqtt_publish_media_state("established");
        if (call_trace_start("sip_incoming", "established", "established",
                             "sip", "sip-established") == 0) {
            call_trace_record("established", "established", "sip",
                              "sip-established", 0);
        }
        prometheus_set_call_active(1);
        prometheus_set_sip_call_active(1);
        prometheus_set_ringing(0);
        prometheus_inc_call_started();
    }

    // Handle call termination - ONLY send STOP_CALL if we're ending an ESTABLISHED call
    if (old_state == SIP_CALL_STATE_ESTABLISHED &&
        (new_state == SIP_CALL_STATE_IDLE || new_state == SIP_CALL_STATE_FAILED)) {
        if (get_call_active_status()) {
            PJ_LOG(3,(THIS_FILE, "Established call terminated - sending STOP_CALL to intercom"));
            mute_audio_input_for_ms(app_config.audio_line_mute_ms, "intercom-stop");
            close_intercom_call("sip-established-ended");
        } else {
            PJ_LOG(3,(THIS_FILE, "Established call terminated - intercom line already closed"));
            clear_intercom_call_state("sip-established-ended");
        }
        release_sip_video_or_stop("established-call-ended");
        stop_audio_session();
        mqtt_publish_sip_call_active(0);
        mqtt_publish_ringing(0);
        mqtt_publish_media_state("idle");
        call_trace_record(new_state == SIP_CALL_STATE_FAILED ? "sip_failed" : "sip_ended",
                          "idle", "sip", "sip-established-ended", 1);
        prometheus_set_sip_call_active(0);
        prometheus_set_ringing(0);
    }

    // Handle non-established call termination (no intercom command needed)
    if (old_state != SIP_CALL_STATE_ESTABLISHED && old_state != SIP_CALL_STATE_IDLE &&
        (new_state == SIP_CALL_STATE_IDLE || new_state == SIP_CALL_STATE_FAILED)) {
        if (simulated_ding_panel_context_active) {
            PJ_LOG(3,(THIS_FILE, "Non-established simulated DING call terminated - sending STOP_CALL to intercom"));
            mute_audio_input_for_ms(app_config.audio_line_mute_ms, "intercom-stop");
            close_intercom_call("sip-non-established-simulated");
        } else {
            PJ_LOG(3,(THIS_FILE, "Non-established call terminated - no intercom command needed"));
        }
        release_sip_video_or_stop("non-established-call-ended");
        stop_audio_session();
        mqtt_publish_sip_call_active(0);
        mqtt_publish_ringing(0);
        mqtt_publish_media_state("idle");
        call_trace_record(new_state == SIP_CALL_STATE_FAILED ? "sip_failed" : "sip_cancelled",
                          "idle", "sip", "sip-non-established-ended", 1);
        prometheus_set_sip_call_active(0);
        prometheus_set_ringing(0);
    }
}

static void on_audio_ready(const char* remote_ip, int remote_rtp_port,
                           int remote_video_rtp_port, void* user_data) {
    (void)user_data;
    PJ_LOG(3,(THIS_FILE, "Media ready: audio=%s:%d video=%s:%d",
              remote_ip, remote_rtp_port, remote_ip, remote_video_rtp_port));

    // Don't attach duplicate RTP targets for the same SIP media session.
    if (get_audio_sip_rtp_active()) {
        PJ_LOG(3,(THIS_FILE, "Audio session already active - ignoring duplicate"));
        return;
    }
    start_audio_session(remote_ip, remote_rtp_port);
    PJ_LOG(3,(THIS_FILE, "Audio session start returned; starting video"));
    start_video_session(remote_ip, remote_video_rtp_port);
}

static void close_video_control_fd(void) {
    if (video_bridge_control_fd >= 0) {
        close(video_bridge_control_fd);
        video_bridge_control_fd = -1;
    }
}

static int write_video_control_command(const char* command, size_t len) {
    size_t off = 0;

    if (video_bridge_control_fd < 0 || !command || len == 0) {
        return -1;
    }

    while (off < len) {
        ssize_t wr = write(video_bridge_control_fd, command + off, len - off);
        if (wr > 0) {
            off += (size_t)wr;
            continue;
        }
        if (wr < 0 && errno == EINTR) {
            continue;
        }
        return -1;
    }
    return 0;
}

static int attach_video_worker_rtp(const char* remote_ip, int remote_video_port,
                                   int payload_type) {
    char command[160];
    size_t len;

    if (video_bridge_control_fd < 0 || !remote_ip || !remote_ip[0] ||
        remote_video_port <= 0 || payload_type <= 0 || payload_type > 127) {
        return -1;
    }

    len = (size_t)snprintf(command, sizeof(command), "SET_RTP %s %d %d %d\n",
                           remote_ip, remote_video_port,
                           app_config.video_rtp_port, payload_type);
    if (len == 0 || len >= sizeof(command)) {
        return -1;
    }

    return write_video_control_command(command, len);
}

static int request_video_worker_snapshot(const char* path, int quality) {
    char command[220];
    size_t len;

    if (video_bridge_pid <= 0 || video_bridge_control_fd < 0 ||
        !path || !path[0]) {
        return -1;
    }

    len = (size_t)snprintf(command, sizeof(command), "SNAPSHOT %s %d\n",
                           path, quality > 0 ? quality : 90);
    if (len == 0 || len >= sizeof(command)) {
        return -1;
    }
    return write_video_control_command(command, len);
}

static int wait_for_snapshot_file(const char* path, int timeout_ms) {
    int waited = 0;

    while (waited < timeout_ms) {
        struct stat st;
        if (stat(path, &st) == 0 && st.st_size > 0) {
            return 0;
        }
        usleep(50000);
        waited += 50;
    }
    return -1;
}

static int clear_video_worker_rtp(void) {
    const char command[] = "CLEAR_RTP\n";

    if (video_bridge_pid <= 0 || video_bridge_control_fd < 0) {
        return -1;
    }
    return write_video_control_command(command, sizeof(command) - 1);
}

static int clear_video_worker_rtsp(void) {
    const char command[] = "CLEAR_RTSP\n";

    if (video_bridge_pid <= 0 || video_bridge_control_fd < 0) {
        return -1;
    }
    return write_video_control_command(command, sizeof(command) - 1);
}

static int force_video_worker_idr(const char* reason) {
    const char command[] = "FORCE_IDR\n";
    int ret;

    if (video_bridge_pid <= 0 || video_bridge_control_fd < 0) {
        return -1;
    }

    ret = write_video_control_command(command, sizeof(command) - 1);
    if (ret == 0) {
        PJ_LOG(3,(THIS_FILE, "Requested video IDR frame (%s)",
                  reason ? reason : "unknown"));
    }
    return ret;
}

static void start_video_session(const char* remote_ip, int remote_video_port) {
    const sip_call_session_t *session;
    int payload_type;
    int wait_count;

    if (!app_config.video_enabled) {
        PJ_LOG(3,(THIS_FILE, "Video disabled by configuration"));
        return;
    }
    if (remote_video_port <= 0) {
        PJ_LOG(3,(THIS_FILE, "Remote SDP has no video port; not starting video"));
        return;
    }

    for (wait_count = 0; wait_count < 30; wait_count++) {
        int active;
        pthread_mutex_lock(&snapshot_mutex);
        active = snapshot_active;
        pthread_mutex_unlock(&snapshot_mutex);
        if (!active) {
            break;
        }
        if (wait_count == 0) {
            PJ_LOG(3,(THIS_FILE, "Video start waiting for snapshot capture to finish"));
        }
        usleep(100000);
    }

    session = sip_calling_get_session();
    payload_type = app_config.video_payload_type;
    if (session && session->remote_video_payload_type > 0) {
        payload_type = session->remote_video_payload_type;
    }

    if (video_bridge_pid > 0) {
        if (attach_video_worker_rtp(remote_ip, remote_video_port, payload_type) == 0) {
            PJ_LOG(3,(THIS_FILE, "Attached SIP RTP target %s:%d payload=%d to video worker pid=%d",
                      remote_ip, remote_video_port, payload_type, video_bridge_pid));
            video_bridge_has_sip_rtp = 1;
            return;
        }
        PJ_LOG(2,(THIS_FILE, "Failed to attach SIP RTP target to video worker; restarting video worker"));
        stop_video_session();
    }

    start_video_worker(remote_ip, remote_video_port, payload_type, "sip");
}

static int start_video_worker(const char* remote_ip, int remote_video_port,
                              int payload_type, const char* reason) {
    const char *dumpfile = NULL;
    long long dump_limit_bytes = 0;
    int rtsp_video_fd;
    int recording_seconds;
    int control_pipe[2] = {-1, -1};
    int has_sip_rtp = remote_ip && remote_ip[0] && remote_video_port > 0;
    video_encoder_tuning_t tuning;

    if (!app_config.video_enabled) {
        PJ_LOG(3,(THIS_FILE, "Video worker not started: video disabled"));
        return 0;
    }
    if (video_bridge_pid > 0) {
        PJ_LOG(3,(THIS_FILE, "Video worker already running pid=%d", video_bridge_pid));
        return 1;
    }
    if (payload_type <= 0 || payload_type > 127) {
        payload_type = app_config.video_payload_type;
    }
    populate_video_encoder_tuning(&tuning);

    if (has_sip_rtp &&
        app_config.video_recording_enabled && app_config.video_recording_path[0]) {
        recording_seconds = clamp_recording_max_seconds(app_config.video_recording_max_seconds);
        dumpfile = app_config.video_recording_path;
        dump_limit_bytes = ((long long)app_config.video_bitrate_kbps * 1024LL / 8LL) *
                           (long long)recording_seconds;
        PJ_LOG(3,(THIS_FILE, "Video recording enabled path=%s limit=%lld bytes (%ds)",
                  dumpfile, dump_limit_bytes, recording_seconds));
    }

    rtsp_video_fd = rtsp_stream_get_video_pipe_fd();
    if (!has_sip_rtp && rtsp_video_fd < 0) {
        PJ_LOG(2,(THIS_FILE, "Video worker not started: no SIP target and RTSP pipe unavailable"));
        return 0;
    }
    if (pipe(control_pipe) < 0) {
        PJ_LOG(1,(THIS_FILE, "Video worker not started: control pipe failed: %s",
                  strerror(errno)));
        return 0;
    }
    close_video_control_fd();

    video_bridge_pid = fork();
    if (video_bridge_pid < 0) {
        PJ_LOG(1,(THIS_FILE, "Failed to fork video worker: %s", strerror(errno)));
        close(control_pipe[0]);
        close(control_pipe[1]);
        video_bridge_pid = -1;
        return 0;
    }
    if (video_bridge_pid == 0) {
        int log_fd = open("/tmp/wibox-video-worker.log",
                          O_WRONLY | O_CREAT | O_TRUNC, 0644);
        int fd;
        long max_fd;
        close(control_pipe[1]);
        if (log_fd >= 0) {
            dup2(log_fd, STDOUT_FILENO);
            dup2(log_fd, STDERR_FILENO);
            close(log_fd);
        }
        max_fd = sysconf(_SC_OPEN_MAX);
        if (max_fd < 0 || max_fd > 256) {
            max_fd = 256;
        }
        for (fd = 3; fd < max_fd; fd++) {
            if (fd == rtsp_video_fd || fd == control_pipe[0]) {
                continue;
            }
            close(fd);
        }
        _exit(video_worker_run(has_sip_rtp ? remote_ip : NULL,
                               has_sip_rtp ? remote_video_port : 0,
                               has_sip_rtp ? app_config.video_rtp_port : 0,
                               payload_type,
                               app_config.video_bitrate_kbps,
                               &tuning,
                               dumpfile, dump_limit_bytes,
                               rtsp_video_fd, control_pipe[0]));
    }

    close(control_pipe[0]);
    video_bridge_control_fd = control_pipe[1];
    video_bridge_has_sip_rtp = has_sip_rtp ? 1 : 0;
    PJ_LOG(3,(THIS_FILE, "Started video worker pid=%d reason=%s sip_rtp=%d target=%s:%d payload=%d bitrate=%dkbps gop_n=%d idr_interval=%d brc_mode=%d rtsp_periodic_idr_ms=%d rtsp_fd=%d",
              video_bridge_pid, reason ? reason : "unknown", video_bridge_has_sip_rtp,
              has_sip_rtp ? remote_ip : "-", has_sip_rtp ? remote_video_port : 0,
              payload_type, app_config.video_bitrate_kbps, tuning.gop_n,
              tuning.idr_interval, tuning.brc_mode, tuning.rtsp_periodic_idr_ms,
              rtsp_video_fd));
    mqtt_publish_video_active(1);
    prometheus_set_video_active(1);
    prometheus_inc_video_started();
    return 1;
}

static int start_rtsp_preview_session(const char* reason) {
    if (!app_config.video_enabled) {
        PJ_LOG(3,(THIS_FILE, "RTSP preview not started: video disabled"));
        return 0;
    }
    if (!app_config.rtsp_enabled) {
        PJ_LOG(3,(THIS_FILE, "RTSP preview not started: RTSP disabled"));
        return 0;
    }
    return start_video_worker(NULL, 0, app_config.video_payload_type,
                              reason ? reason : "rtsp");
}

static void release_sip_video_or_stop(const char* reason) {
    if (video_bridge_pid <= 0) {
        return;
    }

    if (app_config.rtsp_enabled && app_config.video_enabled &&
        rtsp_stream_get_video_client_count() > 0) {
        if (video_bridge_has_sip_rtp) {
            if (clear_video_worker_rtp() == 0) {
                PJ_LOG(3,(THIS_FILE, "Detached SIP RTP from video worker pid=%d; keeping RTSP alive (%s)",
                          video_bridge_pid, reason ? reason : "unknown"));
            } else {
                PJ_LOG(2,(THIS_FILE, "Failed to detach SIP RTP from video worker pid=%d; keeping RTSP alive anyway",
                          video_bridge_pid));
            }
        }
        video_bridge_has_sip_rtp = 0;
        mqtt_publish_video_active(1);
        prometheus_set_video_active(1);
        return;
    }

    stop_video_session();
}

static void stop_video_session(void) {
    int status;
    int i;
    pid_t pid;

    pthread_mutex_lock(&video_lifecycle_mutex);
    if (video_bridge_pid <= 0) {
        pthread_mutex_unlock(&video_lifecycle_mutex);
        return;
    }

    pid = video_bridge_pid;
    PJ_LOG(3,(THIS_FILE, "Stopping video worker pid=%d", pid));
    close_video_control_fd();
    kill(pid, SIGTERM);
    for (i = 0; i < 20; i++) {
        pid_t r = waitpid(pid, &status, WNOHANG);
        if (r == pid) {
            video_bridge_pid = -1;
            video_bridge_has_sip_rtp = 0;
            mqtt_publish_video_active(0);
            prometheus_set_video_active(0);
            pthread_mutex_unlock(&video_lifecycle_mutex);
            return;
        }
        usleep(100000);
    }

    kill(pid, SIGKILL);
    waitpid(pid, &status, 0);
    video_bridge_pid = -1;
    video_bridge_has_sip_rtp = 0;
    mqtt_publish_video_active(0);
    prometheus_set_video_active(0);
    pthread_mutex_unlock(&video_lifecycle_mutex);
}

static int start_rtsp_service(void) {
    if (!app_config.rtsp_enabled) {
        return 0;
    }
    if (rtsp_stream_start(app_config.rtsp_port, local_ip_addr,
                          app_config.video_enabled,
                          app_config.rtsp_auth_user,
                          app_config.rtsp_auth_pass) < 0) {
        printf("Warning: Failed to start RTSP stream\n");
        return -1;
    }
    rtsp_stream_set_client_callback(on_rtsp_client_count_change, NULL);
    return 0;
}

static void stop_rtsp_service(void) {
    if (video_bridge_pid > 0 && video_bridge_has_sip_rtp) {
        clear_video_worker_rtsp();
    }
    rtsp_stream_stop();
    if (video_bridge_pid > 0 && !video_bridge_has_sip_rtp) {
        stop_video_session();
    }
    maybe_stop_audio_engine("rtsp-disabled");
}

static void pj_external_thread_key_init(void) {
    pthread_key_create(&pj_external_thread_key, free);
}

static int ensure_pj_thread_registered(const char *name) {
    pj_external_thread_t *external_thread;
    pj_status_t status;

    if (!pool) {
        printf("%s callback ignored until PJLIB is ready\n",
               name ? name : "external");
        return 0;
    }
    if (pj_thread_is_registered()) {
        return 1;
    }

    pthread_once(&pj_external_thread_key_once, pj_external_thread_key_init);
    external_thread = pthread_getspecific(pj_external_thread_key);
    if (!external_thread) {
        external_thread = calloc(1, sizeof(*external_thread));
        if (!external_thread) {
            printf("Failed to allocate PJLIB thread registration for %s\n",
                   name ? name : "external");
            return 0;
        }
        status = pj_thread_register(name ? name : "external",
                                    external_thread->desc,
                                    &external_thread->thread);
        if (status != PJ_SUCCESS) {
            printf("Failed to register %s thread with PJLIB: %d\n",
                   name ? name : "external", status);
            free(external_thread);
            return 0;
        }
        pthread_setspecific(pj_external_thread_key, external_thread);
    }

    return 1;
}

static void on_rtsp_client_count_change(int video_clients, int audio_clients, void* user_data) {
    int should_start_preview;
    int should_stop_preview;
    int should_start_audio;
    int should_maybe_stop_audio;
    int should_force_idr;

    (void)user_data;

    printf("rtsp: active clients video=%d audio=%d\n", video_clients, audio_clients);
    pthread_mutex_lock(&snapshot_mutex);
    should_start_preview = app_config.rtsp_enabled && app_config.video_enabled &&
                           video_clients > 0 && video_bridge_pid <= 0;
    pthread_mutex_unlock(&snapshot_mutex);
    should_force_idr = app_config.rtsp_enabled && app_config.video_enabled &&
                       video_clients > 0 && video_bridge_pid > 0;
    should_stop_preview = (!app_config.rtsp_enabled || !app_config.video_enabled ||
                          video_clients == 0) && video_bridge_pid > 0 &&
                          !video_bridge_has_sip_rtp &&
                          !sip_calling_is_call_active();
    should_start_audio = audio_clients > 0 && !get_audio_engine_running();
    should_maybe_stop_audio = audio_clients == 0;
    if (!should_start_preview && !should_stop_preview &&
        !should_start_audio && !should_maybe_stop_audio && !should_force_idr) {
        return;
    }

    if (!ensure_pj_thread_registered("rtsp_callback")) {
        return;
    }

    if (should_start_audio) {
        ensure_audio_engine_running("rtsp-client");
    }

    if (should_start_preview) {
        if (start_rtsp_preview_session("rtsp-client")) {
            should_force_idr = 1;
        }
    }

    if (should_force_idr) {
        force_video_worker_idr("rtsp-client");
    }

    if (should_stop_preview) {
        stop_video_session();
    }

    if (should_maybe_stop_audio) {
        maybe_stop_audio_engine("rtsp-client");
    }
}

static void unlock_door(const char* source) {
    printf("Unlocking door from %s\n", source ? source : "unknown");
    if (intercom_send_command(INTERCOM_CMD_UNLOCK_DOOR) == 0) {
        printf("Door unlock command sent successfully\n");
        mqtt_publish_door_unlocked_pulse();
        call_trace_record("door_opened", NULL, NULL, source, 0);
        prometheus_inc_door_unlock();

        /*
         * Opening the door completes the doorbell interaction.  In
         * particular, an MQTT button press can happen while Asterisk is
         * still forking the SIP call to several clients.  End our SIP leg
         * immediately so Asterisk propagates the hangup to every remaining
         * ringing destination.
         */
        if (app_config.hangup_on_door_unlock && source &&
            strcmp(source, "mqtt") == 0 && sip_calling_is_call_active()) {
            PJ_LOG(3, (THIS_FILE,
                       "Door opened - terminating active SIP call"));
            if (sip_calling_terminate_call() != PJ_SUCCESS) {
                PJ_LOG(2, (THIS_FILE,
                           "Failed to terminate SIP call after door unlock"));
            }
        }
    } else {
        printf("Failed to send door unlock command\n");
    }
}

static void record_intercom_close(const char* reason) {
    pthread_mutex_lock(&intercom_reopen_state_mutex);
    intercom_last_close_ms = now_ms();
    snprintf(intercom_last_close_reason, sizeof(intercom_last_close_reason),
             "%s", reason ? reason : "unknown");
    pthread_mutex_unlock(&intercom_reopen_state_mutex);
}

static void wait_for_intercom_reopen_guard(const char* reason) {
    long long last_close_ms;
    long long elapsed_ms;
    int wait_ms;
    char close_reason[64];

    if (app_config.intercom_reopen_guard_ms <= 0) {
        return;
    }

    pthread_mutex_lock(&intercom_reopen_state_mutex);
    last_close_ms = intercom_last_close_ms;
    snprintf(close_reason, sizeof(close_reason), "%s",
             intercom_last_close_reason);
    pthread_mutex_unlock(&intercom_reopen_state_mutex);

    if (last_close_ms <= 0) {
        return;
    }

    elapsed_ms = now_ms() - last_close_ms;
    if (elapsed_ms < 0) {
        elapsed_ms = 0;
    }
    wait_ms = app_config.intercom_reopen_guard_ms - (int)elapsed_ms;
    if (wait_ms <= 0) {
        return;
    }

    PJ_LOG(3,(THIS_FILE,
              "Intercom reopen guard waiting %d ms before %s "
              "(previous close=%s, elapsed=%lld ms)",
              wait_ms, reason ? reason : "open", close_reason, elapsed_ms));
    usleep((unsigned int)wait_ms * 1000U);
}

static int ensure_intercom_call_open(const char* reason) {
    int rc = 0;

    pthread_mutex_lock(&intercom_command_mutex);
    if (get_call_active_status()) {
        PJ_LOG(3,(THIS_FILE, "Intercom call line already active - not sending START_CALL reason=%s",
                  reason ? reason : "unknown"));
        goto out;
    }

    wait_for_intercom_reopen_guard(reason);

    if (get_call_active_status()) {
        PJ_LOG(3,(THIS_FILE,
                  "Intercom call became active during reopen guard - "
                  "not sending START_CALL reason=%s",
                  reason ? reason : "unknown"));
        goto out;
    }

    if (intercom_send_command(INTERCOM_CMD_START_CALL) != 0) {
        PJ_LOG(2,(THIS_FILE, "Failed to send START_CALL reason=%s",
                  reason ? reason : "unknown"));
        rc = -1;
        goto out;
    }

    set_call_active_status(PJ_TRUE);
    mqtt_publish_call_active(1);
    prometheus_set_call_active(1);
    rc = 1;

out:
    pthread_mutex_unlock(&intercom_command_mutex);
    return rc;
}

static void clear_intercom_call_state(const char* reason) {
    PJ_LOG(3,(THIS_FILE, "Clearing intercom call state reason=%s",
              reason ? reason : "unknown"));
    simulated_ding_panel_context_active = 0;
    set_call_active_status(PJ_FALSE);
    mqtt_publish_call_active(0);
    prometheus_set_call_active(0);
}

static int close_intercom_call(const char* reason) {
    int rc;

    pthread_mutex_lock(&intercom_command_mutex);
    rc = intercom_send_command(INTERCOM_CMD_STOP_CALL);
    if (rc != 0) {
        PJ_LOG(2,(THIS_FILE, "Failed to send STOP_CALL reason=%s",
                  reason ? reason : "unknown"));
    } else {
        record_intercom_close(reason);
    }

    clear_intercom_call_state(reason);
    pthread_mutex_unlock(&intercom_command_mutex);
    return rc == 0 ? 0 : -1;
}


static void mqtt_open_door_callback(void* user_data) {
    int started_panel_context;
    (void)user_data;

    if (sip_calling_is_call_active()) {
        unlock_door("mqtt");
        return;
    }

    printf("MQTT open door requested without active call; starting panel context\n");
    started_panel_context = ensure_intercom_call_open("mqtt-open-door");
    if (started_panel_context < 0) {
        return;
    }
    usleep(500000);
    unlock_door("mqtt");
    if (started_panel_context > 0) {
        usleep(1000000);
        close_intercom_call("mqtt-open-door");
    }
}

static void mqtt_trigger_f1_callback(void* user_data) {
    (void)user_data;

    printf("MQTT F1 function requested\n");
    if (intercom_send_command(INTERCOM_CMD_F1_ON) == 0) {
        prometheus_inc_uart_f1();
        usleep(500000);
        intercom_send_command(INTERCOM_CMD_F1_OFF);
    } else {
        printf("Failed to trigger F1 function\n");
    }
}

static void mqtt_reboot_device_callback(void* user_data) {
    int already_requested;

    (void)user_data;
    pthread_mutex_lock(&reboot_mutex);
    already_requested = reboot_requested;
    reboot_requested = 1;
    pthread_mutex_unlock(&reboot_mutex);

    if (already_requested) {
        printf("MQTT device reboot already requested, ignoring duplicate callback\n");
        return;
    }

    printf("MQTT device reboot accepted; syncing filesystems\n");
    sync();
    if (reboot(RB_AUTOBOOT) != 0) {
        printf("MQTT device reboot failed: %s\n", strerror(errno));
    }
}

static void mqtt_take_snapshot_callback(void* user_data) {
    (void)user_data;
    start_snapshot_capture(1, 0, "manual", 0);
}

static void mqtt_simulate_ding_callback(void* user_data) {
    int fd;
    char message[64];
    size_t len;
    (void)user_data;

    snprintf(message, sizeof(message), "%s\n",
             app_config.ding_message[0] ? app_config.ding_message : "DING");
    len = strlen(message);

    fd = open(app_config.sip_listen_pipe, O_WRONLY | O_NONBLOCK);
    if (fd < 0) {
        PJ_LOG(2,(THIS_FILE, "Developer simulate ding failed to open %s: %s",
                  app_config.sip_listen_pipe, strerror(errno)));
        return;
    }

    if (write(fd, message, len) != (ssize_t)len) {
        PJ_LOG(2,(THIS_FILE, "Developer simulate ding failed to write %s: %s",
                  app_config.sip_listen_pipe, strerror(errno)));
    } else {
        PJ_LOG(3,(THIS_FILE, "Developer simulate ding wrote '%s' to %s",
                  app_config.ding_message, app_config.sip_listen_pipe));
    }
    close(fd);
}

static void mqtt_simulate_handset_answered_callback(void* user_data) {
    static const unsigned char frame[4] = {0xFB, 0x23, 0x00, 0x2E};
    int close_simulated_panel;
    (void)user_data;

    if (!ensure_pj_thread_registered("mqtt_handset")) {
        return;
    }

    close_simulated_panel = simulated_ding_panel_context_active;
    PJ_LOG(3,(THIS_FILE,
              "Developer simulate handset answered - injecting FB 23 00 2E"));
    handle_uart_frame(frame);

    if (close_simulated_panel) {
        PJ_LOG(3,(THIS_FILE,
                  "Closing panel context opened by developer simulated DING"));
        mute_audio_input_for_ms(app_config.audio_line_mute_ms,
                                "simulated-handset-stop");
        close_intercom_call("developer-simulated-handset-answered");
    }
}

static void publish_snapshot_button_availability(void) {
    int busy;

    pthread_mutex_lock(&snapshot_mutex);
    busy = snapshot_active;
    pthread_mutex_unlock(&snapshot_mutex);

    mqtt_publish_snapshot_available(app_config.video_enabled && !busy);
}

static int start_snapshot_capture(int open_panel_context, unsigned int delay_ms,
                                  const char* reason, int start_rtsp_after) {
    pthread_t thread;
    snapshot_request_t* request;

    if (!app_config.video_enabled) {
        printf("Snapshot ignored for %s - video disabled\n",
               reason ? reason : "unknown");
        mqtt_publish_snapshot_available(0);
        return -1;
    }

    pthread_mutex_lock(&snapshot_mutex);
    if (snapshot_active) {
        pthread_mutex_unlock(&snapshot_mutex);
        mqtt_publish_snapshot_available(0);
        printf("Snapshot ignored for %s - capture already active\n",
               reason ? reason : "unknown");
        return -1;
    }
    snapshot_active = 1;
    pthread_mutex_unlock(&snapshot_mutex);
    mqtt_publish_snapshot_available(0);

    request = calloc(1, sizeof(*request));
    if (!request) {
        pthread_mutex_lock(&snapshot_mutex);
        snapshot_active = 0;
        pthread_mutex_unlock(&snapshot_mutex);
        publish_snapshot_button_availability();
        printf("Failed to allocate snapshot request\n");
        return -1;
    }
    request->open_panel_context = open_panel_context ? 1 : 0;
    request->delay_ms = delay_ms;
    request->reason = reason ? reason : "unknown";
    request->start_rtsp_after = start_rtsp_after ? 1 : 0;

    if (pthread_create(&thread, NULL, snapshot_thread_func, request) != 0) {
        pthread_mutex_lock(&snapshot_mutex);
        snapshot_active = 0;
        pthread_mutex_unlock(&snapshot_mutex);
        publish_snapshot_button_availability();
        free(request);
        printf("Failed to create snapshot thread\n");
        return -1;
    }
    pthread_detach(thread);
    return 0;
}

static void* snapshot_thread_func(void* arg) {
    snapshot_request_t request;
    int started_panel_context = 0;
    int status = 1;
    pid_t pid;
    int child_status = 0;
    pj_thread_desc desc = {0};
    pj_thread_t *thread;
    pj_status_t pj_status;

    request.open_panel_context = 1;
    request.delay_ms = 0;
    request.reason = "manual";
    request.start_rtsp_after = 0;
    if (arg) {
        request = *(snapshot_request_t*)arg;
        free(arg);
    }

    pj_status = pj_thread_register("snapshot_thread", desc, &thread);
    if (pj_status != PJ_SUCCESS) {
        printf("Failed to register snapshot thread with PJLIB: %d\n", pj_status);
        goto done;
    }

    if (request.delay_ms > 0) {
        printf("Snapshot %s waiting %u ms before capture\n",
               request.reason ? request.reason : "unknown", request.delay_ms);
        usleep(request.delay_ms * 1000);
    }

    unlink(SNAPSHOT_PATH);

    if (!get_call_active_status()) {
        if (request.open_panel_context) {
            printf("Snapshot starting temporary panel context\n");
            started_panel_context = ensure_intercom_call_open("snapshot");
            if (started_panel_context < 0) {
                goto done;
            }
            if (started_panel_context > 0) {
                usleep(500000);
            }
        } else {
            printf("Snapshot %s capturing without temporary panel context\n",
                   request.reason ? request.reason : "unknown");
        }
    }

    if (video_bridge_pid <= 0 && app_config.video_enabled &&
        app_config.rtsp_enabled && rtsp_stream_get_video_client_count() > 0) {
        start_rtsp_preview_session("snapshot");
        usleep(300000);
    }

    if (video_bridge_pid > 0) {
        printf("Snapshot requesting active video worker pid=%d\n", video_bridge_pid);
        if (request_video_worker_snapshot(SNAPSHOT_PATH, 90) == 0 &&
            wait_for_snapshot_file(SNAPSHOT_PATH, 5000) == 0) {
            if (mqtt_publish_snapshot_file(SNAPSHOT_PATH) == 0) {
                status = 0;
            }
        } else {
            printf("Snapshot worker-control request failed or timed out\n");
        }
        goto stop_context;
    }

    pid = fork();
    if (pid < 0) {
        printf("Snapshot fork failed: %s\n", strerror(errno));
        goto stop_context;
    }
    if (pid == 0) {
        int log_fd = open(SNAPSHOT_LOG_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (log_fd >= 0) {
            dup2(log_fd, STDOUT_FILENO);
            dup2(log_fd, STDERR_FILENO);
            close(log_fd);
        }
        _exit(video_snapshot_capture(SNAPSHOT_PATH, 90));
    }

    if (waitpid(pid, &child_status, 0) == pid &&
        WIFEXITED(child_status) && WEXITSTATUS(child_status) == 0 &&
        access(SNAPSHOT_PATH, R_OK) == 0) {
        if (mqtt_publish_snapshot_file(SNAPSHOT_PATH) == 0) {
            status = 0;
        }
    } else {
        printf("Snapshot worker failed status=0x%x\n", child_status);
    }

stop_context:
    if (started_panel_context) {
        if (sip_calling_is_call_active()) {
            printf("Snapshot leaving panel context active because SIP call is active\n");
        } else {
            close_intercom_call("snapshot");
        }
    }

done:
    pthread_mutex_lock(&snapshot_mutex);
    snapshot_active = 0;
    pthread_mutex_unlock(&snapshot_mutex);
    publish_snapshot_button_availability();
    if (request.start_rtsp_after && app_config.rtsp_enabled && app_config.video_enabled &&
        rtsp_stream_get_video_client_count() > 0 && video_bridge_pid <= 0) {
        start_rtsp_preview_session("ring-after-snapshot");
    }
    printf("Snapshot complete status=%d\n", status);
    return NULL;
}

static void mqtt_set_video_enabled_callback(int enabled, void* user_data) {
    (void)user_data;

    if (!ensure_pj_thread_registered("mqtt_callback")) {
        return;
    }

    app_config.video_enabled = enabled ? 1 : 0;
    printf("MQTT video_enabled set to %d\n", app_config.video_enabled);
    sip_calling_set_video_config(app_config.video_enabled ? app_config.video_rtp_port : 0,
                                 app_config.video_payload_type);
    mqtt_publish_video_enabled(app_config.video_enabled);
    prometheus_set_video_enabled(app_config.video_enabled);
    publish_snapshot_button_availability();
    rtsp_stream_set_video_enabled(app_config.video_enabled);
    if (!app_config.video_enabled) {
        stop_video_session();
        maybe_stop_audio_engine("video-disabled");
        return;
    }
    if (app_config.rtsp_enabled &&
        rtsp_stream_get_video_client_count() > 0 && video_bridge_pid <= 0) {
        ensure_audio_engine_running("rtsp-client-video-enabled");
        start_rtsp_preview_session("rtsp-client-video-enabled");
    }
}

static void populate_video_encoder_tuning(video_encoder_tuning_t* tuning) {
    if (!tuning) {
        return;
    }

    tuning->gop_n = clamp_video_gop_n(app_config.video_gop_n);
    tuning->idr_interval = clamp_video_idr_interval(app_config.video_idr_interval);
    tuning->brc_mode = clamp_video_brc_mode(app_config.video_brc_mode);
    tuning->rtsp_periodic_idr_ms =
        clamp_video_rtsp_periodic_idr_ms(app_config.video_rtsp_periodic_idr_ms);
}

static void* ringing_timeout_thread_func(void* arg) {
    ringing_timeout_request_t request;
    int is_current;

    if (!arg) {
        return NULL;
    }

    request = *(ringing_timeout_request_t*)arg;
    free(arg);

    if (request.timeout_seconds <= 0) {
        return NULL;
    }

    sleep((unsigned int)request.timeout_seconds);

    pthread_mutex_lock(&ringing_timeout_mutex);
    is_current = request.generation == ringing_timeout_generation;
    pthread_mutex_unlock(&ringing_timeout_mutex);

    if (!is_current || sip_calling_is_call_active()) {
        return NULL;
    }

    if (!ensure_pj_thread_registered("ring_timeout")) {
        return NULL;
    }

    printf("Ring timeout after %d seconds - returning media_state to idle\n",
           request.timeout_seconds);
    if (simulated_ding_panel_context_active) {
        close_intercom_call("ring-timeout-simulated-ding");
    }
    mqtt_publish_ringing(0);
    mqtt_publish_media_state("idle");
    call_trace_record("timeout", "idle", "none", "ring-timeout", 1);
    prometheus_set_ringing(0);
    return NULL;
}

static void invalidate_ringing_timeout(const char* reason) {
    pthread_mutex_lock(&ringing_timeout_mutex);
    ringing_timeout_generation++;
    pthread_mutex_unlock(&ringing_timeout_mutex);
    PJ_LOG(3,(THIS_FILE, "Cancelled pending ringing timeout reason=%s",
              reason ? reason : "unknown"));
}

static void schedule_ringing_timeout(int timeout_seconds) {
    pthread_t thread;
    ringing_timeout_request_t* request;

    timeout_seconds = clamp_outgoing_call_timeout(timeout_seconds);
    request = calloc(1, sizeof(*request));
    if (!request) {
        printf("Failed to allocate ringing timeout request\n");
        return;
    }

    pthread_mutex_lock(&ringing_timeout_mutex);
    ringing_timeout_generation++;
    request->generation = ringing_timeout_generation;
    pthread_mutex_unlock(&ringing_timeout_mutex);
    request->timeout_seconds = timeout_seconds;

    if (pthread_create(&thread, NULL, ringing_timeout_thread_func, request) != 0) {
        printf("Failed to create ringing timeout thread\n");
        free(request);
        return;
    }
    pthread_detach(thread);
}

static void mqtt_set_video_bitrate_callback(int bitrate_kbps, void* user_data) {
    (void)user_data;
    app_config.video_bitrate_kbps = clamp_video_bitrate(bitrate_kbps);
    printf("MQTT video_bitrate_kbps set to %d\n", app_config.video_bitrate_kbps);
    mqtt_publish_video_bitrate(app_config.video_bitrate_kbps);
}

static void mqtt_set_sip_outgoing_call_enabled_callback(int enabled, void* user_data) {
    (void)user_data;

    app_config.sip_outgoing_call_enabled = enabled ? 1 : 0;
    printf("MQTT sip_outgoing_call_enabled set to %d\n",
           app_config.sip_outgoing_call_enabled);
    mqtt_publish_sip_outgoing_call_enabled(app_config.sip_outgoing_call_enabled);
}

static void mqtt_set_hangup_on_door_unlock_callback(int enabled, void* user_data) {
    (void)user_data;

    app_config.hangup_on_door_unlock = enabled ? 1 : 0;
    printf("MQTT hangup_on_door_unlock set to %d\n",
           app_config.hangup_on_door_unlock);
    mqtt_publish_hangup_on_door_unlock(app_config.hangup_on_door_unlock);
}

static void mqtt_set_outgoing_call_target_callback(const char* target_uri, void* user_data) {
    char normalized[sizeof(app_config.outgoing_call_target)];
    pj_status_t status;
    (void)user_data;

    if (!ensure_pj_thread_registered("mqtt_callback")) {
        return;
    }

    if (normalize_sip_target_uri(target_uri, normalized, sizeof(normalized)) != 0) {
        printf("MQTT outgoing_call_target rejected: invalid SIP URI\n");
        mqtt_publish_outgoing_call_target(app_config.outgoing_call_target);
        return;
    }

    status = sip_calling_set_target_uri(normalized);
    if (status != PJ_SUCCESS) {
        printf("MQTT outgoing_call_target rejected: SIP core busy status=%d\n", status);
        mqtt_publish_outgoing_call_target(app_config.outgoing_call_target);
        return;
    }

    strncpy(app_config.outgoing_call_target, normalized,
            sizeof(app_config.outgoing_call_target) - 1);
    app_config.outgoing_call_target[sizeof(app_config.outgoing_call_target) - 1] = '\0';
    printf("MQTT outgoing_call_target set to %s\n", app_config.outgoing_call_target);
    mqtt_publish_outgoing_call_target(app_config.outgoing_call_target);
}

static void mqtt_set_outgoing_call_timeout_callback(int timeout_seconds, void* user_data) {
    (void)user_data;

    if (!ensure_pj_thread_registered("mqtt_callback")) {
        return;
    }

    app_config.outgoing_call_timeout = clamp_outgoing_call_timeout(timeout_seconds);
    printf("MQTT outgoing_call_timeout set to %d\n", app_config.outgoing_call_timeout);
    sip_calling_set_call_timeout(app_config.outgoing_call_timeout);
    mqtt_publish_outgoing_call_timeout(app_config.outgoing_call_timeout);
}

static void mqtt_set_ring_snapshot_delay_callback(int delay_ms, void* user_data) {
    (void)user_data;
    app_config.ring_snapshot_delay_ms = clamp_ring_snapshot_delay(delay_ms);
    printf("MQTT ring_snapshot_delay_ms set to %d\n", app_config.ring_snapshot_delay_ms);
    mqtt_publish_ring_snapshot_delay(app_config.ring_snapshot_delay_ms);
}

static void mqtt_set_call_forward_enabled_callback(int enabled, void* user_data) {
    intercom_cmd_t cmd = enabled ? INTERCOM_CMD_ENABLE_PUSH_STATE : INTERCOM_CMD_DISABLE_PUSH_STATE;
    (void)user_data;

    if (intercom_send_command(cmd) == 0) {
        printf("MQTT call_forward_enabled set to %d\n", enabled ? 1 : 0);
        mqtt_publish_call_forward_enabled(enabled ? 1 : 0);
    } else {
        printf("Failed to set call_forward_enabled to %d\n", enabled ? 1 : 0);
    }
}

static void mqtt_set_rtsp_enabled_callback(int enabled, void* user_data) {
    (void)user_data;

    if (!ensure_pj_thread_registered("mqtt_callback")) {
        return;
    }

    app_config.rtsp_enabled = enabled ? 1 : 0;
    printf("MQTT rtsp_enabled set to %d\n", app_config.rtsp_enabled);
    mqtt_publish_rtsp_enabled(app_config.rtsp_enabled);

    if (app_config.rtsp_enabled) {
        start_rtsp_service();
        return;
    }

    stop_rtsp_service();
}

static void handle_audio_test_control(const char* message) {
    char ip[64];
    int port;
    int seconds;
    int started_panel_context;

    if (sscanf(message, "%63s %d %d", ip, &port, &seconds) != 3 ||
        port <= 0 || seconds <= 0 || seconds > 30) {
        PJ_LOG(2,(THIS_FILE, "Invalid AUDIO_TEST command: '%s'", message));
        return;
    }

    if (get_audio_sip_rtp_active()) {
        PJ_LOG(2,(THIS_FILE, "AUDIO_TEST ignored - SIP audio target already active"));
        return;
    }

    PJ_LOG(3,(THIS_FILE, "AUDIO_TEST starting to %s:%d for %d seconds",
              ip, port, seconds));
    started_panel_context = ensure_intercom_call_open("audio-test");
    if (started_panel_context < 0) {
        return;
    }
    if (started_panel_context > 0) {
        usleep(500000);
    }
    start_audio_session(ip, port);
    sleep((unsigned)seconds);
    stop_audio_session();
    if (started_panel_context > 0) {
        close_intercom_call("audio-test");
    }
    PJ_LOG(3,(THIS_FILE, "AUDIO_TEST complete"));
}

static void handle_video_test_control(const char* message) {
    char ip[64];
    int port;
    int seconds;
    int started_panel_context;

    if (sscanf(message, "%63s %d %d", ip, &port, &seconds) != 3 ||
        port <= 0 || seconds <= 0 || seconds > 30) {
        PJ_LOG(2,(THIS_FILE, "Invalid VIDEO_TEST command: '%s'", message));
        return;
    }

    if (video_bridge_pid > 0) {
        PJ_LOG(2,(THIS_FILE, "VIDEO_TEST ignored - video worker already running"));
        return;
    }

    PJ_LOG(3,(THIS_FILE, "VIDEO_TEST starting to %s:%d for %d seconds",
              ip, port, seconds));
    started_panel_context = ensure_intercom_call_open("video-test");
    if (started_panel_context < 0) {
        return;
    }
    if (started_panel_context > 0) {
        usleep(500000);
    }
    start_video_session(ip, port);
    sleep((unsigned)seconds);
    stop_video_session();
    if (started_panel_context > 0) {
        close_intercom_call("video-test");
    }
    PJ_LOG(3,(THIS_FILE, "VIDEO_TEST complete"));
}

static void handle_simulated_ding_trigger(void) {
    int start_result;

    PJ_LOG(3,(THIS_FILE, "Simulated DING - opening panel context before native ring path"));
    mute_audio_input_for_ms(app_config.audio_line_mute_ms, "simulated-ding-start");
    start_result = ensure_intercom_call_open("simulated-ding");
    if (start_result < 0) {
        return;
    }
    if (start_result > 0) {
        simulated_ding_panel_context_active = 1;
    }

    handle_ding_trigger("developer_simulation");
}

static void handle_ding_trigger(const char* source) {
    pj_status_t status;
    int trace_started;
    int is_panel_alarm = source &&
        (strcmp(source, "physical_panel") == 0 ||
         strcmp(source, "developer_simulation") == 0);

    PJ_LOG(3,(THIS_FILE, "DING detected from %s - checking if we can make outgoing call",
              source ? source : "unknown"));

    if (sip_calling_is_call_active()) {
        PJ_LOG(2,(THIS_FILE, "DING ignored - call already active"));
        call_trace_record("ring_ignored", NULL, NULL, "sip-call-active", 0);
        return;
    }

    mqtt_publish_ringing(1);
    mqtt_publish_media_state("ringing");
    trace_started = call_trace_start(source, "ringing", "ringing", "none",
                                     is_panel_alarm ? "alarm-report" : "ding");
    if (trace_started == 0) {
        call_trace_record("ring_repeated", "ringing", NULL,
                          source ? source : "unknown", 0);
    }
    prometheus_set_ringing(1);
    prometheus_inc_ring();
    schedule_ringing_timeout(app_config.outgoing_call_timeout);

    if (is_panel_alarm) {
        start_snapshot_capture(0, (unsigned int)app_config.ring_snapshot_delay_ms, "ring", 1);
    }

    if (!app_config.sip_outgoing_call_enabled) {
        PJ_LOG(3,(THIS_FILE, "Outgoing SIP call disabled - keeping media_state=ringing only"));
        call_trace_record("sip_disabled", "ringing", "none", "outgoing-disabled", 0);
        return;
    }

    // Refresh the advertised local IP right before offering, so a DHCP change
    // since init/last call is reflected in the SDP/Contact rather than sending a
    // stale address that would break media.
    refresh_local_ip();

    PJ_LOG(3,(THIS_FILE, "Making outgoing call due to %s", source ? source : "DING"));
    call_trace_record("sip_calling", "ringing", "sip", "outgoing-call", 0);
    status = sip_calling_make_call();
    if (status != PJ_SUCCESS) {
        PJ_LOG(1,(THIS_FILE, "Failed to make outgoing call: %d", status));
        call_trace_record("sip_call_failed", "ringing", "sip", "make-call-failed", 0);
        if (simulated_ding_panel_context_active) {
            close_intercom_call("simulated-ding-call-failed");
        }
    }
}

static void handle_control_message(const char* message) {
    unsigned char frame[4];

    if (strncmp(message, app_config.ding_message, strlen(app_config.ding_message)) == 0) {
        handle_simulated_ding_trigger();
        return;
    }

    if (strncmp(message, "VIDEO_TEST ", 11) == 0) {
        handle_video_test_control(message + 11);
        return;
    }

    if (strncmp(message, "AUDIO_TEST ", 11) == 0) {
        handle_audio_test_control(message + 11);
        return;
    }

    if (uart_protocol_parse_control_frame(message, frame) == 0) {
        PJ_LOG(3,(THIS_FILE, "Injecting UART frame from control pipe: %02X %02X %02X %02X",
                  frame[0], frame[1], frame[2], frame[3]));
        handle_uart_frame(frame);
        return;
    }

    PJ_LOG(2,(THIS_FILE, "Unknown control pipe message: '%s'", message));
}

// DING monitoring thread
static void* ding_monitor_thread_func(void* arg) {
    (void)arg;
    char buffer[64];
    ssize_t bytes_read;

    // Register this thread with PJLIB
    pj_thread_desc desc = {0};
    pj_thread_t *thread;
    pj_status_t status;

    status = pj_thread_register("ding_monitor_thread", desc, &thread);
    if (status != PJ_SUCCESS) {
        printf("Failed to register DING monitor thread with PJLIB: %d\n", status);
        return NULL;
    }

    PJ_LOG(3,(THIS_FILE, "DING monitor thread started"));

    while (ding_monitoring_active && !quit_flag) {
        // Try to open pipe if not already open
        if (ding_pipe_fd < 0) {
            ding_pipe_fd = open(app_config.sip_listen_pipe, O_RDONLY | O_NONBLOCK);
            if (ding_pipe_fd < 0) {
                if (errno != ENOENT) {  // Don't spam if pipe doesn't exist
                    PJ_LOG(2,(THIS_FILE, "Failed to open DING pipe: %s", strerror(errno)));
                }
                sleep(1);
                continue;
            }
            PJ_LOG(3,(THIS_FILE, "DING pipe opened successfully"));
        }

        // Read from pipe
        bytes_read = read(ding_pipe_fd, buffer, sizeof(buffer) - 1);

        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            // Remove newline if present
            if (buffer[bytes_read-1] == '\n') buffer[bytes_read-1] = '\0';

            PJ_LOG(3,(THIS_FILE, "Received DING pipe message: '%s'", buffer));

            handle_control_message(buffer);
        } else if (bytes_read == 0) {
            // EOF - no writers, keep pipe open
            usleep(100000);  // 100ms sleep
        } else {
            // Error occurred
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No data available
                usleep(100000);  // 100ms sleep
            } else {
                PJ_LOG(2,(THIS_FILE, "DING pipe read error: %s", strerror(errno)));
                close(ding_pipe_fd);
                ding_pipe_fd = -1;
                sleep(1);
            }
        }
    }

    if (ding_pipe_fd >= 0) {
        close(ding_pipe_fd);
        ding_pipe_fd = -1;
    }

    PJ_LOG(3,(THIS_FILE, "DING monitor thread stopped"));
    return NULL;
}

static int start_ding_monitoring(void) {
    if (ding_monitoring_active) {
        return 0;  // Already running
    }

    // Create pipe if it doesn't exist
    if (mkfifo(app_config.sip_listen_pipe, 0666) < 0 && errno != EEXIST) {
        PJ_LOG(2,(THIS_FILE, "Failed to create DING pipe: %s", strerror(errno)));
        return -1;
    }

    ding_monitoring_active = PJ_TRUE;

    if (pthread_create(&ding_monitor_thread, NULL, ding_monitor_thread_func, NULL) != 0) {
        PJ_LOG(1,(THIS_FILE, "Failed to create DING monitor thread"));
        ding_monitoring_active = PJ_FALSE;
        return -1;
    }

    PJ_LOG(3,(THIS_FILE, "DING monitoring started"));
    return 0;
}

static void stop_ding_monitoring(void) {
    if (!ding_monitoring_active) {
        return;
    }

    PJ_LOG(3,(THIS_FILE, "Stopping DING monitoring"));
    ding_monitoring_active = PJ_FALSE;

    // Close pipe to wake up thread
    if (ding_pipe_fd >= 0) {
        close(ding_pipe_fd);
        ding_pipe_fd = -1;
    }

    pthread_join(ding_monitor_thread, NULL);
    PJ_LOG(3,(THIS_FILE, "DING monitoring stopped"));
}

static void report_alarm_event(int event_id) {
    FILE* fp = fopen("/mnt/mtd/alarm.log", "a");

    if (!fp) {
        PJ_LOG(2,(THIS_FILE, "Failed to append alarm log: %s", strerror(errno)));
        return;
    }
    fprintf(fp, "%ld,%d\n", (long)time(NULL), event_id);
    fclose(fp);
}

static void terminate_call_from_serial(const char* reason) {
    if (!sip_calling_is_call_active()) {
        return;
    }

    PJ_LOG(3,(THIS_FILE, "Serial %s received - terminating SIP call", reason));
    sip_calling_terminate_call();
}

static void request_wifi_ap_mode(void) {
    static const char marker[] = "requested\n";
    int fd = open(WIFI_AP_REQUEST_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0600);

    if (fd < 0) {
        PJ_LOG(2,(THIS_FILE, "Cannot persist AP request %s: %s",
                  WIFI_AP_REQUEST_PATH, strerror(errno)));
        return;
    }
    if (write(fd, marker, sizeof(marker) - 1) != (ssize_t)(sizeof(marker) - 1) ||
        fsync(fd) != 0) {
        PJ_LOG(2,(THIS_FILE, "Cannot commit AP request %s: %s",
                  WIFI_AP_REQUEST_PATH, strerror(errno)));
        close(fd);
        return;
    }
    close(fd);

    PJ_LOG(2,(THIS_FILE, "AP provisioning requested from physical WiFi button"));
    sync();
    if (reboot(RB_AUTOBOOT) != 0) {
        PJ_LOG(2,(THIS_FILE, "AP provisioning reboot failed: %s", strerror(errno)));
    }
}

static void handle_uart_frame(const unsigned char frame[4]) {
    const uart_code_def_t* def = uart_protocol_find(frame);

    if (!def) {
        prometheus_inc_uart_unknown_frame();
        PJ_LOG(3,(THIS_FILE, "UART code unknown: %02X %02X %02X %02X",
                  frame[0], frame[1], frame[2], frame[3]));
        mqtt_publish_uart_event("unknown_fb", "UNKNOWN", frame, 4, (int)frame[2], 0);
        return;
    }

    prometheus_inc_uart_frame();
    PJ_LOG(3,(THIS_FILE, "UART code received: %s [%02X %02X %02X %02X]",
              def->name, frame[0], frame[1], frame[2], frame[3]));
    mqtt_publish_uart_event(def->event_type, def->name, frame, 4, (int)frame[2], 1);

    switch (def->code) {
    case UART_CODE_ALARM_REPORT:
        prometheus_inc_uart_alarm_report();
        report_alarm_event(1);
        handle_ding_trigger("physical_panel");
        break;
    case UART_CODE_HANG_UP_0:
    case UART_CODE_HANG_UP_1:
        prometheus_inc_uart_hangup();
        report_alarm_event(2);
        record_intercom_close(def->name);
        invalidate_ringing_timeout(def->name);
        clear_intercom_call_state(def->name);
        mqtt_publish_ringing(0);
        mqtt_publish_media_state("idle");
        call_trace_record(def->event_type, "idle", NULL, def->name, 1);
        prometheus_set_ringing(0);
        terminate_call_from_serial(def->name);
        break;
    case UART_CODE_PHYSICAL_HANDSET_ANSWERED:
        prometheus_inc_uart_stop_ring();
        report_alarm_event(3);
        PJ_LOG(3,(THIS_FILE, "Physical handset answered - ending remote ringing state"));
        invalidate_ringing_timeout(def->name);
        clear_intercom_call_state(def->name);
        mqtt_publish_ringing(0);
        mqtt_publish_media_state("idle");
        call_trace_record("physical_handset_answered", "idle",
                          "physical_handset", def->name, 1);
        prometheus_set_ringing(0);
        terminate_call_from_serial(def->name);
        break;
    case UART_CODE_CMD_RESET:
        prometheus_inc_uart_reset();
        PJ_LOG(2,(THIS_FILE, "Reset command received from panel"));
        system("sync && reboot");
        break;
    case UART_CODE_STA_TO_AP:
        /* Retain compatibility with MCU revisions that emit the direct frame. */
        request_wifi_ap_mode();
        break;
    case UART_CODE_START_CALL:
        PJ_LOG(3,(THIS_FILE, "Intercom call line is active"));
        mute_audio_input_for_ms(app_config.audio_line_mute_ms, "uart-start-call");
        set_call_active_status(PJ_TRUE);
        mqtt_publish_call_active(1);
        break;
    case UART_CODE_PUSH_STATE_0:
        prometheus_inc_uart_push_state();
        PJ_LOG(3,(THIS_FILE, "Intercom call forwarding state is inactive"));
        mqtt_publish_call_forward_enabled(0);
        break;
    case UART_CODE_PUSH_STATE_1:
        prometheus_inc_uart_push_state();
        PJ_LOG(3,(THIS_FILE, "Intercom call forwarding state is active"));
        mqtt_publish_call_forward_enabled(1);
        break;
    case UART_CODE_MCU_STATE_0:
    case UART_CODE_MCU_STATE_1:
        break;
    case UART_CODE_CMD_DOWN_LONG_1:
        wifi_button_long_started_ms = now_ms();
        PJ_LOG(3,(THIS_FILE, "Physical WiFi button long-press stage 1 received"));
        break;
    case UART_CODE_CMD_DOWN_LONG_2:
    {
        unsigned long long completed_ms = now_ms();
        unsigned long long elapsed_ms = wifi_button_long_started_ms > 0 &&
                                        completed_ms >= wifi_button_long_started_ms
                                            ? completed_ms - wifi_button_long_started_ms
                                            : WIFI_BUTTON_LONG_SEQUENCE_MAX_MS + 1;
        wifi_button_long_started_ms = 0;
        if (elapsed_ms <= WIFI_BUTTON_LONG_SEQUENCE_MAX_MS) {
            PJ_LOG(2,(THIS_FILE,
                      "Physical WiFi button long press confirmed after %llu ms",
                      elapsed_ms));
            mqtt_publish_uart_event("sta_to_ap", "WIFI_BUTTON_LONG_PRESS",
                                    frame, 4, (int)frame[2], 1);
            request_wifi_ap_mode();
        } else {
            PJ_LOG(3,(THIS_FILE,
                      "Ignoring WiFi button stage 2 without recent stage 1"));
        }
        break;
    }
    default:
        break;
    }
}

static void* serial_monitor_thread_func(void* arg) {
    (void)arg;
    unsigned char frame[4];
    size_t frame_len = 0;
    pj_thread_desc desc = {0};
    pj_thread_t *thread;
    pj_status_t status;

    status = pj_thread_register("serial_monitor_thread", desc, &thread);
    if (status != PJ_SUCCESS) {
        printf("Failed to register serial monitor thread with PJLIB: %d\n", status);
        return NULL;
    }

    PJ_LOG(3,(THIS_FILE, "Serial monitor thread started"));

    while (serial_monitoring_active && !quit_flag) {
        unsigned char buffer[32];
        ssize_t n;
        ssize_t i;

        if (serial_fd < 0) {
            serial_fd = open(app_config.intercom_device, O_RDONLY | O_NONBLOCK | O_NOCTTY);
            if (serial_fd < 0) {
                PJ_LOG(2,(THIS_FILE, "Failed to open %s for serial monitoring: %s",
                          app_config.intercom_device, strerror(errno)));
                sleep(1);
                continue;
            }
            if (configure_serial_raw(serial_fd) < 0) {
                PJ_LOG(2,(THIS_FILE, "Failed to configure %s as raw serial: %s",
                          app_config.intercom_device, strerror(errno)));
                close(serial_fd);
                serial_fd = -1;
                sleep(1);
                continue;
            }
            PJ_LOG(3,(THIS_FILE, "Serial monitor opened %s", app_config.intercom_device));
        }

        n = read(serial_fd, buffer, sizeof(buffer));
        if (n > 0) {
            char raw_hex[128];
            uart_protocol_format_bytes(buffer, (size_t)n, raw_hex, sizeof(raw_hex));
            PJ_LOG(3,(THIS_FILE, "UART raw read %zd bytes: %s", n, raw_hex));
            mqtt_publish_uart_event("raw_read", "RAW_READ", buffer, (size_t)n, -1, 0);
            for (i = 0; i < n; i++) {
                if (frame_len == 0 && buffer[i] != 0xFB) {
                    PJ_LOG(3,(THIS_FILE, "Ignoring UART byte before frame: %02X", buffer[i]));
                    continue;
                }

                frame[frame_len++] = buffer[i];
                if (frame_len == sizeof(frame)) {
                    handle_uart_frame(frame);
                    frame_len = 0;
                }
            }
        } else if (n == 0) {
            usleep(100000);
        } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            usleep(100000);
        } else {
            PJ_LOG(2,(THIS_FILE, "Serial monitor read error: %s", strerror(errno)));
            close(serial_fd);
            serial_fd = -1;
            frame_len = 0;
            sleep(1);
        }
    }

    if (serial_fd >= 0) {
        close(serial_fd);
        serial_fd = -1;
    }

    PJ_LOG(3,(THIS_FILE, "Serial monitor thread stopped"));
    return NULL;
}

static int start_serial_monitoring(void) {
    if (!app_config.serial_listener_enabled) {
        PJ_LOG(3,(THIS_FILE, "Serial monitoring disabled by config"));
        return 0;
    }
    if (serial_monitoring_active) {
        return 0;
    }

    serial_monitoring_active = PJ_TRUE;
    if (pthread_create(&serial_monitor_thread, NULL, serial_monitor_thread_func, NULL) != 0) {
        PJ_LOG(1,(THIS_FILE, "Failed to create serial monitor thread"));
        serial_monitoring_active = PJ_FALSE;
        return -1;
    }

    PJ_LOG(3,(THIS_FILE, "Serial monitoring started"));
    return 0;
}

static int configure_serial_raw(int fd) {
    struct termios tio;

    if (tcgetattr(fd, &tio) < 0) {
        return -1;
    }

    cfmakeraw(&tio);
    tio.c_cflag |= CREAD | CLOCAL;
    tio.c_cc[VMIN] = 1;
    tio.c_cc[VTIME] = 0;

    if (tcsetattr(fd, TCSANOW, &tio) < 0) {
        return -1;
    }

    tcflush(fd, TCIFLUSH);
    return 0;
}

static void stop_serial_monitoring(void) {
    if (!serial_monitoring_active) {
        return;
    }

    PJ_LOG(3,(THIS_FILE, "Stopping serial monitoring"));
    serial_monitoring_active = PJ_FALSE;
    if (serial_fd >= 0) {
        close(serial_fd);
        serial_fd = -1;
    }
    pthread_join(serial_monitor_thread, NULL);
    PJ_LOG(3,(THIS_FILE, "Serial monitoring stopped"));
}

// Generate error audio pattern
static void generate_error_audio(unsigned char* buffer, int size) {
    static int pattern_pos = 0;
    int i;
    // Create a recognizable error pattern: 80 bytes of 0x55, 80 bytes of 0xAA
    // This creates a distinctive alternating pattern in A-Law
    for (i = 0; i < size; i++) {
        if ((pattern_pos / 80) % 2 == 0) {
            buffer[i] = 0x55;  // A-Law value creating a tone
        } else {
            buffer[i] = 0xAA;  // A-Law value creating opposite phase
        }
        pattern_pos = (pattern_pos + 1) % 160;  // Reset every 160 bytes
    }
}

// Signal handler for clean shutdown
static void signal_handler(int sig) {
    PJ_LOG(3,(THIS_FILE, "Signal %d received, shutting down...", sig));

    // Terminate any active call
    sip_calling_terminate_call();

    quit_flag = PJ_TRUE;
}

// Setup RTP socket
static int setup_rtp_socket(void) {
    struct sockaddr_in local_addr;
    int sock;
    int reuse = 1;

    // Create UDP socket for audio
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        printf("Failed to create RTP socket: %s\n", strerror(errno));
        return -1;
    }

    // Set socket options for better recovery
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        printf("Warning: Failed to set SO_REUSEADDR on RTP socket: %s\n", strerror(errno));
        // Continue anyway
    }

    // Bind to configured RTP port on all interfaces
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = INADDR_ANY;  // Listen on all IPs
    local_addr.sin_port = htons(app_config.rtp_port);

    if (bind(sock, (struct sockaddr*)&local_addr, sizeof(local_addr)) < 0) {
        printf("Failed to bind RTP socket to port %d: %s\n", app_config.rtp_port, strerror(errno));
        close(sock);
        return -1;
    }

    // Get current local IP for logging
    char current_ip[16];
    get_local_ip(current_ip, sizeof(current_ip));
    printf("RTP socket bound to port %d (local IP: %s)\n", app_config.rtp_port, current_ip);
    return sock;
}

// Separate AI thread (microphone to network)
static void* audio_input_handler(void* arg) {
    (void)arg;
    unsigned char *audio_buffer;
    unsigned char *rtp_packet;
    ssize_t bytes_read;
    uint16_t seq_num = 0;                               // RTP sequence number
    uint32_t timestamp = 0;                             // RTP timestamp
    int consecutive_errors = 0;
    const int MAX_CONSECUTIVE_ERRORS = 10;
    int rtp_send_failures = 0;
    int packets_sent = 0;
    int bytes_payload_sent = 0;
    time_t last_network_check = time(NULL);

    // Allocate buffers based on configuration
    audio_buffer = malloc(app_config.audio_buffer_size);
    rtp_packet = malloc(app_config.audio_buffer_size + 12);
    if (!audio_buffer || !rtp_packet) {
        PJ_LOG(1,(THIS_FILE, "Failed to allocate audio buffers"));
        free(audio_buffer);
        free(rtp_packet);
        return NULL;
    }

    PJ_LOG(3,(THIS_FILE, "Audio input thread started"));

    while (get_audio_engine_running() && !quit_flag) {
        struct sockaddr_in sip_target;
        int has_sip_target;

        // === AI -> RTP (microphone to network) ===
        bytes_read = audio_hw_get_frame(audio_buffer, app_config.audio_buffer_size);
        if (bytes_read > 0) {
            consecutive_errors = 0;
        } else {
            consecutive_errors++;
            if (consecutive_errors >= MAX_CONSECUTIVE_ERRORS) {
                if (consecutive_errors == MAX_CONSECUTIVE_ERRORS ||
                    consecutive_errors % 100 == 0) {
                    PJ_LOG(1,(THIS_FILE, "Audio hardware input failed repeatedly; sending fallback audio"));
                }
            }
            generate_error_audio(audio_buffer, app_config.audio_buffer_size);
            bytes_read = app_config.audio_buffer_size;
        }

        if (audio_input_is_muted()) {
            memset(audio_buffer, 0xD5, (size_t)bytes_read);
        }

        // Always send RTP packet (either real audio or error pattern)
        // Create RTP packet
        // RTP Header (12 bytes)
        rtp_packet[0] = 0x80;  // Version 2, no padding, no extension, no CSRC
        rtp_packet[1] = 0x08;  // Payload type 8 (PCMA/A-Law).
        rtp_packet[2] = (seq_num >> 8) & 0xFF;  // Sequence number high byte
        rtp_packet[3] = seq_num & 0xFF;         // Sequence number low byte
        rtp_packet[4] = (timestamp >> 24) & 0xFF; // Timestamp bytes
        rtp_packet[5] = (timestamp >> 16) & 0xFF;
        rtp_packet[6] = (timestamp >> 8) & 0xFF;
        rtp_packet[7] = timestamp & 0xFF;
        rtp_packet[8] = 0x00;  // SSRC (source identifier)
        rtp_packet[9] = 0x00;
        rtp_packet[10] = 0x00;
        rtp_packet[11] = 0x01;

        memcpy(rtp_packet + 12, audio_buffer, bytes_read);
        rtsp_stream_send_audio_rtp(rtp_packet, (size_t)bytes_read + 12);

        has_sip_target = get_audio_sip_target(&sip_target);
        if (has_sip_target) {
            // Send RTP packet to the active SIP media endpoint.
            ssize_t bytes_sent = sendto(rtp_socket, rtp_packet, bytes_read + 12, 0,
                                (struct sockaddr*)&sip_target, sizeof(sip_target));
            if (bytes_sent < 0) {
                rtp_send_failures++;
                PJ_LOG(2,(THIS_FILE, "Failed to send RTP packet: %s (failure count: %d)", strerror(errno), rtp_send_failures));

                // If we have persistent send failures, check network health
                if (rtp_send_failures >= 5) {
                    time_t now = time(NULL);
                    if (now - last_network_check >= 10) {  // Check every 10 seconds max
                        PJ_LOG(2,(THIS_FILE, "Multiple RTP send failures, checking network health"));
                        if (ensure_rtp_socket_ready() == 0) {
                            PJ_LOG(3,(THIS_FILE, "Network recovery attempted, resetting failure count"));
                            rtp_send_failures = 0;
                        }
                        last_network_check = now;
                    }
                }
            } else {
                packets_sent++;
                bytes_payload_sent += (int)bytes_read;
                // Successful send, reset failure counter
                if (rtp_send_failures > 0) {
                    rtp_send_failures = 0;
                }
            }
        } else if (rtp_send_failures > 0) {
            rtp_send_failures = 0;
        }

        seq_num++;
        timestamp += (uint32_t)bytes_read;
    }

    free(audio_buffer);
    free(rtp_packet);

    PJ_LOG(3,(THIS_FILE, "Audio input thread stopped: sip_packets=%d sip_payload_bytes=%d",
              packets_sent, bytes_payload_sent));
    return NULL;
}

// Separate AO thread (network to speaker)
static void* audio_output_handler(void* arg) {
    (void)arg;
    unsigned char *rtp_packet;
    ssize_t bytes_read;
    int logged_packets = 0;
    int packets_received = 0;
    int payload_bytes_played = 0;

    // Allocate buffer based on configuration
    rtp_packet = malloc(app_config.audio_buffer_size + 12);
    if (!rtp_packet) {
        PJ_LOG(1,(THIS_FILE, "Failed to allocate RTP packet buffer"));
        return NULL;
    }

    PJ_LOG(3,(THIS_FILE, "Audio output thread started"));

    while (get_audio_engine_running() && !quit_flag) {
        // === RTP → AO (Network to speaker) ===
        struct sockaddr_in from_addr;
        socklen_t from_len = sizeof(from_addr);
        int sip_active = get_audio_sip_rtp_active();

        bytes_read = recvfrom(rtp_socket, rtp_packet, app_config.audio_buffer_size + 12, MSG_DONTWAIT,
                             (struct sockaddr*)&from_addr, &from_len);

        if (bytes_read > 12) {
            unsigned char payload_type = rtp_packet[1] & 0x7F;
            if (!sip_active) {
                usleep(10000);
                continue;
            }
            if (logged_packets < 40 || payload_type == current_dtmf_payload_type) {
                PJ_LOG(3,(THIS_FILE, "Incoming RTP audio: pt=%u len=%d from=%s:%u dtmf_pt=%d",
                          payload_type, (int)bytes_read,
                          inet_ntoa(from_addr.sin_addr), ntohs(from_addr.sin_port),
                          current_dtmf_payload_type));
                logged_packets++;
            }
            if (parse_rtp_dtmf_event(rtp_packet, bytes_read)) {
                continue;
            }

            if (audio_hw_send_frame(rtp_packet + 12, (size_t)(bytes_read - 12)) < 0) {
                PJ_LOG(2,(THIS_FILE, "Audio hardware output send failed"));
            } else {
                packets_received++;
                payload_bytes_played += (int)(bytes_read - 12);
            }
        }

        usleep(sip_active ? 1000 : 10000);
    }

    free(rtp_packet);

    PJ_LOG(3,(THIS_FILE, "Audio output thread stopped: packets=%d payload_bytes=%d",
              packets_received, payload_bytes_played));
    return NULL;
}

static int ensure_audio_engine_running(const char* reason) {
    pj_status_t status;

    pthread_mutex_lock(&audio_lifecycle_mutex);
    if (get_audio_engine_running()) {
        pthread_mutex_unlock(&audio_lifecycle_mutex);
        return 0;
    }

    if (!pool) {
        PJ_LOG(2,(THIS_FILE, "Audio engine not started for %s: PJ pool is not ready",
                  reason ? reason : "unknown"));
        pthread_mutex_unlock(&audio_lifecycle_mutex);
        return -1;
    }

    PJ_LOG(3,(THIS_FILE, "Starting audio engine reason=%s",
              reason ? reason : "unknown"));

    if (audio_hw_start(app_config.audio_chip_gpio, app_config.audio_buffer_size,
                       app_config.audio_input_gain_percent,
                       app_config.audio_output_volume_percent) < 0) {
        PJ_LOG(1,(THIS_FILE, "Failed to start audio hardware"));
        pthread_mutex_unlock(&audio_lifecycle_mutex);
        return -1;
    }

    set_audio_engine_running(1);

    audio_input_thread = NULL;
    audio_output_thread = NULL;

    // Create audio input thread using PJLIB
    status = pj_thread_create(pool, "audio_input",
                             (pj_thread_proc*)audio_input_handler, NULL,
                             PJ_THREAD_DEFAULT_STACK_SIZE, 0, &audio_input_thread);
    if (status != PJ_SUCCESS) {
        PJ_LOG(1,(THIS_FILE, "Failed to create audio input thread: %d", status));
        set_audio_engine_running(0);
        audio_hw_stop();
        pthread_mutex_unlock(&audio_lifecycle_mutex);
        return -1;
    }

    // Create audio output thread using PJLIB
    status = pj_thread_create(pool, "audio_output",
                             (pj_thread_proc*)audio_output_handler, NULL,
                             PJ_THREAD_DEFAULT_STACK_SIZE, 0, &audio_output_thread);
    if (status != PJ_SUCCESS) {
        PJ_LOG(1,(THIS_FILE, "Failed to create audio output thread: %d", status));
        set_audio_engine_running(0);
        if (audio_input_thread) {
            pj_thread_join(audio_input_thread);
            pj_thread_destroy(audio_input_thread);
            audio_input_thread = NULL;
        }
        audio_hw_stop();
        pthread_mutex_unlock(&audio_lifecycle_mutex);
        return -1;
    }

    PJ_LOG(3,(THIS_FILE, "Audio engine started reason=%s",
              reason ? reason : "unknown"));
    pthread_mutex_unlock(&audio_lifecycle_mutex);
    return 0;
}

static void stop_audio_engine(const char* reason) {
    pthread_mutex_lock(&audio_lifecycle_mutex);
    if (!get_audio_engine_running() && !audio_input_thread && !audio_output_thread) {
        pthread_mutex_unlock(&audio_lifecycle_mutex);
        return;
    }

    PJ_LOG(3,(THIS_FILE, "Stopping audio engine reason=%s",
              reason ? reason : "unknown"));

    clear_audio_sip_target();
    set_audio_engine_running(0);

    // If quit_flag is set, give audio thread time to send goodbye packets
    if (quit_flag) {
        PJ_LOG(3,(THIS_FILE, "Waiting for goodbye packets to be sent..."));
        usleep(100000); // 100ms should be enough for 3 packets at 10ms intervals
    }

    // Wait for threads to complete using PJLIB functions
    if (audio_input_thread) {
        pj_thread_join(audio_input_thread);
        pj_thread_destroy(audio_input_thread);
        audio_input_thread = NULL;
    }

    if (audio_output_thread) {
        pj_thread_join(audio_output_thread);
        pj_thread_destroy(audio_output_thread);
        audio_output_thread = NULL;
    }

    audio_hw_stop();
    pthread_mutex_unlock(&audio_lifecycle_mutex);
}

static void maybe_stop_audio_engine(const char* reason) {
    if (get_audio_sip_rtp_active()) {
        return;
    }
    if (app_config.rtsp_enabled && rtsp_stream_get_audio_client_count() > 0) {
        return;
    }
    stop_audio_engine(reason);
}

// Attach SIP RTP to the shared audio engine.
static void start_audio_session(const char* remote_ip, int remote_port) {
    const sip_call_session_t *session;
    struct sockaddr_in target;

    if (!remote_ip || !remote_ip[0] || remote_port <= 0) {
        PJ_LOG(2,(THIS_FILE, "Invalid SIP audio target %s:%d",
                  remote_ip ? remote_ip : "-", remote_port));
        return;
    }

    PJ_LOG(3,(THIS_FILE, "Attaching SIP audio target %s:%d", remote_ip, remote_port));

    session = sip_calling_get_session();
    current_dtmf_payload_type = RTP_PAYLOAD_DTMF;
    if (session && session->remote_dtmf_payload_type > 0) {
        current_dtmf_payload_type = session->remote_dtmf_payload_type;
    }
    PJ_LOG(3,(THIS_FILE, "Using DTMF RTP payload type %d", current_dtmf_payload_type));

    memset(&target, 0, sizeof(target));
    target.sin_family = AF_INET;
    target.sin_port = htons(remote_port);
    if (inet_pton(AF_INET, remote_ip, &target.sin_addr) != 1) {
        PJ_LOG(2,(THIS_FILE, "Invalid SIP audio RTP IP: %s", remote_ip));
        return;
    }

    pthread_mutex_lock(&audio_state_mutex);
    remote_rtp_addr = target;
    audio_sip_rtp_active = 1;
    pthread_mutex_unlock(&audio_state_mutex);

    if (ensure_audio_engine_running("sip") < 0) {
        clear_audio_sip_target();
        return;
    }

    PJ_LOG(3,(THIS_FILE, "SIP audio target attached %s:%d", remote_ip, remote_port));
}

// Detach SIP RTP from the shared audio engine.
static void stop_audio_session(void) {
    if (!get_audio_sip_rtp_active()) {
        maybe_stop_audio_engine("sip-no-target");
        return;
    }

    PJ_LOG(3,(THIS_FILE, "Detaching SIP audio target"));
    clear_audio_sip_target();
    maybe_stop_audio_engine("sip-ended");
}

// Handle incoming SIP responses
static pj_bool_t on_rx_response(pjsip_rx_data *rdata) {
    // Let unified SIP calling module handle it
    return sip_calling_handle_response(rdata);
}

// Handle incoming SIP requests
static pj_bool_t on_rx_request(pjsip_rx_data *rdata) {
    pjsip_method *method = &rdata->msg_info.msg->line.req.method;

    if (method->name.slen == 7 &&
        strncmp(method->name.ptr, "OPTIONS", 7) == 0) {
        PJ_LOG(4,(THIS_FILE, "SIP OPTIONS keepalive from %s:%d",
                  pj_inet_ntoa(rdata->pkt_info.src_addr.ipv4.sin_addr),
                  pj_ntohs(rdata->pkt_info.src_addr.ipv4.sin_port)));
        pjsip_endpt_respond_stateless(sip_endpt, rdata, 200, NULL, NULL, NULL);
        return PJ_TRUE;
    }

    // Add detailed logging - simplified
    PJ_LOG(3,(THIS_FILE, "=== SIP REQUEST RECEIVED ==="));
    PJ_LOG(3,(THIS_FILE, "Method: %.*s", method->name.slen, method->name.ptr));
    PJ_LOG(3,(THIS_FILE, "From IP: %s", pj_inet_ntoa(rdata->pkt_info.src_addr.ipv4.sin_addr)));
    PJ_LOG(3,(THIS_FILE, "Source Port: %d", pj_ntohs(rdata->pkt_info.src_addr.ipv4.sin_port)));

    if (pjsip_method_cmp(method, pjsip_get_invite_method()) == 0) {
        PJ_LOG(3,(THIS_FILE, "Processing INVITE request"));

        // Use unified module to handle incoming INVITE
        pj_status_t status = sip_calling_handle_incoming_invite(rdata);
        if (status != PJ_SUCCESS && status != PJ_EBUSY) {
            // If not busy, send 500 Server Internal Error
            pjsip_endpt_respond_stateless(sip_endpt, rdata, 500, NULL, NULL, NULL);
        }

    } else if (pjsip_method_cmp(method, pjsip_get_bye_method()) == 0) {
        PJ_LOG(3,(THIS_FILE, "Processing BYE request"));

        // Use unified module to handle BYE
        sip_calling_handle_incoming_bye(rdata);

    } else if (pjsip_method_cmp(method, pjsip_get_ack_method()) == 0) {
        PJ_LOG(3,(THIS_FILE, "Processing ACK request"));

        // Use unified module to handle ACK
        sip_calling_handle_incoming_ack(rdata);

    } else if (pjsip_method_cmp(method, pjsip_get_cancel_method()) == 0) {
        PJ_LOG(3,(THIS_FILE, "Processing CANCEL request"));

        // Use unified module to handle CANCEL
        sip_calling_handle_incoming_cancel(rdata);

    } else if (method->name.slen == 4 &&
               strncmp(method->name.ptr, "INFO", 4) == 0) {
        PJ_LOG(3,(THIS_FILE, "Processing INFO request"));
        handle_sip_info_dtmf(rdata);

    } else {
        PJ_LOG(3,(THIS_FILE, "Unsupported method: %.*s", method->name.slen, method->name.ptr));
        pjsip_endpt_respond_stateless(sip_endpt, rdata, 405, NULL, NULL, NULL);
    }

    PJ_LOG(3,(THIS_FILE, "=== END SIP REQUEST ==="));
    return PJ_TRUE;
}

int main(int argc, char *argv[]) {
    pj_status_t status;
    pj_caching_pool cp;
    char local_ip[16];
    sip_call_config_t call_config;
    mqtt_callbacks_t mqtt_callbacks;
    const char *config_file = argc > 1 && argv[1] && argv[1][0] ? argv[1] : CONFIG_FILE;

    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    // Load configuration first
    if (config_load(config_file, &app_config) < 0) {
        if (strcmp(config_file, CONFIG_FILE) == 0) {
            printf("Warning: %s load failed, trying legacy %s\n",
                   CONFIG_FILE, LEGACY_CONFIG_FILE);
            if (config_load(LEGACY_CONFIG_FILE, &app_config) < 0) {
                printf("Warning: Configuration load failed, using defaults\n");
                config_init_defaults(&app_config);
            }
        } else {
            printf("Warning: Configuration load failed, using defaults\n");
            config_init_defaults(&app_config);
        }
    }

    app_config.ring_snapshot_delay_ms = clamp_ring_snapshot_delay(app_config.ring_snapshot_delay_ms);
    app_config.video_recording_max_seconds = clamp_recording_max_seconds(app_config.video_recording_max_seconds);
    if (app_config.intercom_reopen_guard_ms < 0) {
        app_config.intercom_reopen_guard_ms = 0;
    } else if (app_config.intercom_reopen_guard_ms > 5000) {
        app_config.intercom_reopen_guard_ms = 5000;
    }
    if (app_config.hardware_watchdog_timeout_seconds < 5) {
        app_config.hardware_watchdog_timeout_seconds = 5;
    } else if (app_config.hardware_watchdog_timeout_seconds > 300) {
        app_config.hardware_watchdog_timeout_seconds = 300;
    }
    if (app_config.hardware_watchdog_feed_interval_seconds < 1) {
        app_config.hardware_watchdog_feed_interval_seconds = 1;
    }

    // Print loaded configuration
    config_print(&app_config);

    if (call_session_init(&current_call_session, create_call_boot_nonce()) == 0) {
        call_session_ready = 1;
    } else {
        printf("Warning: Failed to initialize call trace; calls remain operational\n");
    }

    // Get and display local IP
    get_local_ip(local_ip, sizeof(local_ip));
    strncpy(local_ip_addr, local_ip, sizeof(local_ip_addr) - 1);
    local_ip_addr[sizeof(local_ip_addr) - 1] = '\0';
    printf("Wibox SIP Media Client - Local IP: %s\n", local_ip);

    memset(&mqtt_callbacks, 0, sizeof(mqtt_callbacks));
    mqtt_callbacks.open_door = mqtt_open_door_callback;
    mqtt_callbacks.trigger_f1 = mqtt_trigger_f1_callback;
    mqtt_callbacks.reboot_device = mqtt_reboot_device_callback;
    mqtt_callbacks.take_snapshot = mqtt_take_snapshot_callback;
    mqtt_callbacks.simulate_ding = mqtt_simulate_ding_callback;
    mqtt_callbacks.simulate_handset_answered = mqtt_simulate_handset_answered_callback;
    mqtt_callbacks.set_video_enabled = mqtt_set_video_enabled_callback;
    mqtt_callbacks.set_video_bitrate = mqtt_set_video_bitrate_callback;
    mqtt_callbacks.set_sip_outgoing_call_enabled = mqtt_set_sip_outgoing_call_enabled_callback;
    mqtt_callbacks.set_hangup_on_door_unlock = mqtt_set_hangup_on_door_unlock_callback;
    mqtt_callbacks.set_outgoing_call_target = mqtt_set_outgoing_call_target_callback;
    mqtt_callbacks.set_outgoing_call_timeout = mqtt_set_outgoing_call_timeout_callback;
    mqtt_callbacks.set_ring_snapshot_delay = mqtt_set_ring_snapshot_delay_callback;
    mqtt_callbacks.set_call_forward_enabled = mqtt_set_call_forward_enabled_callback;
    mqtt_callbacks.set_rtsp_enabled = mqtt_set_rtsp_enabled_callback;
    mqtt_init(&app_config, local_ip, &mqtt_callbacks, NULL);
    if (app_config.prometheus_enabled && prometheus_start(app_config.prometheus_port) < 0) {
        printf("Warning: Failed to start Prometheus exporter\n");
    }
    prometheus_set_video_enabled(app_config.video_enabled);

    // Install signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    // Ignore SIGPIPE to prevent crashes when pipes break
    signal(SIGPIPE, SIG_IGN);

    // Setup RTP socket (no PJLIB involved)
    rtp_socket = setup_rtp_socket();
    if (rtp_socket < 0) {
        return 1;
    }

    // Check intercom access
    if (intercom_init() < 0) {
        printf("Warning: Failed to initialize intercom module\n");
    } else if (app_config.serial_listener_enabled) {
        printf("Enabling intercom physical doorbell push state\n");
        intercom_send_command(INTERCOM_CMD_ENABLE_PUSH_STATE);
    }

    // Initialize PJLIB
    status = pj_init();
    if (status != PJ_SUCCESS) {
        printf("Error initializing PJLIB: %d\n", status);
        return 1;
    }

    // Initialize logging
    pj_log_set_level(3);

    // Create pool factory
    pj_caching_pool_init(&cp, &pj_pool_factory_default_policy, 0);

    // Create memory pool
    pool = pj_pool_create(&cp.factory, "wibox", 4000, 4000, NULL);

    // Create mutex for call_active protection
    status = pj_mutex_create_simple(pool, "call_active", &call_active_mutex);
    if (status != PJ_SUCCESS) {
        printf("Error creating call_active mutex: %d\n", status);
        pj_pool_release(pool);
        pj_caching_pool_destroy(&cp);
        pj_shutdown();
        return 1;
    }

    start_rtsp_service();

    // Create SIP endpoint
    status = pjsip_endpt_create(&cp.factory, "wibox", &sip_endpt);
    if (status != PJ_SUCCESS) {
        printf("Error creating SIP endpoint: %d\n", status);
        pj_shutdown();
        return 1;
    }

    // Add UDP transport
    pjsip_transport *transport;
    pj_sockaddr_in addr;

    pj_bzero(&addr, sizeof(addr));
    addr.sin_family = PJ_AF_INET;
    addr.sin_addr.s_addr = 0;
    addr.sin_port = pj_htons((pj_uint16_t)app_config.sip_port);

    status = pjsip_udp_transport_start(sip_endpt, &addr, NULL, 1, &transport);
    if (status != PJ_SUCCESS) {
        printf("Error starting UDP transport: %d\n", status);
        pjsip_endpt_destroy(sip_endpt);
        pj_shutdown();
        return 1;
    }

    printf("SIP UDP transport started on port %d\n", app_config.sip_port);

    // Register module
    status = pjsip_endpt_register_module(sip_endpt, &mod_wibox);
    if (status != PJ_SUCCESS) {
        printf("Error registering module: %d\n", status);
        pjsip_endpt_destroy(sip_endpt);
        pj_shutdown();
        return 1;
    }

    // Initialize unified SIP calling module
    memset(&call_config, 0, sizeof(call_config));
    strcpy(call_config.local_ip, local_ip);
    call_config.local_sip_port = app_config.sip_port;
    call_config.local_rtp_port = app_config.rtp_port;
    call_config.local_video_rtp_port = app_config.video_enabled ? app_config.video_rtp_port : 0;
    call_config.video_payload_type = app_config.video_payload_type;
    strcpy(call_config.target_uri, app_config.outgoing_call_target);
    call_config.call_timeout_seconds = app_config.outgoing_call_timeout;

    status = sip_calling_init(&call_config, sip_endpt, pool);
    if (status != PJ_SUCCESS) {
        printf("Error initializing SIP calling module: %d\n", status);
        pjsip_endpt_destroy(sip_endpt);
        pj_shutdown();
        return 1;
    }

    // Set unified SIP calling callbacks
    sip_calling_set_callbacks(on_call_state_change, on_audio_ready, NULL);

    // Start DING monitoring
    if (start_ding_monitoring() < 0) {
        printf("Warning: Failed to start DING monitoring\n");
    }
    if (start_serial_monitoring() < 0) {
        printf("Warning: Failed to start serial monitoring\n");
    }
    if (mqtt_start() < 0) {
        printf("Warning: Failed to start MQTT integration\n");
    }
    if (hardware_watchdog_start(
            app_config.hardware_watchdog_enabled,
            app_config.hardware_watchdog_device,
            app_config.hardware_watchdog_timeout_seconds,
            app_config.hardware_watchdog_feed_interval_seconds) != 0) {
        printf("Warning: Hardware watchdog failed to start; continuing without it\n");
    }

    printf("Wibox SIP client ready. Listening on %s:%d, RTP on %s:%d\n",
           local_ip, app_config.sip_port, local_ip, app_config.rtp_port);
    printf("Outgoing SIP calls: %s, target: %s\n",
           app_config.sip_outgoing_call_enabled ? "enabled" : "disabled",
           app_config.outgoing_call_target);
    printf("Send '%s' to %s to trigger outgoing call\n", app_config.ding_message, app_config.sip_listen_pipe);

    // Main event loop
    time_t last_nat_keepalive = time(NULL);

    while (!quit_flag) {
        pj_time_val timeout = {0, 100};
        hardware_watchdog_heartbeat();
        pjsip_endpt_handle_events(sip_endpt, &timeout);

        // Check for timeouts and other periodic tasks
        sip_calling_check_timeout();

        // NAT keep-alive during active calls (every 20 seconds)
        time_t now = time(NULL);
        if (get_audio_sip_rtp_active() && (now - last_nat_keepalive >= 20)) {
            send_nat_keepalive();
            last_nat_keepalive = now;
        }
    }

    // Cleanup
    hardware_watchdog_stop(1);
    stop_video_session();
    stop_audio_engine("shutdown");
    stop_rtsp_service();
    mqtt_stop();
    prometheus_stop();
    stop_serial_monitoring();
    stop_ding_monitoring();
    if (call_session_ready) {
        call_session_destroy(&current_call_session);
        call_session_ready = 0;
    }

    // Terminate any active call
    sip_calling_terminate_call();
    sip_calling_cleanup();

    pjsip_endpt_destroy(sip_endpt);
    pj_pool_release(pool);
    pj_caching_pool_destroy(&cp);
    pj_shutdown();

    if (rtp_socket >= 0) {
        close(rtp_socket);
    }

    intercom_cleanup();

    printf("Wibox SIP client stopped\n");
    return 0;
}
