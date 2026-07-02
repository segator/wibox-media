#include "mqtt.h"

#include <ctype.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define MQTT_FILE "mqtt"

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
    int enabled;
    char host[128];
    char user[128];
    char pass[128];
    char ha_prefix[128];
    char base_topic[128];
    char device_id[128];
    char device_name[128];
    char local_ip[64];
    int video_enabled;
    int call_forward_enabled;
    int firmware_update_enabled;
    char firmware_update_repo[128];
    char firmware_update_version[64];
    int firmware_update_available;
    int firmware_update_installing;
    time_t firmware_update_last_check;
    mqtt_callbacks_t callbacks;
    void* user_data;
    pthread_t thread;
    int running;
    int connected;
    int sock;
    pthread_mutex_t io_mutex;
    unsigned short packet_id;
} mqtt_state_t;

static mqtt_state_t mqtt_state;

static void normalize_id(const char* input, char* out, size_t out_size) {
    size_t i;
    size_t pos = 0;

    if (out_size == 0) return;
    for (i = 0; input && input[i] && pos + 1 < out_size; i++) {
        unsigned char c = (unsigned char)input[i];
        if (isalnum(c)) {
            out[pos++] = (char)tolower(c);
        } else if (c == '-' || c == '_' || c == '/' || c == '.') {
            out[pos++] = '_';
        }
    }
    if (pos == 0 && out_size > 1) {
        strcpy(out, "wibox");
        return;
    }
    out[pos] = '\0';
}

static void get_hostname(char* out, size_t out_size) {
    FILE* fp;

    if (out_size == 0) return;
    out[0] = '\0';

    fp = fopen("/proc/sys/kernel/hostname", "r");
    if (fp) {
        if (fgets(out, out_size, fp)) {
            out[strcspn(out, "\r\n")] = '\0';
        }
        fclose(fp);
    }
    if (out[0] == '\0') {
        strncpy(out, "IDS79380000", out_size - 1);
        out[out_size - 1] = '\0';
    }
}

static int parse_host_port(const char* input, char* host, size_t host_size, int* port) {
    const char* colon;

    if (!input || !host || host_size == 0 || !port) {
        return -1;
    }

    *port = 1883;
    colon = strrchr(input, ':');
    if (colon && colon != input && strchr(colon + 1, ':') == NULL) {
        size_t len = (size_t)(colon - input);
        if (len >= host_size) len = host_size - 1;
        memcpy(host, input, len);
        host[len] = '\0';
        *port = atoi(colon + 1);
        if (*port <= 0) *port = 1883;
    } else {
        strncpy(host, input, host_size - 1);
        host[host_size - 1] = '\0';
    }

    return host[0] ? 0 : -1;
}

static int socket_write_all(int fd, const unsigned char* data, size_t len) {
    size_t sent = 0;

    while (sent < len) {
        ssize_t n = send(fd, data + sent, len - sent, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) return -1;
        sent += (size_t)n;
    }
    return 0;
}

static int socket_read_all(int fd, unsigned char* data, size_t len) {
    size_t got = 0;

    while (got < len) {
        ssize_t n = recv(fd, data + got, len - got, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) return -1;
        got += (size_t)n;
    }
    return 0;
}

static int mqtt_encode_remaining(unsigned char* out, size_t out_size, int len) {
    int pos = 0;

    do {
        unsigned char encoded = (unsigned char)(len % 128);
        len /= 128;
        if (len > 0) encoded |= 128;
        if ((size_t)pos >= out_size) return -1;
        out[pos++] = encoded;
    } while (len > 0);

    return pos;
}

static int mqtt_append_u16(unsigned char* buf, size_t buf_size, size_t* pos, unsigned short value) {
    if (*pos + 2 > buf_size) return -1;
    buf[(*pos)++] = (unsigned char)(value >> 8);
    buf[(*pos)++] = (unsigned char)(value & 0xff);
    return 0;
}

static int mqtt_append_bytes(unsigned char* buf, size_t buf_size, size_t* pos,
                             const unsigned char* data, size_t len) {
    if (*pos + len > buf_size) return -1;
    memcpy(buf + *pos, data, len);
    *pos += len;
    return 0;
}

static int mqtt_append_str(unsigned char* buf, size_t buf_size, size_t* pos, const char* value) {
    size_t len = value ? strlen(value) : 0;
    if (len > 65535) return -1;
    if (mqtt_append_u16(buf, buf_size, pos, (unsigned short)len) < 0) return -1;
    return mqtt_append_bytes(buf, buf_size, pos, (const unsigned char*)(value ? value : ""), len);
}

static int mqtt_send_packet(int packet_type_flags, const unsigned char* payload, size_t payload_len) {
    unsigned char header[5];
    int rem_len_size;

    if (mqtt_state.sock < 0) {
        return -1;
    }

    header[0] = (unsigned char)packet_type_flags;
    rem_len_size = mqtt_encode_remaining(header + 1, sizeof(header) - 1, (int)payload_len);
    if (rem_len_size < 0) {
        return -1;
    }

    pthread_mutex_lock(&mqtt_state.io_mutex);
    if (socket_write_all(mqtt_state.sock, header, (size_t)rem_len_size + 1) < 0 ||
        socket_write_all(mqtt_state.sock, payload, payload_len) < 0) {
        pthread_mutex_unlock(&mqtt_state.io_mutex);
        return -1;
    }
    pthread_mutex_unlock(&mqtt_state.io_mutex);
    return 0;
}

static int mqtt_tcp_connect(void) {
    char host[128];
    char port_str[16];
    int port;
    struct addrinfo hints;
    struct addrinfo* res = NULL;
    struct addrinfo* it;
    int fd = -1;

    if (parse_host_port(mqtt_state.host, host, sizeof(host), &port) < 0) {
        return -1;
    }
    snprintf(port_str, sizeof(port_str), "%d", port);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host, port_str, &hints, &res) != 0) {
        printf("%s: DNS/host lookup failed for %s:%s\n", MQTT_FILE, host, port_str);
        return -1;
    }

    for (it = res; it; it = it->ai_next) {
        fd = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
        if (fd < 0) continue;
        if (connect(fd, it->ai_addr, it->ai_addrlen) == 0) {
            break;
        }
        close(fd);
        fd = -1;
    }

    freeaddrinfo(res);
    if (fd < 0) {
        printf("%s: TCP connect failed to %s:%s\n", MQTT_FILE, host, port_str);
    }
    return fd;
}

static int mqtt_read_packet(unsigned char* type_flags, unsigned char* payload,
                            size_t payload_size, int* payload_len) {
    unsigned char byte;
    int multiplier = 1;
    int remaining = 0;

    if (socket_read_all(mqtt_state.sock, type_flags, 1) < 0) {
        return -1;
    }

    do {
        if (socket_read_all(mqtt_state.sock, &byte, 1) < 0) {
            return -1;
        }
        remaining += (byte & 127) * multiplier;
        multiplier *= 128;
        if (multiplier > 128 * 128 * 128) {
            return -1;
        }
    } while (byte & 128);

    if ((size_t)remaining > payload_size) {
        unsigned char tmp[128];
        int left = remaining;
        while (left > 0) {
            int chunk = left > (int)sizeof(tmp) ? (int)sizeof(tmp) : left;
            if (socket_read_all(mqtt_state.sock, tmp, (size_t)chunk) < 0) return -1;
            left -= chunk;
        }
        return -1;
    }

    if (socket_read_all(mqtt_state.sock, payload, (size_t)remaining) < 0) {
        return -1;
    }
    *payload_len = remaining;
    return 0;
}

static const char* mqtt_connack_reason(int code) {
    switch (code) {
    case 0:
        return "accepted";
    case 1:
        return "unacceptable protocol version";
    case 2:
        return "identifier rejected";
    case 3:
        return "server unavailable";
    case 4:
        return "bad username or password";
    case 5:
        return "not authorized";
    default:
        return "unknown";
    }
}

static int mqtt_connect_session(void) {
    unsigned char payload[1024];
    unsigned char response[8];
    unsigned char type;
    size_t pos = 0;
    int len = 0;
    char client_id[192];
    char will_topic[256];
    unsigned char flags = 0x02; /* clean session */

    mqtt_state.sock = mqtt_tcp_connect();
    if (mqtt_state.sock < 0) {
        return -1;
    }

    snprintf(client_id, sizeof(client_id), "wibox_%s_daemon", mqtt_state.device_id);
    snprintf(will_topic, sizeof(will_topic), "%s", mqtt_state.base_topic);

    if (mqtt_state.user[0]) flags |= 0x80;
    if (mqtt_state.pass[0]) flags |= 0x40;
    flags |= 0x20; /* will retain */
    flags |= 0x04; /* will flag, QoS 0 */

    if (mqtt_append_str(payload, sizeof(payload), &pos, "MQTT") < 0 ||
        mqtt_append_bytes(payload, sizeof(payload), &pos, (const unsigned char*)"\x04", 1) < 0 ||
        mqtt_append_bytes(payload, sizeof(payload), &pos, &flags, 1) < 0 ||
        mqtt_append_u16(payload, sizeof(payload), &pos, 60) < 0 ||
        mqtt_append_str(payload, sizeof(payload), &pos, client_id) < 0 ||
        mqtt_append_str(payload, sizeof(payload), &pos, will_topic) < 0 ||
        mqtt_append_str(payload, sizeof(payload), &pos, "offline") < 0 ||
        (mqtt_state.user[0] && mqtt_append_str(payload, sizeof(payload), &pos, mqtt_state.user) < 0) ||
        (mqtt_state.pass[0] && mqtt_append_str(payload, sizeof(payload), &pos, mqtt_state.pass) < 0)) {
        close(mqtt_state.sock);
        mqtt_state.sock = -1;
        return -1;
    }

    if (mqtt_send_packet(0x10, payload, pos) < 0 ||
        mqtt_read_packet(&type, response, sizeof(response), &len) < 0) {
        printf("%s: CONNECT exchange failed\n", MQTT_FILE);
        close(mqtt_state.sock);
        mqtt_state.sock = -1;
        return -1;
    }

    if (type != 0x20 || len < 2 || response[1] != 0) {
        int code = len >= 2 ? response[1] : -1;
        printf("%s: CONNACK rejected type=0x%02x len=%d code=%d (%s)\n",
               MQTT_FILE, type, len, code, mqtt_connack_reason(code));
        close(mqtt_state.sock);
        mqtt_state.sock = -1;
        return -1;
    }

    return 0;
}

static int mqtt_publish_raw(const char* topic, const char* payload, int retain) {
    unsigned char packet[2300];
    size_t pos = 0;

    if (!mqtt_state.enabled || !mqtt_state.connected || !topic || !payload) {
        return -1;
    }

    if (mqtt_append_str(packet, sizeof(packet), &pos, topic) < 0 ||
        mqtt_append_bytes(packet, sizeof(packet), &pos,
                          (const unsigned char*)payload, strlen(payload)) < 0) {
        return -1;
    }

    return mqtt_send_packet(retain ? 0x31 : 0x30, packet, pos);
}

static void topic_path(char* out, size_t out_size, const char* suffix) {
    snprintf(out, out_size, "%s/%s", mqtt_state.base_topic, suffix);
}

static void publish_suffix(const char* suffix, const char* payload, int retain) {
    char topic[256];
    if (!mqtt_state.connected) {
        return;
    }
    topic_path(topic, sizeof(topic), suffix);
    mqtt_publish_raw(topic, payload, retain);
}

static void clear_retained_topic(const char* topic) {
    if (!mqtt_state.connected || !topic || !topic[0]) {
        return;
    }
    mqtt_publish_raw(topic, "", 1);
}

static void unique_id(char* out, size_t out_size, const char* suffix) {
    char norm_suffix[128];
    normalize_id(suffix, norm_suffix, sizeof(norm_suffix));
    snprintf(out, out_size, "wibox_%s_%s", mqtt_state.device_id, norm_suffix);
}

static void device_json(char* out, size_t out_size) {
    snprintf(out, out_size,
             "\"device\":{\"identifiers\":[\"wibox_%s\"],\"name\":\"%s\","
             "\"model\":\"WiBox 7938\",\"manufacturer\":\"Fermax\","
             "\"suggested_area\":\"Entrance\",\"configuration_url\":\"http://%s\"}",
             mqtt_state.device_id, mqtt_state.device_name, mqtt_state.local_ip);
}

static void discovery_topic(char* out, size_t out_size,
                            const char* component, const char* object_id) {
    char uid[192];
    unique_id(uid, sizeof(uid), object_id);
    snprintf(out, out_size, "%s/%s/%s/config", mqtt_state.ha_prefix, component, uid);
}

static void publish_sensor_config(const char* object_id, const char* name,
                                  const char* state_suffix, const char* device_class,
                                  const char* icon) {
    char topic[256];
    char state_topic[256];
    char uid[192];
    char dev[512];
    char payload[1536];
    char class_part[128] = "";
    char icon_part[128] = "";

    discovery_topic(topic, sizeof(topic), "sensor", object_id);
    topic_path(state_topic, sizeof(state_topic), state_suffix);
    unique_id(uid, sizeof(uid), object_id);
    device_json(dev, sizeof(dev));

    if (device_class && device_class[0]) {
        snprintf(class_part, sizeof(class_part), "\"device_class\":\"%s\",", device_class);
    }
    if (icon && icon[0]) {
        snprintf(icon_part, sizeof(icon_part), "\"icon\":\"mdi:%s\",", icon);
    }

    snprintf(payload, sizeof(payload),
             "{\"name\":\"%s\",\"unique_id\":\"%s\",\"state_topic\":\"%s\","
             "\"availability_topic\":\"%s\",%s%s%s}",
             name, uid, state_topic, mqtt_state.base_topic, class_part, icon_part, dev);
    mqtt_publish_raw(topic, payload, 1);
}

static void publish_button_config(void) {
    char topic[256];
    char command_topic[256];
    char uid[192];
    char dev[512];
    char payload[1536];

    discovery_topic(topic, sizeof(topic), "button", "open_door");
    topic_path(command_topic, sizeof(command_topic), "door/open/set");
    unique_id(uid, sizeof(uid), "open_door");
    device_json(dev, sizeof(dev));

    snprintf(payload, sizeof(payload),
             "{\"name\":\"Open Door\",\"unique_id\":\"%s\","
             "\"command_topic\":\"%s\",\"payload_press\":\"PRESS\","
             "\"availability_topic\":\"%s\",\"icon\":\"mdi:door-open\",%s}",
             uid, command_topic, mqtt_state.base_topic, dev);
    mqtt_publish_raw(topic, payload, 1);
}

static void publish_firmware_update_button_config(void) {
    char topic[256];
    char command_topic[256];
    char availability_topic[256];
    char uid[192];
    char dev[512];
    char payload[1536];

    discovery_topic(topic, sizeof(topic), "button", "firmware_update_install");
    topic_path(command_topic, sizeof(command_topic), "firmware/update/install/set");
    topic_path(availability_topic, sizeof(availability_topic), "firmware/update/install/availability");
    unique_id(uid, sizeof(uid), "firmware_update_install");
    device_json(dev, sizeof(dev));

    snprintf(payload, sizeof(payload),
             "{\"name\":\"Firmware Update Install\",\"unique_id\":\"%s\","
             "\"command_topic\":\"%s\",\"payload_press\":\"PRESS\","
             "\"availability_topic\":\"%s\",\"icon\":\"mdi:update\",%s}",
             uid, command_topic, availability_topic, dev);
    mqtt_publish_raw(topic, payload, 1);
}

static void publish_firmware_update_refresh_button_config(void) {
    char topic[256];
    char command_topic[256];
    char uid[192];
    char dev[512];
    char payload[1536];

    discovery_topic(topic, sizeof(topic), "button", "firmware_update_refresh");
    topic_path(command_topic, sizeof(command_topic), "firmware/update/check/set");
    unique_id(uid, sizeof(uid), "firmware_update_refresh");
    device_json(dev, sizeof(dev));

    snprintf(payload, sizeof(payload),
             "{\"name\":\"Firmware Update Refresh\",\"unique_id\":\"%s\","
             "\"command_topic\":\"%s\",\"payload_press\":\"PRESS\","
             "\"availability_topic\":\"%s\",\"icon\":\"mdi:refresh\",%s}",
             uid, command_topic, mqtt_state.base_topic, dev);
    mqtt_publish_raw(topic, payload, 1);
}

static void publish_firmware_update_binary_sensor_config(void) {
    char topic[256];
    char state_topic[256];
    char uid[192];
    char dev[512];
    char payload[1536];

    discovery_topic(topic, sizeof(topic), "binary_sensor", "firmware_update_available");
    topic_path(state_topic, sizeof(state_topic), "firmware/update/available");
    unique_id(uid, sizeof(uid), "firmware_update_available");
    device_json(dev, sizeof(dev));

    snprintf(payload, sizeof(payload),
             "{\"name\":\"Firmware Update Available\",\"unique_id\":\"%s\","
             "\"state_topic\":\"%s\",\"payload_on\":\"ON\",\"payload_off\":\"OFF\","
             "\"availability_topic\":\"%s\",\"icon\":\"mdi:update\",%s}",
             uid, state_topic, mqtt_state.base_topic, dev);
    mqtt_publish_raw(topic, payload, 1);
}

static void publish_firmware_update_version_config(void) {
    char topic[256];
    char state_topic[256];
    char uid[192];
    char dev[512];
    char payload[1536];

    discovery_topic(topic, sizeof(topic), "sensor", "firmware_update_version");
    topic_path(state_topic, sizeof(state_topic), "firmware/update/version");
    unique_id(uid, sizeof(uid), "firmware_update_version");
    device_json(dev, sizeof(dev));

    snprintf(payload, sizeof(payload),
             "{\"name\":\"Firmware Update Version\",\"unique_id\":\"%s\","
             "\"state_topic\":\"%s\",\"availability_topic\":\"%s\","
             "\"icon\":\"mdi:tag\",%s}",
             uid, state_topic, mqtt_state.base_topic, dev);
    mqtt_publish_raw(topic, payload, 1);
}

static void publish_video_switch_config(void) {
    char topic[256];
    char state_topic[256];
    char command_topic[256];
    char uid[192];
    char dev[512];
    char payload[1536];

    discovery_topic(topic, sizeof(topic), "switch", "video_enabled");
    topic_path(state_topic, sizeof(state_topic), "video/enabled");
    topic_path(command_topic, sizeof(command_topic), "video/enabled/set");
    unique_id(uid, sizeof(uid), "video_enabled");
    device_json(dev, sizeof(dev));

    snprintf(payload, sizeof(payload),
             "{\"name\":\"Video Enabled\",\"unique_id\":\"%s\","
             "\"state_topic\":\"%s\",\"command_topic\":\"%s\","
             "\"availability_topic\":\"%s\",\"payload_on\":\"ON\","
             "\"payload_off\":\"OFF\",\"icon\":\"mdi:video\",%s}",
             uid, state_topic, command_topic, mqtt_state.base_topic, dev);
    mqtt_publish_raw(topic, payload, 1);
}

static void publish_call_forward_switch_config(void) {
    char topic[256];
    char state_topic[256];
    char command_topic[256];
    char uid[192];
    char dev[512];
    char payload[1536];

    discovery_topic(topic, sizeof(topic), "switch", "call_forward_enabled");
    topic_path(state_topic, sizeof(state_topic), "call_forward/enabled");
    topic_path(command_topic, sizeof(command_topic), "call_forward/enabled/set");
    unique_id(uid, sizeof(uid), "call_forward_enabled");
    device_json(dev, sizeof(dev));

    snprintf(payload, sizeof(payload),
             "{\"name\":\"Call Forward Enabled\",\"unique_id\":\"%s\","
             "\"state_topic\":\"%s\",\"command_topic\":\"%s\","
             "\"availability_topic\":\"%s\",\"payload_on\":\"ON\","
             "\"payload_off\":\"OFF\",\"icon\":\"mdi:phone-forward\",%s}",
             uid, state_topic, command_topic, mqtt_state.base_topic, dev);
    mqtt_publish_raw(topic, payload, 1);
}

static void publish_unlock_binary_sensor_config(void) {
    char topic[256];
    char state_topic[256];
    char uid[192];
    char dev[512];
    char payload[1536];

    discovery_topic(topic, sizeof(topic), "binary_sensor", "door_unlocked");
    topic_path(state_topic, sizeof(state_topic), "door/unlocked");
    unique_id(uid, sizeof(uid), "door_unlocked");
    device_json(dev, sizeof(dev));

    snprintf(payload, sizeof(payload),
             "{\"name\":\"Door Unlocked\",\"unique_id\":\"%s\","
             "\"state_topic\":\"%s\",\"payload_on\":\"ON\",\"payload_off\":\"OFF\","
             "\"availability_topic\":\"%s\",\"icon\":\"mdi:lock-open\",%s}",
             uid, state_topic, mqtt_state.base_topic, dev);
    mqtt_publish_raw(topic, payload, 1);
}

static void clear_legacy_entities(void) {
    char topic[256];

    discovery_topic(topic, sizeof(topic), "binary_sensor", "ringing");
    clear_retained_topic(topic);
    discovery_topic(topic, sizeof(topic), "binary_sensor", "call_active");
    clear_retained_topic(topic);
    discovery_topic(topic, sizeof(topic), "binary_sensor", "sip_call_active");
    clear_retained_topic(topic);
    discovery_topic(topic, sizeof(topic), "binary_sensor", "video_active");
    clear_retained_topic(topic);

    topic_path(topic, sizeof(topic), "call/active");
    clear_retained_topic(topic);
    topic_path(topic, sizeof(topic), "sip/active");
    clear_retained_topic(topic);
    topic_path(topic, sizeof(topic), "video/active");
    clear_retained_topic(topic);
    topic_path(topic, sizeof(topic), "ringing/last");
    clear_retained_topic(topic);
    discovery_topic(topic, sizeof(topic), "sensor", "last_unlock");
    clear_retained_topic(topic);
    topic_path(topic, sizeof(topic), "door/last_unlock");
    clear_retained_topic(topic);
}

static void clear_firmware_update_entities(void) {
    char topic[256];

    discovery_topic(topic, sizeof(topic), "button", "firmware_update_install");
    clear_retained_topic(topic);
    discovery_topic(topic, sizeof(topic), "button", "firmware_update_refresh");
    clear_retained_topic(topic);
    discovery_topic(topic, sizeof(topic), "binary_sensor", "firmware_update_available");
    clear_retained_topic(topic);
    discovery_topic(topic, sizeof(topic), "sensor", "firmware_update_version");
    clear_retained_topic(topic);

    topic_path(topic, sizeof(topic), "firmware/update/install");
    clear_retained_topic(topic);
    topic_path(topic, sizeof(topic), "firmware/update/install/availability");
    clear_retained_topic(topic);
    topic_path(topic, sizeof(topic), "firmware/update/available");
    clear_retained_topic(topic);
    topic_path(topic, sizeof(topic), "firmware/update/version");
    clear_retained_topic(topic);
}

static int read_command_output(const char* command, char* output, size_t output_size) {
    FILE* fp;
    char line[256];
    size_t used = 0;

    if (!command || !output || output_size == 0) {
        return -1;
    }

    output[0] = '\0';
    fp = popen(command, "r");
    if (!fp) {
        return -1;
    }
    while (fgets(line, sizeof(line), fp)) {
        size_t len = strlen(line);
        if (used + len >= output_size) {
            len = output_size - used - 1;
        }
        if ((int)len <= 0) {
            break;
        }
        memcpy(output + used, line, len);
        used += len;
        output[used] = '\0';
        if (used + 1 >= output_size) {
            break;
        }
    }
    pclose(fp);
    return output[0] ? 0 : -1;
}

static void publish_firmware_update_install_availability(void) {
    int available = mqtt_state.firmware_update_available && !mqtt_state.firmware_update_installing;
    publish_suffix("firmware/update/install/availability", available ? "online" : "offline", 1);
}

static void firmware_update_check_and_publish(void) {
    char remote_version[64];
    char download_url[512];
    char local_version[64];
    char status[2048];
    char* line;
    char* saveptr;
    FILE* fp;

    if (!mqtt_state.firmware_update_enabled || !mqtt_state.connected) {
        return;
    }

    mqtt_state.firmware_update_last_check = time(NULL);
    local_version[0] = '\0';
    fp = fopen("/etc/wibox-release", "r");
    if (fp) {
        while (fgets(local_version, sizeof(local_version), fp)) {
            if (strncmp(local_version, "WIBOX_VERSION=", 14) == 0) {
                memmove(local_version, local_version + 14, strlen(local_version + 14) + 1);
                local_version[strcspn(local_version, "\r\n")] = '\0';
                break;
            }
            local_version[0] = '\0';
        }
        fclose(fp);
    } else {
        strncpy(local_version, WIBOX_VERSION, sizeof(local_version) - 1);
        local_version[sizeof(local_version) - 1] = '\0';
    }

    remote_version[0] = '\0';
    download_url[0] = '\0';
    if (read_command_output("/usr/bin/firmware_update --status 2>/dev/null", status, sizeof(status)) != 0) {
        printf("%s: firmware update check failed for repo %s\n", MQTT_FILE,
               mqtt_state.firmware_update_repo);
        return;
    }

    for (line = strtok_r(status, "\n", &saveptr); line; line = strtok_r(NULL, "\n", &saveptr)) {
        if (strncmp(line, "remote_version=", 15) == 0) {
            strncpy(remote_version, line + 15, sizeof(remote_version) - 1);
            remote_version[sizeof(remote_version) - 1] = '\0';
        } else if (strncmp(line, "local_version=", 14) == 0) {
            strncpy(local_version, line + 14, sizeof(local_version) - 1);
            local_version[sizeof(local_version) - 1] = '\0';
        } else if (strncmp(line, "available=", 10) == 0) {
            mqtt_state.firmware_update_available = atoi(line + 10) != 0;
        } else if (strncmp(line, "image_url=", 10) == 0) {
            strncpy(download_url, line + 10, sizeof(download_url) - 1);
            download_url[sizeof(download_url) - 1] = '\0';
        }
    }
    if (!remote_version[0]) {
        printf("%s: firmware update status missing remote version\n", MQTT_FILE);
        return;
    }

    strncpy(mqtt_state.firmware_update_version, remote_version, sizeof(mqtt_state.firmware_update_version) - 1);
    mqtt_state.firmware_update_version[sizeof(mqtt_state.firmware_update_version) - 1] = '\0';
    publish_suffix("firmware/update/version", mqtt_state.firmware_update_version, 1);
    publish_suffix("firmware/update/available", mqtt_state.firmware_update_available ? "ON" : "OFF", 1);
    publish_firmware_update_install_availability();
    printf("%s: firmware update check local=%s remote=%s available=%d url=%s\n",
           MQTT_FILE, local_version, remote_version, mqtt_state.firmware_update_available, download_url);
}

static void start_firmware_update_install(void) {
    int rc;

    if (!mqtt_state.firmware_update_enabled || !mqtt_state.connected) {
        return;
    }
    if (mqtt_state.firmware_update_installing) {
        printf("%s: firmware update install already in progress, ignoring duplicate request\n", MQTT_FILE);
        return;
    }

    mqtt_state.firmware_update_installing = 1;
    publish_firmware_update_install_availability();
    rc = system("/usr/bin/firmware_update >/tmp/firmware_update.log 2>&1 &");
    if (rc != 0) {
        printf("%s: failed to launch firmware update script rc=%d\n", MQTT_FILE, rc);
        mqtt_state.firmware_update_installing = 0;
        publish_firmware_update_install_availability();
    } else {
        printf("%s: launched firmware update script\n", MQTT_FILE);
    }
}

void mqtt_publish_discovery(void) {
    if (!mqtt_state.enabled || !mqtt_state.connected) return;

    clear_legacy_entities();
    if (mqtt_state.firmware_update_enabled) {
        publish_firmware_update_button_config();
        publish_firmware_update_refresh_button_config();
        publish_firmware_update_binary_sensor_config();
        publish_firmware_update_version_config();
    } else {
        clear_firmware_update_entities();
    }
    publish_button_config();
    publish_sensor_config("media_state", "Media State", "media/state", "", "phone");
    publish_sensor_config("firmware_version", "Firmware Version", "firmware/version", "", "tag");
    publish_sensor_config("firmware_commit", "Firmware Commit", "firmware/commit", "", "source-commit");
    publish_sensor_config("firmware_build_timestamp", "Firmware Build Timestamp",
                          "firmware/build_timestamp", "timestamp", "clock-outline");
    publish_unlock_binary_sensor_config();
    publish_sensor_config("wifi_rssi", "WiFi RSSI", "wifi/rssi", "signal_strength", "wifi");
    publish_video_switch_config();
    publish_call_forward_switch_config();
}

void mqtt_publish_online(void) {
    if (!mqtt_state.enabled || !mqtt_state.connected) return;
    mqtt_publish_raw(mqtt_state.base_topic, "online", 1);
}

void mqtt_publish_offline(void) {
    if (!mqtt_state.enabled || !mqtt_state.connected) return;
    mqtt_publish_raw(mqtt_state.base_topic, "offline", 1);
}

void mqtt_publish_ringing(int active) {
    (void)active;
}

void mqtt_publish_call_active(int active) {
    (void)active;
}

void mqtt_publish_sip_call_active(int active) {
    (void)active;
}

void mqtt_publish_video_active(int active) {
    (void)active;
}

void mqtt_publish_video_enabled(int enabled) {
    publish_suffix("video/enabled", enabled ? "ON" : "OFF", 1);
}

void mqtt_publish_call_forward_enabled(int enabled) {
    mqtt_state.call_forward_enabled = enabled ? 1 : 0;
    publish_suffix("call_forward/enabled", enabled ? "ON" : "OFF", 1);
}

void mqtt_publish_media_state(const char* state) {
    publish_suffix("media/state", state ? state : "unknown", 1);
}

void mqtt_publish_firmware_version(void) {
    publish_suffix("firmware/version", WIBOX_VERSION, 1);
    publish_suffix("firmware/commit", WIBOX_COMMIT, 1);
    publish_suffix("firmware/build_timestamp", WIBOX_BUILD_TIMESTAMP, 1);
}

void mqtt_publish_door_unlocked_pulse(void) {
    publish_suffix("door/unlocked", "ON", 1);
    sleep(1);
    publish_suffix("door/unlocked", "OFF", 1);
}

void mqtt_publish_wifi_stats(void) {
    FILE* fp;
    char line[128];
    char rssi[32] = "";

    fp = popen("/usr/sbin/wpa_cli -i wlan0 signal_poll 2>/dev/null", "r");
    if (!fp) {
        fp = popen("wpa_cli -i wlan0 signal_poll 2>/dev/null", "r");
    }
    if (!fp) {
        return;
    }
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "RSSI=", 5) == 0) {
            strncpy(rssi, line + 5, sizeof(rssi) - 1);
            rssi[strcspn(rssi, "\r\n")] = '\0';
            break;
        }
    }
    pclose(fp);

    if (rssi[0]) {
        publish_suffix("wifi/rssi", rssi, 1);
    }
}

static int payload_is_on(const char* payload) {
    return payload &&
        (strcasecmp(payload, "ON") == 0 ||
         strcasecmp(payload, "PRESS") == 0 ||
         strcasecmp(payload, "OPEN") == 0 ||
         strcmp(payload, "1") == 0);
}

static int payload_is_off(const char* payload) {
    return payload &&
        (strcasecmp(payload, "OFF") == 0 ||
         strcasecmp(payload, "0") == 0);
}

static void handle_mqtt_message(const char* topic, const char* payload) {
    char expected[256];

    if (!topic || !payload) return;

    if (strcmp(topic, mqtt_state.base_topic) == 0) {
        if (strcasecmp(payload, "CONFIG") == 0 || strcasecmp(payload, "INIT") == 0) {
            printf("%s: config request received\n", MQTT_FILE);
            mqtt_publish_discovery();
            mqtt_publish_online();
            mqtt_publish_firmware_version();
            if (mqtt_state.firmware_update_enabled) {
                firmware_update_check_and_publish();
            }
        }
        return;
    }

    topic_path(expected, sizeof(expected), "door/open/set");
    if (strcmp(topic, expected) == 0) {
        if (payload_is_on(payload) && mqtt_state.callbacks.open_door) {
            printf("%s: open door command received\n", MQTT_FILE);
            mqtt_state.callbacks.open_door(mqtt_state.user_data);
        }
        return;
    }

    topic_path(expected, sizeof(expected), "video/enabled/set");
    if (strcmp(topic, expected) == 0) {
        if (payload_is_on(payload) && mqtt_state.callbacks.set_video_enabled) {
            mqtt_state.callbacks.set_video_enabled(1, mqtt_state.user_data);
        } else if (payload_is_off(payload) && mqtt_state.callbacks.set_video_enabled) {
            mqtt_state.callbacks.set_video_enabled(0, mqtt_state.user_data);
        }
        return;
    }

    topic_path(expected, sizeof(expected), "call_forward/enabled/set");
    if (strcmp(topic, expected) == 0) {
        if (payload_is_on(payload) && mqtt_state.callbacks.set_call_forward_enabled) {
            mqtt_state.callbacks.set_call_forward_enabled(1, mqtt_state.user_data);
        } else if (payload_is_off(payload) && mqtt_state.callbacks.set_call_forward_enabled) {
            mqtt_state.callbacks.set_call_forward_enabled(0, mqtt_state.user_data);
        }
        return;
    }

    topic_path(expected, sizeof(expected), "firmware/update/install/set");
    if (strcmp(topic, expected) == 0) {
        if (payload_is_on(payload)) {
            printf("%s: firmware update install requested\n", MQTT_FILE);
            start_firmware_update_install();
        }
        return;
    }

    topic_path(expected, sizeof(expected), "firmware/update/check/set");
    if (strcmp(topic, expected) == 0) {
        if (payload_is_on(payload) && mqtt_state.firmware_update_enabled) {
            if (mqtt_state.firmware_update_installing) {
                printf("%s: firmware update refresh ignored while install is in progress\n", MQTT_FILE);
                return;
            }
            printf("%s: firmware update refresh requested\n", MQTT_FILE);
            firmware_update_check_and_publish();
        }
        return;
    }
}

static int mqtt_subscribe_topics(void) {
    unsigned char packet[512];
    unsigned char response[64];
    unsigned char type;
    size_t pos = 0;
    int len = 0;
    char wildcard[256];

    if (++mqtt_state.packet_id == 0) mqtt_state.packet_id = 1;
    snprintf(wildcard, sizeof(wildcard), "%s/#", mqtt_state.base_topic);

    if (mqtt_append_u16(packet, sizeof(packet), &pos, mqtt_state.packet_id) < 0 ||
        mqtt_append_str(packet, sizeof(packet), &pos, mqtt_state.base_topic) < 0 ||
        mqtt_append_bytes(packet, sizeof(packet), &pos, (const unsigned char*)"\x00", 1) < 0 ||
        mqtt_append_str(packet, sizeof(packet), &pos, wildcard) < 0 ||
        mqtt_append_bytes(packet, sizeof(packet), &pos, (const unsigned char*)"\x00", 1) < 0) {
        return -1;
    }

    if (mqtt_send_packet(0x82, packet, pos) < 0 ||
        mqtt_read_packet(&type, response, sizeof(response), &len) < 0 ||
        type != 0x90 || len < 3 || response[2] == 0x80) {
        return -1;
    }

    return 0;
}

static void mqtt_handle_publish(const unsigned char* payload, int len) {
    unsigned short topic_len;
    char topic[256];
    char message[512];
    int message_len;

    if (len < 2) return;
    topic_len = (unsigned short)((payload[0] << 8) | payload[1]);
    if (topic_len == 0 || topic_len >= sizeof(topic) || 2 + topic_len > len) return;

    memcpy(topic, payload + 2, topic_len);
    topic[topic_len] = '\0';

    message_len = len - 2 - topic_len;
    if (message_len < 0) return;
    if (message_len >= (int)sizeof(message)) message_len = (int)sizeof(message) - 1;
    memcpy(message, payload + 2 + topic_len, (size_t)message_len);
    message[message_len] = '\0';

    handle_mqtt_message(topic, message);
}

static int mqtt_process_once(void) {
    fd_set rfds;
    struct timeval tv;
    int ret;
    unsigned char type;
    unsigned char payload[2048];
    int len = 0;

    FD_ZERO(&rfds);
    FD_SET(mqtt_state.sock, &rfds);
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    ret = select(mqtt_state.sock + 1, &rfds, NULL, NULL, &tv);
    if (ret < 0) {
        return errno == EINTR ? 0 : -1;
    }
    if (ret == 0) {
        return 0;
    }

    if (mqtt_read_packet(&type, payload, sizeof(payload), &len) < 0) {
        return -1;
    }

    switch (type & 0xf0) {
    case 0x30:
        mqtt_handle_publish(payload, len);
        break;
    case 0xd0: /* PINGRESP */
        break;
    default:
        break;
    }

    return 0;
}

static void* mqtt_thread_func(void* arg) {
    (void)arg;

    while (mqtt_state.running) {
        time_t last_ping;

        if (mqtt_connect_session() != 0 || mqtt_subscribe_topics() != 0) {
            mqtt_state.connected = 0;
            if (mqtt_state.sock >= 0) {
                close(mqtt_state.sock);
                mqtt_state.sock = -1;
            }
            printf("%s: broker unavailable or unauthorized, retrying later\n", MQTT_FILE);
            sleep(30);
            continue;
        }

        mqtt_state.connected = 1;
        last_ping = time(NULL);
        printf("%s: connected topic=%s\n", MQTT_FILE, mqtt_state.base_topic);

        mqtt_publish_discovery();
        mqtt_publish_online();
        mqtt_publish_media_state("idle");
        mqtt_publish_firmware_version();
        if (mqtt_state.firmware_update_enabled) {
            firmware_update_check_and_publish();
        }
        publish_suffix("door/unlocked", "OFF", 1);
        mqtt_publish_video_enabled(mqtt_state.video_enabled);
        mqtt_publish_call_forward_enabled(mqtt_state.call_forward_enabled);
        mqtt_publish_wifi_stats();

        while (mqtt_state.running) {
            time_t now = time(NULL);
            if (mqtt_process_once() < 0) {
                break;
            }
            if (mqtt_state.firmware_update_enabled &&
                mqtt_state.firmware_update_last_check != 0 &&
                now - mqtt_state.firmware_update_last_check >= 86400) {
                firmware_update_check_and_publish();
            }
            if (now - last_ping >= 30) {
                if (mqtt_send_packet(0xc0, (const unsigned char*)"", 0) < 0) {
                    break;
                }
                last_ping = now;
            }
        }

        mqtt_state.connected = 0;
        if (mqtt_state.sock >= 0) {
            close(mqtt_state.sock);
            mqtt_state.sock = -1;
        }
        if (mqtt_state.running) {
            printf("%s: connection stopped, retrying\n", MQTT_FILE);
            sleep(5);
        }
    }

    return NULL;
}

int mqtt_init(const wibox_config_t* app_config, const char* local_ip,
              const mqtt_callbacks_t* callbacks, void* user_data) {
    char hostname[128];

    memset(&mqtt_state, 0, sizeof(mqtt_state));
    mqtt_state.sock = -1;
    pthread_mutex_init(&mqtt_state.io_mutex, NULL);

    if (!app_config || !app_config->mqtt_enabled) {
        mqtt_state.enabled = 0;
        return 0;
    }

    mqtt_state.enabled = 1;
    strncpy(mqtt_state.host, app_config->mqtt_host, sizeof(mqtt_state.host) - 1);
    strncpy(mqtt_state.user, app_config->mqtt_user, sizeof(mqtt_state.user) - 1);
    strncpy(mqtt_state.pass, app_config->mqtt_pass, sizeof(mqtt_state.pass) - 1);
    strncpy(mqtt_state.ha_prefix, app_config->mqtt_homeassistant_prefix, sizeof(mqtt_state.ha_prefix) - 1);
    strncpy(mqtt_state.local_ip, local_ip ? local_ip : "0.0.0.0", sizeof(mqtt_state.local_ip) - 1);
    mqtt_state.video_enabled = app_config->video_enabled;
    mqtt_state.call_forward_enabled = app_config->serial_listener_enabled ? 1 : 0;
    mqtt_state.firmware_update_enabled = app_config->firmware_update_enabled;
    strncpy(mqtt_state.firmware_update_repo, app_config->firmware_update_repo,
            sizeof(mqtt_state.firmware_update_repo) - 1);
    mqtt_state.firmware_update_last_check = 0;
    mqtt_state.firmware_update_installing = 0;

    get_hostname(hostname, sizeof(hostname));
    if (app_config->mqtt_device_id[0]) {
        normalize_id(app_config->mqtt_device_id, mqtt_state.device_id, sizeof(mqtt_state.device_id));
    } else {
        normalize_id(hostname, mqtt_state.device_id, sizeof(mqtt_state.device_id));
    }

    if (app_config->mqtt_device_name[0]) {
        strncpy(mqtt_state.device_name, app_config->mqtt_device_name, sizeof(mqtt_state.device_name) - 1);
    } else {
        snprintf(mqtt_state.device_name, sizeof(mqtt_state.device_name), "WiBox %.121s", hostname);
    }

    if (app_config->mqtt_base_topic[0]) {
        strncpy(mqtt_state.base_topic, app_config->mqtt_base_topic, sizeof(mqtt_state.base_topic) - 1);
    } else {
        snprintf(mqtt_state.base_topic, sizeof(mqtt_state.base_topic), "wibox/%.121s", hostname);
    }

    if (callbacks) {
        mqtt_state.callbacks = *callbacks;
    }
    mqtt_state.user_data = user_data;

    printf("%s: initialized host=%s topic=%s\n",
           MQTT_FILE, mqtt_state.host, mqtt_state.base_topic);
    return 0;
}

int mqtt_start(void) {
    if (!mqtt_state.enabled) {
        printf("%s: disabled\n", MQTT_FILE);
        return 0;
    }

    mqtt_state.running = 1;
    if (pthread_create(&mqtt_state.thread, NULL, mqtt_thread_func, NULL) != 0) {
        printf("%s: failed to create thread: %s\n", MQTT_FILE, strerror(errno));
        mqtt_state.running = 0;
        return -1;
    }

    return 0;
}

void mqtt_stop(void) {
    if (!mqtt_state.enabled || !mqtt_state.running) {
        return;
    }

    mqtt_publish_offline();
    mqtt_state.connected = 0;
    mqtt_state.running = 0;
    if (mqtt_state.sock >= 0) {
        shutdown(mqtt_state.sock, SHUT_RDWR);
    }
    pthread_join(mqtt_state.thread, NULL);
    if (mqtt_state.sock >= 0) {
        close(mqtt_state.sock);
        mqtt_state.sock = -1;
    }
}

int mqtt_is_connected(void) {
    return mqtt_state.enabled && mqtt_state.connected;
}
