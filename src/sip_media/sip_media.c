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
#include <sys/wait.h>        // Child process cleanup
#include <fcntl.h>           // File control
#include <pthread.h>         // POSIX threads
#include <ifaddrs.h>         // For getifaddrs()
#include <errno.h>           // Error numbers
#include <unistd.h>          // For access()
#include <sys/stat.h>        // For mkfifo()
#include <ctype.h>           // Character classification

#include "sip_calling.h"     // Our unified SIP calling module
#include "intercom.h"        // Our communication with the intercom
#include "config.h"          // Configuration management
#include "mqtt.h"            // MQTT/Home Assistant integration
#include "audio_hw.h"        // Direct GADI audio hardware access
#include "video_worker.h"    // In-daemon D1 H.264 RTP worker

#define THIS_FILE "wibox-media-daemon"
#define CONFIG_FILE "/mnt/mtd/sip_media.conf"
#define LEGACY_CONFIG_FILE "/mnt/mtd/sip.conf"

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
static pid_t video_bridge_pid = -1;
static pj_bool_t call_active = PJ_FALSE; // Is there an active audio session?
static pj_mutex_t *call_active_mutex; // Mutex to protect call_active
static struct sockaddr_in remote_rtp_addr; // Where to send audio packets
static int current_dtmf_payload_type = RTP_PAYLOAD_DTMF;

// Configuration
static wibox_config_t app_config;

// DING monitoring
static int ding_pipe_fd = -1;
static pthread_t ding_monitor_thread;
static pj_bool_t ding_monitoring_active = PJ_FALSE;

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
static void* audio_handler(void* arg);
static int setup_rtp_socket(void);
static void start_audio_session(const char* remote_ip, int remote_port);
static void stop_audio_session(void);
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
static void stop_video_session(void);
static void unlock_door(const char* source);
static void mqtt_open_door_callback(void* user_data);
static void mqtt_set_video_enabled_callback(int enabled, void* user_data);

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

    // Check if IP changed (simple string comparison)
    sip_call_config_t call_config;
    const sip_call_session_t* session = sip_calling_get_session();
    if (session) {
        // We don't have direct access to the stored local_ip in sip_calling module
        // So we'll just log the refresh for now
        PJ_LOG(3,(THIS_FILE, "Refreshed local IP: %s", new_local_ip));
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
    if (rtp_socket < 0 || !get_call_active_status()) {
        return;  // Only during active calls
    }

    // Send minimal RTP packet to maintain NAT binding
    // This prevents NAT timeouts during long silent periods
    unsigned char keepalive_packet[12] = {
        0x80, 0x08, 0x00, 0x00,  // RTP header: V=2, PT=8 (PCMA), seq=0
        0x00, 0x00, 0x00, 0x00,  // timestamp=0
        0x00, 0x00, 0x00, 0x01   // SSRC=1
    };

    ssize_t result = sendto(rtp_socket, keepalive_packet, sizeof(keepalive_packet), MSG_DONTWAIT,
                           (struct sockaddr*)&remote_rtp_addr, sizeof(remote_rtp_addr));

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
    // Handle call establishment (both incoming and outgoing)
    if (new_state == SIP_CALL_STATE_ESTABLISHED && old_state != SIP_CALL_STATE_ESTABLISHED) {
        PJ_LOG(3,(THIS_FILE, "Call established - sending START_CALL to intercom"));
        intercom_send_command(INTERCOM_CMD_START_CALL);
        mqtt_publish_call_active(1);
        mqtt_publish_sip_call_active(1);
        mqtt_publish_ringing(0);
        mqtt_publish_media_state("established");
    }

    // Handle call termination - ONLY send STOP_CALL if we're ending an ESTABLISHED call
    if (old_state == SIP_CALL_STATE_ESTABLISHED &&
        (new_state == SIP_CALL_STATE_IDLE || new_state == SIP_CALL_STATE_FAILED)) {
        PJ_LOG(3,(THIS_FILE, "Established call terminated - sending STOP_CALL to intercom"));
        intercom_send_command(INTERCOM_CMD_STOP_CALL);
        stop_video_session();
        stop_audio_session();
        mqtt_publish_call_active(0);
        mqtt_publish_sip_call_active(0);
        mqtt_publish_video_active(0);
        mqtt_publish_media_state("idle");
    }

    // Handle non-established call termination (no intercom command needed)
    if (old_state != SIP_CALL_STATE_ESTABLISHED && old_state != SIP_CALL_STATE_IDLE &&
        (new_state == SIP_CALL_STATE_IDLE || new_state == SIP_CALL_STATE_FAILED)) {
        PJ_LOG(3,(THIS_FILE, "Non-established call terminated - no intercom command needed"));
        stop_video_session();
        stop_audio_session();
        mqtt_publish_call_active(0);
        mqtt_publish_sip_call_active(0);
        mqtt_publish_video_active(0);
        mqtt_publish_media_state("idle");
    }
}

static void on_audio_ready(const char* remote_ip, int remote_rtp_port,
                           int remote_video_rtp_port, void* user_data) {
    PJ_LOG(3,(THIS_FILE, "Media ready: audio=%s:%d video=%s:%d",
              remote_ip, remote_rtp_port, remote_ip, remote_video_rtp_port));

    // Don't start if already active
    if (get_call_active_status()) {
        PJ_LOG(3,(THIS_FILE, "Audio session already active - ignoring duplicate"));
        return;
    }
    start_audio_session(remote_ip, remote_rtp_port);
    PJ_LOG(3,(THIS_FILE, "Audio session start returned; starting video"));
    start_video_session(remote_ip, remote_video_rtp_port);
}

static void start_video_session(const char* remote_ip, int remote_video_port) {
    const sip_call_session_t *session;
    int payload_type;

    if (!app_config.video_enabled) {
        PJ_LOG(3,(THIS_FILE, "Video disabled by configuration"));
        return;
    }
    if (remote_video_port <= 0) {
        PJ_LOG(3,(THIS_FILE, "Remote SDP has no video port; not starting video"));
        return;
    }
    if (video_bridge_pid > 0) {
        PJ_LOG(3,(THIS_FILE, "Video worker already running pid=%d", video_bridge_pid));
        return;
    }

    session = sip_calling_get_session();
    payload_type = app_config.video_payload_type;
    if (session && session->remote_video_payload_type > 0) {
        payload_type = session->remote_video_payload_type;
    }

    video_bridge_pid = fork();
    if (video_bridge_pid < 0) {
        PJ_LOG(1,(THIS_FILE, "Failed to fork video worker: %s", strerror(errno)));
        video_bridge_pid = -1;
        return;
    }
    if (video_bridge_pid == 0) {
        int log_fd = open("/tmp/wibox-video-worker.log",
                          O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (log_fd >= 0) {
            dup2(log_fd, STDOUT_FILENO);
            dup2(log_fd, STDERR_FILENO);
            close(log_fd);
        }
        _exit(video_worker_run(remote_ip, remote_video_port,
                               app_config.video_rtp_port, payload_type, NULL));
    }

    PJ_LOG(3,(THIS_FILE, "Started in-daemon video worker pid=%d to %s:%d payload=%d",
              video_bridge_pid, remote_ip, remote_video_port, payload_type));
    mqtt_publish_video_active(1);
}

static void stop_video_session(void) {
    int status;
    int i;

    if (video_bridge_pid <= 0) {
        return;
    }

    PJ_LOG(3,(THIS_FILE, "Stopping video worker pid=%d", video_bridge_pid));
    kill(video_bridge_pid, SIGTERM);
    for (i = 0; i < 20; i++) {
        pid_t r = waitpid(video_bridge_pid, &status, WNOHANG);
        if (r == video_bridge_pid) {
            video_bridge_pid = -1;
            return;
        }
        usleep(100000);
    }

    kill(video_bridge_pid, SIGKILL);
    waitpid(video_bridge_pid, &status, 0);
    video_bridge_pid = -1;
    mqtt_publish_video_active(0);
}

static void unlock_door(const char* source) {
    printf("Unlocking door from %s\n", source ? source : "unknown");
    if (intercom_send_command(INTERCOM_CMD_UNLOCK_DOOR) == 0) {
        printf("Door unlock command sent successfully\n");
        mqtt_publish_last_unlock();
    } else {
        printf("Failed to send door unlock command\n");
    }
}

static void mqtt_open_door_callback(void* user_data) {
    (void)user_data;

    if (sip_calling_is_call_active()) {
        unlock_door("mqtt");
        return;
    }

    printf("MQTT open door requested without active call; starting panel context\n");
    intercom_send_command(INTERCOM_CMD_START_CALL);
    usleep(500000);
    unlock_door("mqtt");
    usleep(1000000);
    intercom_send_command(INTERCOM_CMD_STOP_CALL);
}

static void mqtt_set_video_enabled_callback(int enabled, void* user_data) {
    (void)user_data;
    app_config.video_enabled = enabled ? 1 : 0;
    printf("MQTT video_enabled set to %d\n", app_config.video_enabled);
    mqtt_publish_video_enabled(app_config.video_enabled);
}

static void handle_audio_test_control(const char* message) {
    char ip[64];
    int port;
    int seconds;

    if (sscanf(message, "%63s %d %d", ip, &port, &seconds) != 3 ||
        port <= 0 || seconds <= 0 || seconds > 30) {
        PJ_LOG(2,(THIS_FILE, "Invalid AUDIO_TEST command: '%s'", message));
        return;
    }

    if (get_call_active_status()) {
        PJ_LOG(2,(THIS_FILE, "AUDIO_TEST ignored - audio session already active"));
        return;
    }

    PJ_LOG(3,(THIS_FILE, "AUDIO_TEST starting to %s:%d for %d seconds",
              ip, port, seconds));
    intercom_send_command(INTERCOM_CMD_START_CALL);
    usleep(500000);
    start_audio_session(ip, port);
    sleep((unsigned)seconds);
    stop_audio_session();
    intercom_send_command(INTERCOM_CMD_STOP_CALL);
    PJ_LOG(3,(THIS_FILE, "AUDIO_TEST complete"));
}

static void handle_video_test_control(const char* message) {
    char ip[64];
    int port;
    int seconds;

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
    intercom_send_command(INTERCOM_CMD_START_CALL);
    usleep(500000);
    start_video_session(ip, port);
    sleep((unsigned)seconds);
    stop_video_session();
    intercom_send_command(INTERCOM_CMD_STOP_CALL);
    PJ_LOG(3,(THIS_FILE, "VIDEO_TEST complete"));
}

static void handle_ding_trigger(const char* source) {
    pj_status_t status;

    PJ_LOG(3,(THIS_FILE, "DING detected from %s - checking if we can make outgoing call",
              source ? source : "unknown"));

    if (sip_calling_is_call_active()) {
        PJ_LOG(2,(THIS_FILE, "DING ignored - call already active"));
        return;
    }

    mqtt_publish_ringing(1);
    mqtt_publish_last_ring();
    mqtt_publish_media_state("ringing");

    PJ_LOG(3,(THIS_FILE, "Making outgoing call due to %s", source ? source : "DING"));
    status = sip_calling_make_call();
    if (status != PJ_SUCCESS) {
        PJ_LOG(1,(THIS_FILE, "Failed to make outgoing call: %d", status));
    }
}

static int hex_nibble(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

static int parse_uart_control_frame(const char* input, unsigned char frame[4]) {
    const char* p = input;
    int count = 0;

    if (strncmp(p, "UART", 4) != 0) {
        return -1;
    }
    p += 4;

    while (*p && count < 4) {
        int hi;
        int lo;

        while (*p && (isspace((unsigned char)*p) || *p == ':' || *p == '-')) {
            p++;
        }
        if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
            p += 2;
        }
        if (p[0] == '\\' && (p[1] == 'x' || p[1] == 'X')) {
            p += 2;
        }

        hi = hex_nibble(p[0]);
        lo = hex_nibble(p[1]);
        if (hi < 0 || lo < 0) {
            return -1;
        }

        frame[count++] = (unsigned char)((hi << 4) | lo);
        p += 2;
    }

    while (*p && isspace((unsigned char)*p)) {
        p++;
    }

    return (count == 4 && *p == '\0') ? 0 : -1;
}

static void handle_control_message(const char* message) {
    unsigned char frame[4];

    if (strncmp(message, app_config.ding_message, strlen(app_config.ding_message)) == 0) {
        handle_ding_trigger("pipe");
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

    if (parse_uart_control_frame(message, frame) == 0) {
        PJ_LOG(3,(THIS_FILE, "Injecting UART frame from control pipe: %02X %02X %02X %02X",
                  frame[0], frame[1], frame[2], frame[3]));
        handle_uart_frame(frame);
        return;
    }

    PJ_LOG(2,(THIS_FILE, "Unknown control pipe message: '%s'", message));
}

// DING monitoring thread
static void* ding_monitor_thread_func(void* arg) {
    char buffer[64];
    ssize_t bytes_read;

    // Register this thread with PJLIB
    pj_thread_desc desc;
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

typedef enum {
    UART_CODE_UNKNOWN = 0,
    UART_CODE_ALARM_REPORT,
    UART_CODE_CMD_RESET,
    UART_CODE_START_CALL,
    UART_CODE_HANG_UP_0,
    UART_CODE_HANG_UP_1,
    UART_CODE_CMD_STOP_RING,
    UART_CODE_PUSH_STATE_0,
    UART_CODE_PUSH_STATE_1,
    UART_CODE_MCU_STATE_0,
    UART_CODE_MCU_STATE_1,
    UART_CODE_CMD_DOWN_LONG_1,
    UART_CODE_CMD_DOWN_LONG_2
} uart_code_t;

typedef struct {
    uart_code_t code;
    const char* name;
    unsigned char bytes[4];
} uart_code_def_t;

static const uart_code_def_t uart_codes[] = {
    {UART_CODE_ALARM_REPORT,    "ALARM_REPORT",    {0xFB, 0x11, 0x00, 0x1C}},
    {UART_CODE_CMD_RESET,       "CMD_RESET",       {0xFB, 0x20, 0x00, 0x2B}},
    {UART_CODE_START_CALL,      "START_CALL",      {0xFB, 0x14, 0x01, 0x20}},
    {UART_CODE_HANG_UP_0,       "HANG_UP_0",       {0xFB, 0x13, 0x00, 0x1E}},
    {UART_CODE_HANG_UP_1,       "HANG_UP_1",       {0xFB, 0x13, 0x01, 0x1F}},
    {UART_CODE_CMD_STOP_RING,   "CMD_STOP_RING",   {0xFB, 0x23, 0x00, 0x2E}},
    {UART_CODE_PUSH_STATE_0,    "PUSH_STATE_0",    {0xFB, 0x19, 0x00, 0x24}},
    {UART_CODE_PUSH_STATE_1,    "PUSH_STATE_1",    {0xFB, 0x19, 0x01, 0x25}},
    {UART_CODE_MCU_STATE_0,     "MCU_STATE_0",     {0xFB, 0x16, 0x00, 0x21}},
    {UART_CODE_MCU_STATE_1,     "MCU_STATE_1",     {0xFB, 0x16, 0x01, 0x22}},
    {UART_CODE_CMD_DOWN_LONG_1, "CMD_DOWN_LONG_1", {0xFB, 0x24, 0x01, 0x30}},
    {UART_CODE_CMD_DOWN_LONG_2, "CMD_DOWN_LONG_2", {0xFB, 0x24, 0x02, 0x31}}
};

static const uart_code_def_t* find_uart_code(const unsigned char frame[4]) {
    size_t i;

    for (i = 0; i < sizeof(uart_codes) / sizeof(uart_codes[0]); i++) {
        if (memcmp(frame, uart_codes[i].bytes, 4) == 0) {
            return &uart_codes[i];
        }
    }
    return NULL;
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

static void handle_uart_frame(const unsigned char frame[4]) {
    const uart_code_def_t* def = find_uart_code(frame);

    if (!def) {
        PJ_LOG(3,(THIS_FILE, "UART code unknown: %02X %02X %02X %02X",
                  frame[0], frame[1], frame[2], frame[3]));
        return;
    }

    PJ_LOG(3,(THIS_FILE, "UART code received: %s [%02X %02X %02X %02X]",
              def->name, frame[0], frame[1], frame[2], frame[3]));

    switch (def->code) {
    case UART_CODE_ALARM_REPORT:
        report_alarm_event(1);
        handle_ding_trigger("serial alarm");
        break;
    case UART_CODE_HANG_UP_0:
    case UART_CODE_HANG_UP_1:
        report_alarm_event(2);
        mqtt_publish_ringing(0);
        mqtt_publish_media_state("idle");
        terminate_call_from_serial(def->name);
        break;
    case UART_CODE_CMD_STOP_RING:
        report_alarm_event(3);
        mqtt_publish_ringing(0);
        mqtt_publish_media_state("idle");
        terminate_call_from_serial(def->name);
        break;
    case UART_CODE_CMD_RESET:
        PJ_LOG(2,(THIS_FILE, "Reset command received from panel"));
        system("sync && reboot");
        break;
    case UART_CODE_START_CALL:
        PJ_LOG(3,(THIS_FILE, "Intercom call line is active"));
        mqtt_publish_call_active(1);
        break;
    case UART_CODE_PUSH_STATE_0:
    case UART_CODE_PUSH_STATE_1:
    case UART_CODE_MCU_STATE_0:
    case UART_CODE_MCU_STATE_1:
    case UART_CODE_CMD_DOWN_LONG_1:
    case UART_CODE_CMD_DOWN_LONG_2:
    default:
        break;
    }
}

static void* serial_monitor_thread_func(void* arg) {
    unsigned char frame[4];
    size_t frame_len = 0;
    pj_thread_desc desc;
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
            serial_fd = open(app_config.intercom_device, O_RDONLY | O_NONBLOCK);
            if (serial_fd < 0) {
                PJ_LOG(2,(THIS_FILE, "Failed to open %s for serial monitoring: %s",
                          app_config.intercom_device, strerror(errno)));
                sleep(1);
                continue;
            }
            PJ_LOG(3,(THIS_FILE, "Serial monitor opened %s", app_config.intercom_device));
        }

        n = read(serial_fd, buffer, sizeof(buffer));
        if (n > 0) {
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

    while (get_call_active_status() && !quit_flag) {
        // === AI -> RTP (microphone to network) ===
        bytes_read = audio_hw_get_frame(audio_buffer, app_config.audio_buffer_size);
        if (bytes_read > 0) {
            consecutive_errors = 0;
        } else {
            consecutive_errors++;
            if (consecutive_errors >= MAX_CONSECUTIVE_ERRORS) {
                PJ_LOG(1,(THIS_FILE, "Audio hardware input failed repeatedly"));
                set_call_active_status(PJ_FALSE);
                break;
            }
            generate_error_audio(audio_buffer, app_config.audio_buffer_size);
            bytes_read = app_config.audio_buffer_size;
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

        // Send RTP packet
        ssize_t bytes_sent = sendto(rtp_socket, rtp_packet, bytes_read + 12, 0,
                            (struct sockaddr*)&remote_rtp_addr, sizeof(remote_rtp_addr));
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

        seq_num++;
        timestamp += 160;
        usleep(18000);
    }

    free(audio_buffer);
    free(rtp_packet);

    PJ_LOG(3,(THIS_FILE, "Audio input thread stopped: packets=%d payload_bytes=%d",
              packets_sent, bytes_payload_sent));
    return NULL;
}

// Separate AO thread (network to speaker)
static void* audio_output_handler(void* arg) {
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

    while (get_call_active_status() && !quit_flag) {
        // === RTP → AO (Network to speaker) ===
        struct sockaddr_in from_addr;
        socklen_t from_len = sizeof(from_addr);
        bytes_read = recvfrom(rtp_socket, rtp_packet, app_config.audio_buffer_size + 12, MSG_DONTWAIT,
                             (struct sockaddr*)&from_addr, &from_len);

        if (bytes_read > 12) {
            unsigned char payload_type = rtp_packet[1] & 0x7F;
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

        usleep(1000); // 1ms polling for received packets - faster than input timing
    }

    free(rtp_packet);

    PJ_LOG(3,(THIS_FILE, "Audio output thread stopped: packets=%d payload_bytes=%d",
              packets_received, payload_bytes_played));
    return NULL;
}

// Start audio session
static void start_audio_session(const char* remote_ip, int remote_port) {
    pj_status_t status;
    const sip_call_session_t *session;

    PJ_LOG(3,(THIS_FILE, "Starting audio session to %s:%d", remote_ip, remote_port));

    session = sip_calling_get_session();
    current_dtmf_payload_type = RTP_PAYLOAD_DTMF;
    if (session && session->remote_dtmf_payload_type > 0) {
        current_dtmf_payload_type = session->remote_dtmf_payload_type;
    }
    PJ_LOG(3,(THIS_FILE, "Using DTMF RTP payload type %d", current_dtmf_payload_type));

    memset(&remote_rtp_addr, 0, sizeof(remote_rtp_addr));
    remote_rtp_addr.sin_family = AF_INET;
    remote_rtp_addr.sin_port = htons(remote_port);
    inet_pton(AF_INET, remote_ip, &remote_rtp_addr.sin_addr);

    if (audio_hw_start(app_config.audio_chip_gpio, app_config.audio_buffer_size) < 0) {
        PJ_LOG(1,(THIS_FILE, "Failed to start audio hardware"));
        return;
    }

    set_call_active_status(PJ_TRUE);

    // Create audio input thread using PJLIB
    status = pj_thread_create(pool, "audio_input",
                             (pj_thread_proc*)audio_input_handler, NULL,
                             PJ_THREAD_DEFAULT_STACK_SIZE, 0, &audio_input_thread);
    if (status != PJ_SUCCESS) {
        PJ_LOG(1,(THIS_FILE, "Failed to create audio input thread: %d", status));
        set_call_active_status(PJ_FALSE);
        audio_hw_stop();
        return;
    }

    // Create audio output thread using PJLIB
    status = pj_thread_create(pool, "audio_output",
                             (pj_thread_proc*)audio_output_handler, NULL,
                             PJ_THREAD_DEFAULT_STACK_SIZE, 0, &audio_output_thread);
    if (status != PJ_SUCCESS) {
        PJ_LOG(1,(THIS_FILE, "Failed to create audio output thread: %d", status));
        set_call_active_status(PJ_FALSE);

        // Clean up the input thread
        pj_thread_destroy(audio_input_thread);
        audio_input_thread = NULL;
        audio_hw_stop();
        return;
    }

    PJ_LOG(3,(THIS_FILE, "Audio threads created successfully"));
}

// Stop audio session
static void stop_audio_session(void) {
    if (get_call_active_status()) {
        PJ_LOG(3,(THIS_FILE, "Stopping audio session"));

        // If quit_flag is set, give audio thread time to send goodbye packets
        if (quit_flag) {
            PJ_LOG(3,(THIS_FILE, "Waiting for goodbye packets to be sent..."));
            usleep(100000); // 100ms should be enough for 3 packets at 10ms intervals
        }

        set_call_active_status(PJ_FALSE);

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
    }
}

// Handle incoming SIP responses
static pj_bool_t on_rx_response(pjsip_rx_data *rdata) {
    // Let unified SIP calling module handle it
    return sip_calling_handle_response(rdata);
}

// Handle incoming SIP requests
static pj_bool_t on_rx_request(pjsip_rx_data *rdata) {
    pjsip_method *method = &rdata->msg_info.msg->line.req.method;

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

    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    // Load configuration first
    if (config_load(CONFIG_FILE, &app_config) < 0) {
        printf("Warning: %s load failed, trying legacy %s\n",
               CONFIG_FILE, LEGACY_CONFIG_FILE);
        if (config_load(LEGACY_CONFIG_FILE, &app_config) < 0) {
            printf("Warning: Configuration load failed, using defaults\n");
            config_init_defaults(&app_config);
        }
    }

    // Print loaded configuration
    config_print(&app_config);

    // Get and display local IP
    get_local_ip(local_ip, sizeof(local_ip));
    printf("Wibox SIP Media Client - Local IP: %s\n", local_ip);

    memset(&mqtt_callbacks, 0, sizeof(mqtt_callbacks));
    mqtt_callbacks.open_door = mqtt_open_door_callback;
    mqtt_callbacks.set_video_enabled = mqtt_set_video_enabled_callback;
    mqtt_init(&app_config, local_ip, &mqtt_callbacks, NULL);

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

    printf("Wibox SIP client ready. Listening on %s:%d, RTP on %s:%d\n",
           local_ip, app_config.sip_port, local_ip, app_config.rtp_port);
    printf("Will make outgoing calls to: %s\n", app_config.outgoing_call_target);
    printf("Send '%s' to %s to trigger outgoing call\n", app_config.ding_message, app_config.sip_listen_pipe);

    // Main event loop
    time_t last_nat_keepalive = time(NULL);

    while (!quit_flag) {
        pj_time_val timeout = {0, 100};
        pjsip_endpt_handle_events(sip_endpt, &timeout);

        // Check for timeouts and other periodic tasks
        sip_calling_check_timeout();

        // NAT keep-alive during active calls (every 20 seconds)
        time_t now = time(NULL);
        if (get_call_active_status() && (now - last_nat_keepalive >= 20)) {
            send_nat_keepalive();
            last_nat_keepalive = now;
        }
    }

    // Cleanup
    mqtt_stop();
    stop_serial_monitoring();
    stop_ding_monitoring();
    stop_video_session();
    stop_audio_session();

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
