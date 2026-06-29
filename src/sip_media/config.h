#ifndef CONFIG_H
#define CONFIG_H

// Configuration structure
typedef struct {
    // SIP Configuration
    char outgoing_call_target[256];
    int outgoing_call_timeout;
    int sip_port;
    int rtp_port;
    int video_enabled;
    int video_rtp_port;
    int video_payload_type;
    char video_bridge_path[256];

    // Pipe Configuration
    char audio_ai_pipe[256];
    char audio_ao_pipe[256];
    char sip_listen_pipe[256];
    char audio_bridge_pipe[256];

    // Message Configuration
    char ding_message[32];

    // Intercom serial listener
    int serial_listener_enabled;
    char intercom_device[256];

    // Audio Configuration
    int audio_buffer_size;
    int pipe_retry_interval_ms;
    int pipe_retry_max_attempts;
} wibox_config_t;

/**
 * Load configuration from file
 * @param config_file Path to configuration file
 * @param config Pointer to configuration structure to fill
 * @return 0 on success, -1 on error
 */
int config_load(const char* config_file, wibox_config_t* config);

/**
 * Initialize configuration with default values
 * @param config Pointer to configuration structure to initialize
 */
void config_init_defaults(wibox_config_t* config);

/**
 * Print current configuration (for debugging)
 * @param config Pointer to configuration structure
 */
void config_print(const wibox_config_t* config);

#endif // CONFIG_H
