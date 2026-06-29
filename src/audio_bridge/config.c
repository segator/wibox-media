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

void config_init_defaults(audio_bridge_config_t* config) {
    if (!config) return;

    // Pipe Configuration
    strcpy(config->pipe_audio_out, "/tmp/audio_from_intercom");
    strcpy(config->pipe_audio_in, "/tmp/audio_to_intercom");

    // Debug Configuration
    config->debug_print_audio_frames = 1;  // Enable by default

    // Hardware Configuration
    config->audio_chip_gpio = 18;  // GPIO18

    // Timing Configuration
    config->audio_thread_sleep_ms = 1;    // 1ms sleep in AO thread
    config->monitor_check_interval_ms = 500;  // 500ms monitor check
}

static int parse_config_line(const char* line, audio_bridge_config_t* config) {
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
    if (strcmp(key, "pipe_audio_out") == 0) {
        strncpy(config->pipe_audio_out, value, sizeof(config->pipe_audio_out) - 1);
        config->pipe_audio_out[sizeof(config->pipe_audio_out) - 1] = '\0';
    } else if (strcmp(key, "pipe_audio_in") == 0) {
        strncpy(config->pipe_audio_in, value, sizeof(config->pipe_audio_in) - 1);
        config->pipe_audio_in[sizeof(config->pipe_audio_in) - 1] = '\0';
    } else if (strcmp(key, "debug_print_audio_frames") == 0) {
        config->debug_print_audio_frames = atoi(value);
    } else if (strcmp(key, "audio_chip_gpio") == 0) {
        config->audio_chip_gpio = atoi(value);
    } else if (strcmp(key, "audio_thread_sleep_ms") == 0) {
        config->audio_thread_sleep_ms = atoi(value);
    } else if (strcmp(key, "monitor_check_interval_ms") == 0) {
        config->monitor_check_interval_ms = atoi(value);
    } else {
        printf("Unknown configuration key: %s\n", key);
        return -1;
    }

    return 0;
}

int config_load(const char* config_file, audio_bridge_config_t* config) {
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

void config_print(const audio_bridge_config_t* config) {
    if (!config) return;

    printf("=== Audio Bridge Configuration ===\n");
    printf("pipe_audio_out = %s\n", config->pipe_audio_out);
    printf("pipe_audio_in = %s\n", config->pipe_audio_in);
    printf("debug_print_audio_frames = %d\n", config->debug_print_audio_frames);
    printf("audio_chip_gpio = %d\n", config->audio_chip_gpio);
    printf("audio_thread_sleep_ms = %d\n", config->audio_thread_sleep_ms);
    printf("monitor_check_interval_ms = %d\n", config->monitor_check_interval_ms);
    printf("=================================\n");
}