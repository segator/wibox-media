#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define THIS_FILE "config"

// Helper function to trim whitespace
static char* trim_whitespace(char* str) {
    char* end;

    // Trim leading space
    while (isspace((unsigned char)*str)) str++;

    if (*str == 0) return str;

    // Trim trailing space
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;

    // Write new null terminator
    end[1] = '\0';

    return str;
}

void config_init_defaults(wibox_config_t* config) {
    if (!config) return;

    // SIP Configuration
    strcpy(config->outgoing_call_target, "sip:1000@192.168.0.31:5060");
    config->outgoing_call_timeout = 60;
    config->sip_port = 5060;
    config->rtp_port = 8000;
    config->video_enabled = 1;
    config->video_rtp_port = 8002;
    config->video_payload_type = 96;

    // Pipe Configuration
    strcpy(config->sip_listen_pipe, "/tmp/pipe_sip");

    // Message Configuration
    strcpy(config->ding_message, "DING");

    // Intercom serial listener
    config->serial_listener_enabled = 1;
    strcpy(config->intercom_device, "/dev/ttySGK1");

    // MQTT/Home Assistant
    config->mqtt_enabled = 1;
    strcpy(config->mqtt_host, "127.0.0.1");
    config->mqtt_user[0] = '\0';
    config->mqtt_pass[0] = '\0';
    strcpy(config->mqtt_homeassistant_prefix, "homeassistant");
    config->mqtt_base_topic[0] = '\0';
    config->mqtt_device_id[0] = '\0';
    config->mqtt_device_name[0] = '\0';
    strcpy(config->mqtt_timestamp_offset, "+00:00");

    // Prometheus metrics exporter
    config->prometheus_enabled = 1;
    config->prometheus_port = 9617;

    // Audio Configuration
    config->audio_buffer_size = 160;
    config->audio_chip_gpio = 18;
}

static int parse_config_line(const char* line, wibox_config_t* config) {
    char key[256], value[256];
    char* equals_pos;
    char line_copy[512];

    // Make a copy of the line for processing
    strncpy(line_copy, line, sizeof(line_copy) - 1);
    line_copy[sizeof(line_copy) - 1] = '\0';

    // Skip comments and empty lines
    char* trimmed = trim_whitespace(line_copy);
    if (trimmed[0] == '#' || trimmed[0] == '\0') {
        return 0; // Skip this line
    }

    // Find the equals sign
    equals_pos = strchr(trimmed, '=');
    if (!equals_pos) {
        printf("Invalid config line (no '='): %s\n", line);
        return -1;
    }

    // Split key and value
    *equals_pos = '\0';
    strcpy(key, trim_whitespace(trimmed));
    strcpy(value, trim_whitespace(equals_pos + 1));

    // Remove quotes from value if present
    if ((value[0] == '"' && value[strlen(value)-1] == '"') ||
        (value[0] == '\'' && value[strlen(value)-1] == '\'')) {
        value[strlen(value)-1] = '\0';
        memmove(value, value + 1, strlen(value));
    }

    // Parse configuration values
    if (strcmp(key, "outgoing_call_target") == 0) {
        strncpy(config->outgoing_call_target, value, sizeof(config->outgoing_call_target) - 1);
        config->outgoing_call_target[sizeof(config->outgoing_call_target) - 1] = '\0';
    } else if (strcmp(key, "outgoing_call_timeout") == 0) {
        config->outgoing_call_timeout = atoi(value);
    } else if (strcmp(key, "sip_port") == 0) {
        config->sip_port = atoi(value);
    } else if (strcmp(key, "rtp_port") == 0) {
        config->rtp_port = atoi(value);
    } else if (strcmp(key, "video_enabled") == 0) {
        config->video_enabled = atoi(value);
    } else if (strcmp(key, "video_rtp_port") == 0) {
        config->video_rtp_port = atoi(value);
    } else if (strcmp(key, "video_payload_type") == 0) {
        config->video_payload_type = atoi(value);
    } else if (strcmp(key, "video_bridge_path") == 0) {
        return 0; /* legacy standalone video bridge config, ignored */
    } else if (strcmp(key, "audio_ai_pipe") == 0) {
        return 0; /* legacy named-pipe config, ignored */
    } else if (strcmp(key, "audio_ao_pipe") == 0) {
        return 0; /* legacy named-pipe config, ignored */
    } else if (strcmp(key, "sip_listen_pipe") == 0) {
        strncpy(config->sip_listen_pipe, value, sizeof(config->sip_listen_pipe) - 1);
        config->sip_listen_pipe[sizeof(config->sip_listen_pipe) - 1] = '\0';
    } else if (strcmp(key, "audio_bridge_pipe") == 0) {
        return 0; /* legacy named-pipe config, ignored */
    } else if (strcmp(key, "ding_message") == 0) {
        strncpy(config->ding_message, value, sizeof(config->ding_message) - 1);
        config->ding_message[sizeof(config->ding_message) - 1] = '\0';
    } else if (strcmp(key, "serial_listener_enabled") == 0) {
        config->serial_listener_enabled = atoi(value);
    } else if (strcmp(key, "intercom_device") == 0) {
        strncpy(config->intercom_device, value, sizeof(config->intercom_device) - 1);
        config->intercom_device[sizeof(config->intercom_device) - 1] = '\0';
    } else if (strcmp(key, "mqtt_enabled") == 0) {
        config->mqtt_enabled = atoi(value);
    } else if (strcmp(key, "mqtt_host") == 0) {
        strncpy(config->mqtt_host, value, sizeof(config->mqtt_host) - 1);
        config->mqtt_host[sizeof(config->mqtt_host) - 1] = '\0';
    } else if (strcmp(key, "mqtt_user") == 0) {
        strncpy(config->mqtt_user, value, sizeof(config->mqtt_user) - 1);
        config->mqtt_user[sizeof(config->mqtt_user) - 1] = '\0';
    } else if (strcmp(key, "mqtt_pass") == 0) {
        strncpy(config->mqtt_pass, value, sizeof(config->mqtt_pass) - 1);
        config->mqtt_pass[sizeof(config->mqtt_pass) - 1] = '\0';
    } else if (strcmp(key, "mqtt_homeassistant_prefix") == 0) {
        strncpy(config->mqtt_homeassistant_prefix, value, sizeof(config->mqtt_homeassistant_prefix) - 1);
        config->mqtt_homeassistant_prefix[sizeof(config->mqtt_homeassistant_prefix) - 1] = '\0';
    } else if (strcmp(key, "mqtt_base_topic") == 0) {
        strncpy(config->mqtt_base_topic, value, sizeof(config->mqtt_base_topic) - 1);
        config->mqtt_base_topic[sizeof(config->mqtt_base_topic) - 1] = '\0';
    } else if (strcmp(key, "mqtt_device_id") == 0) {
        strncpy(config->mqtt_device_id, value, sizeof(config->mqtt_device_id) - 1);
        config->mqtt_device_id[sizeof(config->mqtt_device_id) - 1] = '\0';
    } else if (strcmp(key, "mqtt_device_name") == 0) {
        strncpy(config->mqtt_device_name, value, sizeof(config->mqtt_device_name) - 1);
        config->mqtt_device_name[sizeof(config->mqtt_device_name) - 1] = '\0';
    } else if (strcmp(key, "mqtt_timestamp_offset") == 0) {
        strncpy(config->mqtt_timestamp_offset, value, sizeof(config->mqtt_timestamp_offset) - 1);
        config->mqtt_timestamp_offset[sizeof(config->mqtt_timestamp_offset) - 1] = '\0';
    } else if (strcmp(key, "prometheus_enabled") == 0) {
        config->prometheus_enabled = atoi(value);
    } else if (strcmp(key, "prometheus_port") == 0) {
        config->prometheus_port = atoi(value);
    } else if (strcmp(key, "mqtt_pub_path") == 0) {
        return 0; /* legacy shell-client config, ignored */
    } else if (strcmp(key, "mqtt_sub_path") == 0) {
        return 0; /* legacy shell-client config, ignored */
    } else if (strcmp(key, "audio_buffer_size") == 0) {
        config->audio_buffer_size = atoi(value);
    } else if (strcmp(key, "audio_chip_gpio") == 0) {
        config->audio_chip_gpio = atoi(value);
    } else if (strcmp(key, "pipe_retry_interval_ms") == 0) {
        return 0; /* legacy named-pipe config, ignored */
    } else if (strcmp(key, "pipe_retry_max_attempts") == 0) {
        return 0; /* legacy named-pipe config, ignored */
    } else {
        printf("Warning: ignoring unknown configuration key: %s\n", key);
        return 0;
    }

    return 0;
}

int config_load(const char* config_file, wibox_config_t* config) {
    FILE* fp;
    char line[512];
    int line_num = 0;
    int errors = 0;

    if (!config_file || !config) {
        printf("Invalid parameters to config_load\n");
        return -1;
    }

    // Initialize with defaults first
    config_init_defaults(config);

    // Try to open config file
    fp = fopen(config_file, "r");
    if (!fp) {
        printf("Warning: Could not open config file %s, using defaults\n", config_file);
        return 0; // Not an error, just use defaults
    }

    printf("Loading configuration from %s\n", config_file);

    // Read line by line
    while (fgets(line, sizeof(line), fp)) {
        line_num++;

        // Remove newline
        line[strcspn(line, "\r\n")] = '\0';

        if (parse_config_line(line, config) < 0) {
            printf("Error parsing line %d: %s\n", line_num, line);
            errors++;
        }
    }

    fclose(fp);

    if (errors > 0) {
        printf("Configuration loaded with %d errors\n", errors);
        return -1;
    }

    printf("Configuration loaded successfully\n");
    return 0;
}

void config_print(const wibox_config_t* config) {
    if (!config) return;

    printf("=== Current Configuration ===\n");
    printf("outgoing_call_target = %s\n", config->outgoing_call_target);
    printf("outgoing_call_timeout = %d\n", config->outgoing_call_timeout);
    printf("sip_port = %d\n", config->sip_port);
    printf("rtp_port = %d\n", config->rtp_port);
    printf("video_enabled = %d\n", config->video_enabled);
    printf("video_rtp_port = %d\n", config->video_rtp_port);
    printf("video_payload_type = %d\n", config->video_payload_type);
    printf("sip_listen_pipe = %s\n", config->sip_listen_pipe);
    printf("ding_message = %s\n", config->ding_message);
    printf("serial_listener_enabled = %d\n", config->serial_listener_enabled);
    printf("intercom_device = %s\n", config->intercom_device);
    printf("mqtt_enabled = %d\n", config->mqtt_enabled);
    printf("mqtt_host = %s\n", config->mqtt_host);
    printf("mqtt_user = %s\n", config->mqtt_user);
    printf("mqtt_homeassistant_prefix = %s\n", config->mqtt_homeassistant_prefix);
    printf("mqtt_base_topic = %s\n", config->mqtt_base_topic);
    printf("mqtt_device_id = %s\n", config->mqtt_device_id);
    printf("mqtt_device_name = %s\n", config->mqtt_device_name);
    printf("mqtt_timestamp_offset = %s\n", config->mqtt_timestamp_offset);
    printf("prometheus_enabled = %d\n", config->prometheus_enabled);
    printf("prometheus_port = %d\n", config->prometheus_port);
    printf("audio_buffer_size = %d\n", config->audio_buffer_size);
    printf("audio_chip_gpio = %d\n", config->audio_chip_gpio);
    printf("============================\n");
}
