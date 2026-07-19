#ifndef CONFIG_H
#define CONFIG_H

// Configuration structure
typedef struct {
    // SIP Configuration
    int sip_outgoing_call_enabled;
    int hangup_on_door_unlock;
    char outgoing_call_target[256];
    int outgoing_call_timeout;
    int sip_port;
    int rtp_port;
    int video_enabled;
    int video_rtp_port;
    int video_payload_type;
    int video_bitrate_kbps;
    int video_gop_n;
    int video_idr_interval;
    int video_brc_mode;
    int video_rtsp_periodic_idr_ms;
    int ring_snapshot_delay_ms;
    int video_recording_enabled;
    char video_recording_path[256];
    int video_recording_max_seconds;
    int rtsp_enabled;
    int rtsp_port;
    char rtsp_auth_user[64];
    char rtsp_auth_pass[64];

    // Pipe Configuration
    char sip_listen_pipe[256];

    // Message Configuration
    char ding_message[32];

    // Intercom serial listener
    int serial_listener_enabled;
    char intercom_device[256];
    int intercom_reopen_guard_ms;

    // MQTT/Home Assistant
    int mqtt_enabled;
    char mqtt_host[128];
    char mqtt_user[128];
    char mqtt_pass[128];
    char mqtt_homeassistant_prefix[128];
    char mqtt_base_topic[128];
    char mqtt_device_id[128];
    char mqtt_device_name[128];
    int firmware_update_enabled;
    char firmware_update_repo[128];

    // Prometheus metrics exporter
    int prometheus_enabled;
    int prometheus_port;

    // Hardware watchdog
    int hardware_watchdog_enabled;
    char hardware_watchdog_device[256];
    int hardware_watchdog_timeout_seconds;
    int hardware_watchdog_feed_interval_seconds;

    // Audio Configuration
    int audio_buffer_size;
    int audio_chip_gpio;
    int audio_input_gain_percent;
    int audio_output_volume_percent;
    int audio_line_mute_ms;
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
