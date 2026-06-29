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
    strcpy(config->video_bridge_path, "/usr/bin/video_rtp_bridge");

    // Pipe Configuration
    strcpy(config->audio_ai_pipe, "/tmp/audio_ai_to_sip");
    strcpy(config->audio_ao_pipe, "/tmp/audio_sip_to_ao");
    strcpy(config->sip_listen_pipe, "/tmp/pipe_sip");
    strcpy(config->audio_bridge_pipe, "/tmp/pipe_audio");

    // Message Configuration
    strcpy(config->ding_message, "DING");

    // Audio Configuration
    config->audio_buffer_size = 160;
    config->pipe_retry_interval_ms = 2000;
    config->pipe_retry_max_attempts = 0;  // 0 = unlimited
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
        strncpy(config->video_bridge_path, value, sizeof(config->video_bridge_path) - 1);
        config->video_bridge_path[sizeof(config->video_bridge_path) - 1] = '\0';
    } else if (strcmp(key, "audio_ai_pipe") == 0) {
        strncpy(config->audio_ai_pipe, value, sizeof(config->audio_ai_pipe) - 1);
        config->audio_ai_pipe[sizeof(config->audio_ai_pipe) - 1] = '\0';
    } else if (strcmp(key, "audio_ao_pipe") == 0) {
        strncpy(config->audio_ao_pipe, value, sizeof(config->audio_ao_pipe) - 1);
        config->audio_ao_pipe[sizeof(config->audio_ao_pipe) - 1] = '\0';
    } else if (strcmp(key, "sip_listen_pipe") == 0) {
        strncpy(config->sip_listen_pipe, value, sizeof(config->sip_listen_pipe) - 1);
        config->sip_listen_pipe[sizeof(config->sip_listen_pipe) - 1] = '\0';
    } else if (strcmp(key, "audio_bridge_pipe") == 0) {
        strncpy(config->audio_bridge_pipe, value, sizeof(config->audio_bridge_pipe) - 1);
        config->audio_bridge_pipe[sizeof(config->audio_bridge_pipe) - 1] = '\0';
    } else if (strcmp(key, "ding_message") == 0) {
        strncpy(config->ding_message, value, sizeof(config->ding_message) - 1);
        config->ding_message[sizeof(config->ding_message) - 1] = '\0';
    } else if (strcmp(key, "audio_buffer_size") == 0) {
        config->audio_buffer_size = atoi(value);
    } else if (strcmp(key, "pipe_retry_interval_ms") == 0) {
        config->pipe_retry_interval_ms = atoi(value);
    } else if (strcmp(key, "pipe_retry_max_attempts") == 0) {
        config->pipe_retry_max_attempts = atoi(value);
    } else {
        printf("Unknown configuration key: %s\n", key);
        return -1;
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
    printf("video_bridge_path = %s\n", config->video_bridge_path);
    printf("audio_ai_pipe = %s\n", config->audio_ai_pipe);
    printf("audio_ao_pipe = %s\n", config->audio_ao_pipe);
    printf("sip_listen_pipe = %s\n", config->sip_listen_pipe);
    printf("audio_bridge_pipe = %s\n", config->audio_bridge_pipe);
    printf("ding_message = %s\n", config->ding_message);
    printf("audio_buffer_size = %d\n", config->audio_buffer_size);
    printf("pipe_retry_interval_ms = %d\n", config->pipe_retry_interval_ms);
    printf("pipe_retry_max_attempts = %d\n", config->pipe_retry_max_attempts);
    printf("============================\n");
}
