#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>
#include <pthread.h>
#include <time.h>           // For timestamp in logging
#include <libgen.h>         // For dirname()

#include "adi_sys.h"
#include "adi_audio.h"
#include "ap.h"
#include "config.h"

#define THIS_FILE "audio_bridge"
#define CONFIG_FILE "/mnt/mtd/audio.conf"

// Thread configuration
#define AI_THREAD_PRIORITY     4    // Higher priority for audio input
#define AO_THREAD_PRIORITY     3    // Lower priority for audio output
#define MONITOR_THREAD_PRIORITY 2   // Lower priority for monitoring
#define AUDIO_THREAD_STACKSIZE   2048

typedef enum {
    AUDIO_STATE_STOPPED,
    AUDIO_STATE_RUNNING,
    AUDIO_STATE_STOPPING
} audio_state_t;

// Global state
static volatile int shutdown_requested = 0;
static pthread_mutex_t state_mutex = PTHREAD_MUTEX_INITIALIZER; // Mutex to protect shared state
static GADI_AUDIO_AioAttrT audio_config;
static GADI_U32 frame_size;
static int audio_chip_enabled = 0;

// Thread handles
static GADI_SYS_ThreadHandleT ai_thread_handle;
static GADI_SYS_ThreadHandleT ao_thread_handle;
static GADI_SYS_ThreadHandleT monitor_thread_handle;

// AEC handle
static int aec_handle = -1;

static audio_state_t audio_state = AUDIO_STATE_STOPPED;
static int audio_system_initialized = 0;

// Configuration
static audio_bridge_config_t app_config;

// Enhanced logging macro with both timestamp and location
#define DEBUG_LOG(fmt, ...) \
    do { \
        struct timespec ts; \
        clock_gettime(CLOCK_REALTIME, &ts); \
        struct tm *tm_info = localtime(&ts.tv_sec); \
        char time_buf[32]; \
        strftime(time_buf, sizeof(time_buf), "%H:%M:%S", tm_info); \
        printf("[%s.%03ld %s:%d] " fmt "\n", \
               time_buf, ts.tv_nsec / 1000000, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
        fflush(stdout); \
    } while(0)

// Thread-safe state access functions
static audio_state_t get_audio_state(void) {
    audio_state_t state;
    pthread_mutex_lock(&state_mutex);
    state = audio_state;
    pthread_mutex_unlock(&state_mutex);
    return state;
}

static void set_audio_state(audio_state_t new_state) {
    pthread_mutex_lock(&state_mutex);
    audio_state = new_state;
    pthread_mutex_unlock(&state_mutex);
}

static int get_audio_system_initialized(void) {
    int initialized;
    pthread_mutex_lock(&state_mutex);
    initialized = audio_system_initialized;
    pthread_mutex_unlock(&state_mutex);
    return initialized;
}

static void set_audio_system_initialized(int initialized) {
    pthread_mutex_lock(&state_mutex);
    audio_system_initialized = initialized;
    pthread_mutex_unlock(&state_mutex);
}

static int enable_audio_chip(void) {
    int fd;
    char gpio_path[64];

    if (audio_chip_enabled) {
        return 0;  // Already enabled
    }

    DEBUG_LOG("Enabling audio chip (GPIO%d -> 0)", app_config.audio_chip_gpio);

    snprintf(gpio_path, sizeof(gpio_path), "/sys/class/gpio/gpio%d/value", app_config.audio_chip_gpio);
    fd = open(gpio_path, O_WRONLY);
    if (fd < 0) {
        DEBUG_LOG("Failed to open GPIO%d: %s", app_config.audio_chip_gpio, strerror(errno));
        return -1;
    }

    if (write(fd, "0", 1) != 1) {
        DEBUG_LOG("Failed to enable audio chip: %s", strerror(errno));
        close(fd);
        return -1;
    }

    close(fd);
    audio_chip_enabled = 1;
    DEBUG_LOG("Audio chip enabled successfully");
    return 0;
}

static int disable_audio_chip(void) {
    int fd;
    char gpio_path[64];

    if (!audio_chip_enabled) {
        return 0;  // Already disabled
    }

    DEBUG_LOG("Disabling audio chip (GPIO%d -> 1)", app_config.audio_chip_gpio);

    snprintf(gpio_path, sizeof(gpio_path), "/sys/class/gpio/gpio%d/value", app_config.audio_chip_gpio);
    fd = open(gpio_path, O_WRONLY);
    if (fd < 0) {
        DEBUG_LOG("Failed to open GPIO%d: %s", app_config.audio_chip_gpio, strerror(errno));
        return -1;
    }

    if (write(fd, "1", 1) != 1) {
        DEBUG_LOG("Failed to disable audio chip: %s", strerror(errno));
        close(fd);
        return -1;
    }

    close(fd);
    audio_chip_enabled = 0;
    DEBUG_LOG("Audio chip disabled successfully");
    return 0;
}

static int has_audio_activity(GADI_U8 *data, GADI_U32 len) {
    int i;
    GADI_U8 byte_val;
    int activity_count = 0;

    // A-Law silence/noise values we want to ignore
    // 0x55, 0xd5 = common silence values
    // 0x5a, 0x5b = background noise

    for (i = 0; i < len; i++) {
        byte_val = data[i];

        // Check if byte is significantly different from silence/noise
        if (byte_val != 0x55 && byte_val != 0xd5 &&     // Not silence
            byte_val != 0x5a && byte_val != 0x5b &&     // Not background noise
            (byte_val < 0x50 || byte_val > 0x60)) {     // Outside quiet range

            activity_count++;
        }
    }

    // Consider it "active" if more than 10% of samples are above threshold
    return (activity_count > (len / 10));
}

static void print_audio_frame_hex(const char *prefix, GADI_U8 *data, GADI_U32 len) {
    int i;
    int print_len;
    char c;

    // Check if debug printing is enabled
    if (!app_config.debug_print_audio_frames) {
        return;  // Debug printing disabled
    }

    // Only print if frame has significant audio activity
    if (!has_audio_activity(data, len)) {
        return;  // Skip printing for silence/noise
    }

    // Print first 16 bytes in hex
    print_len = (len > 16) ? 16 : len;
    printf("%s: Frame %d bytes: ", prefix, len);
    for (i = 0; i < print_len; i++) {
        printf("%02x ", data[i]);
    }

    if (len > 16) {
        printf("... ");
    }

    // Also show as ASCII characters (for debugging)
    printf(" [");
    for (i = 0; i < print_len; i++) {
        c = data[i];
        printf("%c", (c >= 32 && c <= 126) ? c : '.');
    }
    printf("]\n");
    fflush(stdout);
}

// Signal handler for clean shutdown
static void shutdown_handler(int sig) {
    DEBUG_LOG("Received signal %d, initiating shutdown", sig);
    shutdown_requested = 1;
}

// Create directories recursively if they don't exist
static int create_directories(const char *path) {
    char *path_copy = strdup(path);
    char *dir_path = dirname(path_copy);

    // Check if directory already exists
    struct stat st;
    if (stat(dir_path, &st) == 0) {
        free(path_copy);
        return 0;  // Directory exists
    }

    // Create directory recursively
    if (mkdir(dir_path, 0755) < 0 && errno != EEXIST) {
        DEBUG_LOG("Failed to create directory %s: %s", dir_path, strerror(errno));
        free(path_copy);
        return -1;
    }

    DEBUG_LOG("Created directory: %s", dir_path);
    free(path_copy);
    return 0;
}

// Create named pipe with error handling
static int create_pipe(const char *pipe_path) {
    // Ensure directory exists
    if (create_directories(pipe_path) < 0) {
        return -1;
    }

    // Remove existing pipe file
    unlink(pipe_path);

    if (mkfifo(pipe_path, 0666) < 0) {
        DEBUG_LOG("Failed to create pipe %s: %s", pipe_path, strerror(errno));
        return -1;
    }

    DEBUG_LOG("Created pipe: %s", pipe_path);
    return 0;
}

// AI Thread: Microphone -> Pipe
static GADI_VOID ai_thread_func(GADI_VOID *arg) {
    GADI_AUDIO_AioFrameT ai_frame;
    GADI_AEC_AioFrameT aec_frame;
    int pipe_fd = -1;
    GADI_ERR ret;

    DEBUG_LOG("AI thread started");

    while (!shutdown_requested) {
        // Ensure pipe is open for writing
        if (pipe_fd < 0) {
            pipe_fd = open(app_config.pipe_audio_out, O_WRONLY | O_NONBLOCK);

            if (pipe_fd < 0) {
                if (errno != ENXIO) {
                    DEBUG_LOG("AI: Failed to open pipe: %s", strerror(errno));
                }
                sleep(1);
                continue;
            }
            DEBUG_LOG("AI: Client connected, streaming audio");
        }

        // Exit check after pipe operations
        if (shutdown_requested) {
            break;
        }

        // Get audio frame from microphone (blocking)
        ret = gadi_audio_ai_get_frame_aec(&ai_frame, &aec_frame, GADI_TRUE);
        if (ret != GADI_OK) {
            DEBUG_LOG("AI: Failed to get frame: error %d", ret);
            continue;
        }

        GADI_U8 *audio_data = ai_frame.virAddr;
        GADI_U32 audio_len = ai_frame.len;
        print_audio_frame_hex("AI_READ", audio_data, audio_len);

        // Write to pipe
        ssize_t written = write(pipe_fd, audio_data, audio_len);
        if (written < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Pipe is full - Client can't keep up
                DEBUG_LOG("AI: Pipe full, dropping frame");
                // Don't close pipe, just drop this frame
            } else if (errno == EPIPE) {
                DEBUG_LOG("AI: Client disconnected (EPIPE)");
                close(pipe_fd);
                pipe_fd = -1;
            } else {
                DEBUG_LOG("AI: Write error: %s", strerror(errno));
                close(pipe_fd);
                pipe_fd = -1;
            }
        } else if (written != audio_len) {
            DEBUG_LOG("AI: Partial write: %zd/%d bytes", written, audio_len);
        }
    }

    DEBUG_LOG("AI: Shutdown detected, cleaning up...");
    if (pipe_fd >= 0) {
        close(pipe_fd);
    }
    DEBUG_LOG("AI thread terminated");
}

// AO Thread
static GADI_VOID ao_thread_func(GADI_VOID *arg) {
    GADI_AUDIO_AioFrameT ao_frame;
    GADI_U8 *audio_buffer;
    int pipe_fd = -1;
    GADI_ERR ret;

    // State tracking to reduce log spam
    int waiting_logged = 0;        // Have we logged "waiting" message?
    int connection_logged = 0;     // Have we logged "disconnected" message?

    DEBUG_LOG("AO thread started");

    // Allocate audio buffer
    audio_buffer = (GADI_U8*)gadi_sys_malloc(frame_size);
    if (!audio_buffer) {
        DEBUG_LOG("AO: Failed to allocate audio buffer");
        return;
    }

    ao_frame.virAddr = audio_buffer;
    ao_frame.len = frame_size;

    while (!shutdown_requested) {
        // Only try pipe operations if not shutting down
        if (!shutdown_requested && pipe_fd < 0) {
            pipe_fd = open(app_config.pipe_audio_in, O_RDONLY | O_NONBLOCK);

            if (pipe_fd < 0) {
                DEBUG_LOG("AO: Pipe open failed: %s", strerror(errno));
                sleep(1);
                continue;
            }
            // Don't log successful open - it always succeeds for readers
        }

        // If we have an open pipe, try to read
        if (!shutdown_requested && pipe_fd >= 0) {
            ssize_t bytes_read = read(pipe_fd, audio_buffer, frame_size);

            if (bytes_read > 0) {
                // We have actual data - reset waiting flags
                if (waiting_logged) {
                    DEBUG_LOG("AO: Client connected, receiving data");
                    waiting_logged = 0;
                    connection_logged = 0;
                }

                if (bytes_read == frame_size) {
                    ret = gadi_audio_ao_send_frame_aec(&ao_frame, GADI_TRUE);
                    if (ret == GADI_OK) {
                        // DEBUG the audio data we just sent to the intercom
                        print_audio_frame_hex("AO_SEND", audio_buffer, frame_size);
                    }
                    else {
                        DEBUG_LOG("AO: Failed to send frame: error %d", ret);
                    }
                }
                else {
                    DEBUG_LOG("AO: Error. Partial frame received, discarding");
                }
            } else if (bytes_read == 0) {
                // EOF - no writer connected (this is the normal state)
                if (!connection_logged) {
                    DEBUG_LOG("AO: No client connected, waiting...");
                    connection_logged = 1;
                    waiting_logged = 1;
                }
            } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
                DEBUG_LOG("AO: Read error: %s", strerror(errno));
                close(pipe_fd);
                pipe_fd = -1;
                waiting_logged = 0;
                connection_logged = 0;
            }
            // For EAGAIN/EWOULDBLOCK, just continue

            if (bytes_read <= 0) {
                usleep(app_config.audio_thread_sleep_ms * 1000);  // Configurable sleep
            }
        }
    }

    DEBUG_LOG("AO: Shutdown detected, cleaning up...");
    if (pipe_fd >= 0) {
        close(pipe_fd);
    }
    gadi_sys_free(audio_buffer);
    DEBUG_LOG("AO thread terminated");
}

// Initialize audio system
static GADI_ERR init_audio_system(void) {
    GADI_ERR ret;
    GADI_AUDIO_SampleFormatEnumT sample_format = GADI_AUDIO_SAMPLE_FORMAT_A_LAW;

    DEBUG_LOG("Initializing audio system...");

    ret = gadi_audio_init();
    if (ret != GADI_OK) {
        DEBUG_LOG("Failed to init audio: %d", ret);
        return ret;
    }

    // Configure audio attributes
    audio_config.bitWidth = GADI_AUDIO_BIT_WIDTH_16;
    audio_config.soundMode = GADI_AUDIO_SOUND_MODE_SINGLE;
    audio_config.sampleRate = GADI_AUDIO_SAMPLE_RATE_8000;
    audio_config.frameSamples = 160;
    audio_config.frameNum = 30;

    // Calculate frame size for A-Law (1 byte per sample)
    frame_size = audio_config.frameSamples;
    DEBUG_LOG("Base frame size: %d bytes", frame_size);

    // Set sample format for AI and AO
    ret = gadi_audio_set_sample_format(gadi_audio_ai_get_fd(), sample_format);
    if (ret != GADI_OK) {
        DEBUG_LOG("Failed to set AI sample format: %d", ret);
        goto cleanup;
    }

    ret = gadi_audio_set_sample_format(gadi_audio_ao_get_fd(), sample_format);
    if (ret != GADI_OK) {
        DEBUG_LOG("Failed to set AO sample format: %d", ret);
        goto cleanup;
    }

    // Set AI/AO attributes
    ret = gadi_audio_ai_set_attr(&audio_config);
    if (ret != GADI_OK) {
        DEBUG_LOG("Failed to set AI attributes: %d", ret);
        goto cleanup;
    }

    ret = gadi_audio_ao_set_attr(&audio_config);
    if (ret != GADI_OK) {
        DEBUG_LOG("Failed to set AO attributes: %d", ret);
        goto cleanup;
    }

    // Enable AI and AO
    ret = gadi_audio_ai_enable();
    if (ret != GADI_OK) {
        DEBUG_LOG("Failed to enable AI: %d", ret);
        goto cleanup;
    }

    ret = gadi_audio_ao_enable();
    if (ret != GADI_OK) {
        DEBUG_LOG("Failed to enable AO: %d", ret);
        goto cleanup_ai;
    }

    // REGISTER AND ENABLE AEC to handle internal echo
    ret = ap_aec_register(&aec_handle);
    if (ret == GADI_OK) {
        DEBUG_LOG("AEC registered successfully, handle: %d", aec_handle);

        // NOW ENABLE AEC
        ret = gadi_audio_ai_aec_enable();
        if (ret == GADI_OK) {
            DEBUG_LOG("AEC enabled successfully - should cancel internal echo");
        } else {
            DEBUG_LOG("Failed to enable AEC: %d", ret);
        }
    } else {
        DEBUG_LOG("Failed to register AEC: %d", ret);
    }

    // Change volume if necessary
    // GADI_AUDIO_VolumeLevelEnumT max_volume = VLEVEL_12;  // Highest volume 0xb0 (176), default is 0xa3 (163)
    // ret = gadi_audio_ao_set_volume(&max_volume);
    // if (ret == GADI_OK) {
    //     DEBUG_LOG("Set AO volume to maximum: 0x%02x", max_volume);
    // } else {
    //     DEBUG_LOG("Failed to set AO volume: %d", ret);
    // }

    // ---- VERIFICATION

    GADI_AUDIO_AioAttrT aec_attr;
    ret = gadi_audio_ai_get_attr(&aec_attr);
    if (ret == GADI_OK) {
        DEBUG_LOG("AI attr after AEC: sampleRate=%d, bitWidth=%d, frameSamples=%d",
                  aec_attr.sampleRate, aec_attr.bitWidth, aec_attr.frameSamples);
    }

    // 1. Check AO file descriptor
    GADI_S32 ao_fd = gadi_audio_ao_get_fd();
    DEBUG_LOG("AO file descriptor: %d", ao_fd);

    // 2. Verify AO attributes were set correctly
    GADI_AUDIO_AioAttrT check_attr;
    ret = gadi_audio_ao_get_attr(&check_attr);
    if (ret == GADI_OK) {
        DEBUG_LOG("AO attr: bitWidth=%d, sampleRate=%d, frameSamples=%d, frameNum=%d",
                  check_attr.bitWidth, check_attr.sampleRate,
                  check_attr.frameSamples, check_attr.frameNum);
    } else {
        DEBUG_LOG("Failed to get AO attributes: %d", ret);
    }

    // 3. Check AO volume level
    GADI_AUDIO_VolumeLevelEnumT volume_level;
    ret = gadi_audio_ao_get_volume(&volume_level);
    if (ret == GADI_OK) {
        DEBUG_LOG("AO volume level: 0x%02x", volume_level);
    } else {
        DEBUG_LOG("Failed to get AO volume: %d", ret);
    }

    DEBUG_LOG("Audio system initialized successfully");
    return GADI_OK;

cleanup_ai:
    gadi_audio_ai_disable();
cleanup:
    gadi_audio_exit();
    return ret;
}

// Cleanup audio system
static void cleanup_audio_system(void) {
    DEBUG_LOG("Cleaning up audio system...");

    // Disable and unregister AEC if it was registered
    if (aec_handle >= 0) {
        gadi_audio_ai_aec_disable();
        ap_aec_unregister(aec_handle);
        aec_handle = -1;
        DEBUG_LOG("AEC disabled and unregistered");
    }

    gadi_audio_ao_disable();
    gadi_audio_ai_disable();
    gadi_audio_exit();
    disable_audio_chip();

    DEBUG_LOG("Audio system cleanup complete");
}

static GADI_ERR start_audio_system(void) {
    GADI_ERR ret;

    if (get_audio_system_initialized()) {
        DEBUG_LOG("Audio system already initialized");
        return GADI_OK;
    }

    DEBUG_LOG("Starting audio system...");

    // Enable audio chip first
    if (enable_audio_chip() != 0) {
        DEBUG_LOG("Failed to enable audio chip");
        return GADI_ERR_FROM_DRIVER;
    }

    // Initialize audio (existing init_audio_system code)
    ret = init_audio_system();
    if (ret != GADI_OK) {
        disable_audio_chip();
        return ret;
    }

    // Start threads
    ret = gadi_sys_thread_create(ai_thread_func, NULL, AI_THREAD_PRIORITY,
                                AUDIO_THREAD_STACKSIZE, "audio_ai", &ai_thread_handle);
    if (ret != GADI_OK) {
        cleanup_audio_system();
        disable_audio_chip();
        return ret;
    }

    ret = gadi_sys_thread_create(ao_thread_func, NULL, AO_THREAD_PRIORITY,
                                AUDIO_THREAD_STACKSIZE, "audio_ao", &ao_thread_handle);
    if (ret != GADI_OK) {
        gadi_sys_wait_end_thread(ai_thread_handle);
        cleanup_audio_system();
        disable_audio_chip();
        return ret;
    }

    set_audio_system_initialized(1);
    set_audio_state(AUDIO_STATE_RUNNING);
    DEBUG_LOG("Audio system started successfully");
    return GADI_OK;
}

static void stop_audio_system(void) {
    if (!get_audio_system_initialized()) {
        DEBUG_LOG("Audio system not initialized");
        return;
    }

    DEBUG_LOG("Stopping audio system...");
    set_audio_state(AUDIO_STATE_STOPPING);

    // Signal threads to stop
    shutdown_requested = 1;

    // Wait for threads to finish
    gadi_sys_wait_end_thread(ai_thread_handle);
    gadi_sys_wait_end_thread(ao_thread_handle);

    // Cleanup audio system
    cleanup_audio_system();

    // Reset state
    shutdown_requested = 0;
    set_audio_system_initialized(0);
    set_audio_state(AUDIO_STATE_STOPPED);

    DEBUG_LOG("Audio system stopped");
}

// Monitor thread: Detects client connections and manages audio system
static GADI_VOID monitor_thread_func(GADI_VOID *arg) {
    int pipe_fd = -1;
    int client_connected = 0;

    DEBUG_LOG("Monitor thread started - waiting for client connections");

    while (!shutdown_requested) {
        // Try to open pipe for writing (non-blocking)
        pipe_fd = open(app_config.pipe_audio_out, O_WRONLY | O_NONBLOCK);

        if (pipe_fd >= 0) {
            // Client is connected
            if (!client_connected) {
                DEBUG_LOG("Monitor: Client connected - starting audio system");
                if (start_audio_system() == GADI_OK) {
                    client_connected = 1;
                    DEBUG_LOG("Monitor: Audio system started successfully");
                } else {
                    DEBUG_LOG("Monitor: Failed to start audio system");
                }
            }
            close(pipe_fd);
            pipe_fd = -1;
        } else {
            // No client connected (ENXIO expected)
            if (client_connected && errno == ENXIO) {
                DEBUG_LOG("Monitor: Client disconnected - stopping audio system");
                stop_audio_system();
                client_connected = 0;
                DEBUG_LOG("Monitor: Audio system stopped");
            }
        }

        // Configurable check interval
        usleep(app_config.monitor_check_interval_ms * 1000);
    }

    DEBUG_LOG("Monitor thread terminated");
}

int main(int argc, char *argv[]) {
    GADI_ERR ret;

    printf("Audio Bridge v4.0 - Client-triggered mode with configuration\n");

    // Load configuration first
    if (config_load(CONFIG_FILE, &app_config) < 0) {
        printf("Warning: Configuration load failed, using defaults\n");
        config_init_defaults(&app_config);
    }

    // Print loaded configuration
    config_print(&app_config);

    // Setup signal handling
    signal(SIGPIPE, SIG_IGN);  // Ignore SIGPIPE, handle EPIPE instead
    signal(SIGINT, shutdown_handler);
    signal(SIGTERM, shutdown_handler);

    // Initialize system
    ret = gadi_sys_init();
    if (ret != GADI_OK) {
        printf("System init failed: %d\n", ret);
        return -1;
    }

    // Create pipes at startup using configured paths
    if (create_pipe(app_config.pipe_audio_out) < 0 || create_pipe(app_config.pipe_audio_in) < 0) {
        DEBUG_LOG("Failed to create pipes");
        gadi_sys_exit();
        return -1;
    }

    // Start monitoring thread
    ret = gadi_sys_thread_create(monitor_thread_func, NULL, MONITOR_THREAD_PRIORITY,
                                AUDIO_THREAD_STACKSIZE, "monitor", &monitor_thread_handle);
    if (ret != GADI_OK) {
        DEBUG_LOG("Failed to create monitor thread: %d", ret);
        unlink(app_config.pipe_audio_out);
        unlink(app_config.pipe_audio_in);
        gadi_sys_exit();
        return -1;
    }

    DEBUG_LOG("Audio bridge ready - waiting for client connections");
    DEBUG_LOG("Connect a client to read from %s to start audio", app_config.pipe_audio_out);

    // Wait for monitor thread to finish
    gadi_sys_wait_end_thread(monitor_thread_handle);

    // Cleanup on shutdown
    if (get_audio_system_initialized()) {
        stop_audio_system();
    }

    // Remove pipes using configured paths
    unlink(app_config.pipe_audio_out);
    unlink(app_config.pipe_audio_in);

    gadi_sys_exit();

    DEBUG_LOG("Audio bridge terminated");
    return 0;
}
