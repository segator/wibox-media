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

    // Pipe Configuration
    char sip_listen_pipe[256];

    // Message Configuration
    char ding_message[32];

    // Intercom serial listener
    int serial_listener_enabled;
    char intercom_device[256];

    // MQTT/Home Assistant
    int mqtt_enabled;
    char mqtt_host[128];
    char mqtt_user[128];
    char mqtt_pass[128];
    char mqtt_homeassistant_prefix[128];
    char mqtt_base_topic[128];
    char mqtt_device_id[128];
    char mqtt_device_name[128];
    char mqtt_timezone[64];

    // Prometheus metrics exporter
    int prometheus_enabled;
    int prometheus_port;

    // Audio Configuration
    int audio_buffer_size;
    int audio_chip_gpio;
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
