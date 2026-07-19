#ifndef WIBOX_MQTT_H
#define WIBOX_MQTT_H

#include "call_session.h"
#include "config.h"
#include <stddef.h>

typedef struct {
    void (*open_door)(void* user_data);
    void (*trigger_f1)(void* user_data);
    void (*reboot_device)(void* user_data);
    void (*take_snapshot)(void* user_data);
    void (*simulate_ding)(void* user_data);
    void (*simulate_handset_answered)(void* user_data);
    void (*set_video_enabled)(int enabled, void* user_data);
    void (*set_video_bitrate)(int bitrate_kbps, void* user_data);
    void (*set_sip_outgoing_call_enabled)(int enabled, void* user_data);
    void (*set_hangup_on_door_unlock)(int enabled, void* user_data);
    void (*set_outgoing_call_target)(const char* target_uri, void* user_data);
    void (*set_outgoing_call_timeout)(int timeout_seconds, void* user_data);
    void (*set_ring_snapshot_delay)(int delay_ms, void* user_data);
    void (*set_call_forward_enabled)(int enabled, void* user_data);
    void (*set_rtsp_enabled)(int enabled, void* user_data);
} mqtt_callbacks_t;

int mqtt_init(const wibox_config_t* app_config, const char* local_ip,
              const mqtt_callbacks_t* callbacks, void* user_data);
int mqtt_start(void);
void mqtt_stop(void);
int mqtt_is_connected(void);

void mqtt_publish_online(void);
void mqtt_publish_offline(void);
void mqtt_publish_discovery(void);
void mqtt_publish_ringing(int active);
void mqtt_publish_call_active(int active);
void mqtt_publish_sip_call_active(int active);
void mqtt_publish_video_active(int active);
void mqtt_publish_video_enabled(int enabled);
void mqtt_publish_video_bitrate(int bitrate_kbps);
void mqtt_publish_sip_outgoing_call_enabled(int enabled);
void mqtt_publish_hangup_on_door_unlock(int enabled);
void mqtt_publish_outgoing_call_target(const char* target_uri);
void mqtt_publish_outgoing_call_timeout(int timeout_seconds);
void mqtt_publish_ring_snapshot_delay(int delay_ms);
void mqtt_publish_call_forward_enabled(int enabled);
void mqtt_publish_rtsp_enabled(int enabled);
void mqtt_publish_media_state(const char* state);
void mqtt_publish_call_id(const char* call_id);
void mqtt_publish_call_event(const call_session_event_t* event);
void mqtt_publish_firmware_version(void);
void mqtt_publish_door_unlocked_pulse(void);
void mqtt_publish_wifi_stats(void);
void mqtt_publish_snapshot_available(int available);
int mqtt_publish_snapshot_file(const char* path);
void mqtt_publish_uart_event(const char* event_type, const char* alias,
                             const unsigned char* raw, size_t raw_len,
                             int param, int known);
void mqtt_publish_uart_event_ex(const char* event_type, const char* alias,
                                const char* direction,
                                const unsigned char* raw, size_t raw_len,
                                int param, int known);

#endif
