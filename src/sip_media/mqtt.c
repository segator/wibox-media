#include "mqtt.h"

#include <ctype.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define MQTT_FILE "mqtt"

typedef struct {
    int enabled;
    char host[128];
    char user[128];
    char pass[128];
    char ha_prefix[128];
    char base_topic[128];
    char device_id[128];
    char device_name[128];
    char pub_path[256];
    char sub_path[256];
    char local_ip[64];
    int video_enabled;
    mqtt_callbacks_t callbacks;
    void* user_data;
    pthread_t thread;
    int running;
    int connected;
    pid_t sub_pid;
} mqtt_state_t;

static mqtt_state_t mqtt_state;

static void shell_quote(const char* input, char* out, size_t out_size) {
    size_t pos = 0;
    const char* p;

    if (out_size == 0) return;
    out[pos++] = '\'';

    for (p = input ? input : ""; *p && pos + 5 < out_size; p++) {
        if (*p == '\'') {
            out[pos++] = '\'';
            out[pos++] = '\\';
            out[pos++] = '\'';
            out[pos++] = '\'';
        } else {
            out[pos++] = *p;
        }
    }

    if (pos + 1 < out_size) {
        out[pos++] = '\'';
    }
    out[pos] = '\0';
}

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

static void build_common_opts(char* out, size_t out_size, const char* client_suffix) {
    char q_id[192];
    char q_host[192];
    char q_user[192];
    char q_pass[192];
    char client_id[192];

    snprintf(client_id, sizeof(client_id), "wibox_%s_%s",
             mqtt_state.device_id, client_suffix ? client_suffix : "daemon");
    shell_quote(client_id, q_id, sizeof(q_id));
    shell_quote(mqtt_state.host, q_host, sizeof(q_host));
    snprintf(out, out_size, "-I %s -h %s", q_id, q_host);

    if (mqtt_state.user[0]) {
        shell_quote(mqtt_state.user, q_user, sizeof(q_user));
        strncat(out, " -u ", out_size - strlen(out) - 1);
        strncat(out, q_user, out_size - strlen(out) - 1);
    }
    if (mqtt_state.pass[0]) {
        shell_quote(mqtt_state.pass, q_pass, sizeof(q_pass));
        strncat(out, " -P ", out_size - strlen(out) - 1);
        strncat(out, q_pass, out_size - strlen(out) - 1);
    }
}

static int run_command(const char* command) {
    int rc;

    rc = system(command);
    if (rc == -1) {
        printf("%s: system failed: %s\n", MQTT_FILE, strerror(errno));
        return -1;
    }
    if (WIFEXITED(rc)) {
        return WEXITSTATUS(rc);
    }
    return -1;
}

static int mqtt_publish_raw(const char* topic, const char* payload, int retain) {
    char q_pub[320];
    char q_topic[256];
    char q_payload[2048];
    char opts[512];
    char cmd[4096];

    if (!mqtt_state.enabled || !topic || !payload) {
        return -1;
    }

    shell_quote(mqtt_state.pub_path, q_pub, sizeof(q_pub));
    shell_quote(topic, q_topic, sizeof(q_topic));
    shell_quote(payload, q_payload, sizeof(q_payload));
    build_common_opts(opts, sizeof(opts), "pub");

    snprintf(cmd, sizeof(cmd), "%s %s %s -t %s -m %s >/dev/null 2>&1",
             q_pub, retain ? "-r" : "", opts, q_topic, q_payload);
    return run_command(cmd);
}

static int mqtt_check_connection(void) {
    char q_pub[320];
    char q_topic[256];
    char opts[512];
    char cmd[2048];

    if (!mqtt_state.enabled) {
        return -1;
    }

    shell_quote(mqtt_state.pub_path, q_pub, sizeof(q_pub));
    shell_quote(mqtt_state.base_topic, q_topic, sizeof(q_topic));
    build_common_opts(opts, sizeof(opts), "probe");
    snprintf(cmd, sizeof(cmd), "%s %s -t %s -m init >/dev/null 2>&1",
             q_pub, opts, q_topic);
    return run_command(cmd);
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

static void iso_now(char* out, size_t out_size) {
    time_t now = time(NULL);
    struct tm tm_value;

    localtime_r(&now, &tm_value);
    strftime(out, out_size, "%Y-%m-%dT%H:%M:%S%z", &tm_value);
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

static void publish_binary_sensor_config(const char* object_id, const char* name,
                                         const char* state_suffix,
                                         const char* device_class) {
    char topic[256];
    char state_topic[256];
    char uid[192];
    char dev[512];
    char payload[1536];

    discovery_topic(topic, sizeof(topic), "binary_sensor", object_id);
    topic_path(state_topic, sizeof(state_topic), state_suffix);
    unique_id(uid, sizeof(uid), object_id);
    device_json(dev, sizeof(dev));

    snprintf(payload, sizeof(payload),
             "{\"name\":\"%s\",\"unique_id\":\"%s\",\"state_topic\":\"%s\","
             "\"availability_topic\":\"%s\",\"payload_on\":\"ON\","
             "\"payload_off\":\"OFF\",\"device_class\":\"%s\",%s}",
             name, uid, state_topic, mqtt_state.base_topic, device_class, dev);
    mqtt_publish_raw(topic, payload, 1);
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

void mqtt_publish_discovery(void) {
    if (!mqtt_state.enabled || !mqtt_state.connected) return;

    publish_button_config();
    publish_binary_sensor_config("ringing", "Ringing", "ringing", "occupancy");
    publish_binary_sensor_config("call_active", "Call Active", "call/active", "connectivity");
    publish_binary_sensor_config("sip_call_active", "SIP Call Active", "sip/active", "connectivity");
    publish_binary_sensor_config("video_active", "Video Active", "video/active", "connectivity");
    publish_sensor_config("media_state", "Media State", "media/state", "", "phone");
    publish_sensor_config("last_ring", "Last Ring", "ringing/last", "timestamp", "history");
    publish_sensor_config("last_unlock", "Last Unlock", "door/last_unlock", "timestamp", "lock-open");
    publish_sensor_config("wifi_rssi", "WiFi RSSI", "wifi/rssi", "signal_strength", "wifi");
    publish_video_switch_config();
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
    publish_suffix("ringing", active ? "ON" : "OFF", 1);
}

void mqtt_publish_call_active(int active) {
    publish_suffix("call/active", active ? "ON" : "OFF", 1);
}

void mqtt_publish_sip_call_active(int active) {
    publish_suffix("sip/active", active ? "ON" : "OFF", 1);
}

void mqtt_publish_video_active(int active) {
    publish_suffix("video/active", active ? "ON" : "OFF", 1);
}

void mqtt_publish_video_enabled(int enabled) {
    publish_suffix("video/enabled", enabled ? "ON" : "OFF", 1);
}

void mqtt_publish_media_state(const char* state) {
    publish_suffix("media/state", state ? state : "unknown", 1);
}

void mqtt_publish_last_ring(void) {
    char value[64];
    iso_now(value, sizeof(value));
    publish_suffix("ringing/last", value, 1);
}

void mqtt_publish_last_unlock(void) {
    char value[64];
    iso_now(value, sizeof(value));
    publish_suffix("door/last_unlock", value, 1);
}

void mqtt_publish_wifi_stats(void) {
    FILE* fp;
    char line[128];
    char cmd[512];
    char rssi[32] = "";

    fp = popen("wpa_cli -i wlan0 signal_poll 2>/dev/null", "r");
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

    (void)cmd;
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

static void handle_mqtt_line(char* line) {
    char* payload;
    char topic[256];
    char expected[256];

    line[strcspn(line, "\r\n")] = '\0';
    payload = strchr(line, ' ');
    if (!payload) {
        return;
    }
    *payload++ = '\0';
    strncpy(topic, line, sizeof(topic) - 1);
    topic[sizeof(topic) - 1] = '\0';

    if (strcmp(topic, mqtt_state.base_topic) == 0) {
        if (strcasecmp(payload, "CONFIG") == 0 || strcasecmp(payload, "INIT") == 0) {
            printf("%s: config request received\n", MQTT_FILE);
            mqtt_publish_discovery();
            mqtt_publish_online();
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
}

static FILE* start_subscriber(void) {
    int pipefd[2];
    pid_t pid;
    char opts[512];
    char q_sub[320];
    char q_base[256];
    char q_will[256];
    char will_topic[256];

    if (pipe(pipefd) != 0) {
        printf("%s: pipe failed: %s\n", MQTT_FILE, strerror(errno));
        return NULL;
    }

    pid = fork();
    if (pid < 0) {
        printf("%s: fork failed: %s\n", MQTT_FILE, strerror(errno));
        close(pipefd[0]);
        close(pipefd[1]);
        return NULL;
    }

    if (pid == 0) {
        char cmd[2048];

        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);

        shell_quote(mqtt_state.sub_path, q_sub, sizeof(q_sub));
        shell_quote(mqtt_state.base_topic, q_base, sizeof(q_base));
        snprintf(will_topic, sizeof(will_topic), "%s/#", mqtt_state.base_topic);
        shell_quote(will_topic, q_will, sizeof(q_will));
        build_common_opts(opts, sizeof(opts), "sub");
        snprintf(cmd, sizeof(cmd),
                 "exec %s %s -v -k 300 --will-topic %s --will-payload offline "
                 "--will-retain -t %s -t %s",
                 q_sub, opts, q_base, q_base, q_will);
        execl("/bin/sh", "sh", "-c", cmd, (char*)NULL);
        _exit(127);
    }

    close(pipefd[1]);
    mqtt_state.sub_pid = pid;
    return fdopen(pipefd[0], "r");
}

static void* mqtt_thread_func(void* arg) {
    while (mqtt_state.running) {
        FILE* fp;
        char line[512];

        if (mqtt_check_connection() != 0) {
            mqtt_state.connected = 0;
            printf("%s: broker unavailable or unauthorized, retrying later\n", MQTT_FILE);
            sleep(30);
            continue;
        }
        mqtt_state.connected = 1;

        fp = start_subscriber();
        if (!fp) {
            mqtt_state.connected = 0;
            sleep(5);
            continue;
        }

        printf("%s: subscriber started pid=%d topic=%s\n",
               MQTT_FILE, mqtt_state.sub_pid, mqtt_state.base_topic);

        mqtt_publish_discovery();
        mqtt_publish_online();
        mqtt_publish_ringing(0);
        mqtt_publish_call_active(0);
        mqtt_publish_sip_call_active(0);
        mqtt_publish_video_active(0);
        mqtt_publish_media_state("idle");
        mqtt_publish_video_enabled(mqtt_state.video_enabled);

        while (mqtt_state.running && fgets(line, sizeof(line), fp)) {
            handle_mqtt_line(line);
        }

        fclose(fp);
        mqtt_state.connected = 0;
        if (mqtt_state.sub_pid > 0) {
            int status;
            waitpid(mqtt_state.sub_pid, &status, WNOHANG);
            mqtt_state.sub_pid = -1;
        }
        if (mqtt_state.running) {
            printf("%s: subscriber stopped, retrying\n", MQTT_FILE);
            sleep(5);
        }
    }

    return NULL;
}

int mqtt_init(const wibox_config_t* app_config, const char* local_ip,
              const mqtt_callbacks_t* callbacks, void* user_data) {
    char hostname[128];

    memset(&mqtt_state, 0, sizeof(mqtt_state));
    mqtt_state.sub_pid = -1;

    if (!app_config || !app_config->mqtt_enabled) {
        mqtt_state.enabled = 0;
        return 0;
    }

    mqtt_state.enabled = 1;
    strncpy(mqtt_state.host, app_config->mqtt_host, sizeof(mqtt_state.host) - 1);
    strncpy(mqtt_state.user, app_config->mqtt_user, sizeof(mqtt_state.user) - 1);
    strncpy(mqtt_state.pass, app_config->mqtt_pass, sizeof(mqtt_state.pass) - 1);
    strncpy(mqtt_state.ha_prefix, app_config->mqtt_homeassistant_prefix, sizeof(mqtt_state.ha_prefix) - 1);
    strncpy(mqtt_state.pub_path, app_config->mqtt_pub_path, sizeof(mqtt_state.pub_path) - 1);
    strncpy(mqtt_state.sub_path, app_config->mqtt_sub_path, sizeof(mqtt_state.sub_path) - 1);
    strncpy(mqtt_state.local_ip, local_ip ? local_ip : "0.0.0.0", sizeof(mqtt_state.local_ip) - 1);
    mqtt_state.video_enabled = app_config->video_enabled;

    get_hostname(hostname, sizeof(hostname));
    if (app_config->mqtt_device_id[0]) {
        normalize_id(app_config->mqtt_device_id, mqtt_state.device_id, sizeof(mqtt_state.device_id));
    } else {
        normalize_id(hostname, mqtt_state.device_id, sizeof(mqtt_state.device_id));
    }

    if (app_config->mqtt_device_name[0]) {
        strncpy(mqtt_state.device_name, app_config->mqtt_device_name, sizeof(mqtt_state.device_name) - 1);
    } else {
        snprintf(mqtt_state.device_name, sizeof(mqtt_state.device_name), "WiBox %s", hostname);
    }

    if (app_config->mqtt_base_topic[0]) {
        strncpy(mqtt_state.base_topic, app_config->mqtt_base_topic, sizeof(mqtt_state.base_topic) - 1);
    } else {
        snprintf(mqtt_state.base_topic, sizeof(mqtt_state.base_topic), "wibox/%s", hostname);
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

    if (access(mqtt_state.pub_path, X_OK) != 0 || access(mqtt_state.sub_path, X_OK) != 0) {
        printf("%s: mosquitto clients not available, MQTT disabled\n", MQTT_FILE);
        mqtt_state.enabled = 0;
        return -1;
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
    if (mqtt_state.sub_pid > 0) {
        kill(mqtt_state.sub_pid, SIGTERM);
    }
    pthread_join(mqtt_state.thread, NULL);
    mqtt_state.sub_pid = -1;
}
