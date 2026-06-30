#ifndef WIBOX_MQTT_H
#define WIBOX_MQTT_H

#include "config.h"

typedef struct {
    void (*open_door)(void* user_data);
    void (*set_video_enabled)(int enabled, void* user_data);
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
void mqtt_publish_media_state(const char* state);
void mqtt_publish_firmware_version(void);
void mqtt_publish_last_ring(void);
void mqtt_publish_last_unlock(void);
void mqtt_publish_wifi_stats(void);

#endif
