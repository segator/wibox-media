#ifndef CONFIG_H
#define CONFIG_H

// Configuration structure
typedef struct {
    // Pipe Configuration
    char pipe_audio_out[256];
    char pipe_audio_in[256];

    // Debug Configuration
    int debug_print_audio_frames;

    // Hardware Configuration
    int audio_chip_gpio;

    // Timing Configuration
    int audio_thread_sleep_ms;
    int monitor_check_interval_ms;
} audio_bridge_config_t;

/**
 * Load configuration from file
 * @param config_file Path to configuration file
 * @param config Pointer to configuration structure to fill
 * @return 0 on success, -1 on error
 */
int config_load(const char* config_file, audio_bridge_config_t* config);

/**
 * Initialize configuration with default values
 * @param config Pointer to configuration structure to initialize
 */
void config_init_defaults(audio_bridge_config_t* config);

/**
 * Print current configuration (for debugging)
 * @param config Pointer to configuration structure
 */
void config_print(const audio_bridge_config_t* config);

#endif // CONFIG_H