#include "config.h"
#include "mqtt.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    int open_count;
    int video_value;
} harness_state_t;

static void on_open_door(void* user_data) {
    harness_state_t* state = (harness_state_t*)user_data;
    state->open_count++;
    printf("CALLBACK open_door=%d\n", state->open_count);
}

static void on_video_enabled(int enabled, void* user_data) {
    harness_state_t* state = (harness_state_t*)user_data;
    state->video_value = enabled;
    printf("CALLBACK video_enabled=%d\n", enabled);
}

int main(void) {
    wibox_config_t config;
    mqtt_callbacks_t callbacks;
    harness_state_t state;
    int i;

    memset(&callbacks, 0, sizeof(callbacks));
    memset(&state, 0, sizeof(state));
    state.video_value = -1;

    config_init_defaults(&config);
    config.mqtt_enabled = 1;
    strcpy(config.mqtt_host, "127.0.0.1:18883");
    strcpy(config.mqtt_user, "wibox");
    strcpy(config.mqtt_pass, "test");
    strcpy(config.mqtt_base_topic, "wibox/test");
    strcpy(config.mqtt_device_id, "test");
    strcpy(config.mqtt_device_name, "WiBox Test");
    strcpy(config.mqtt_homeassistant_prefix, "homeassistant");
    config.video_enabled = 1;

    callbacks.open_door = on_open_door;
    callbacks.set_video_enabled = on_video_enabled;

    if (mqtt_init(&config, "127.0.0.1", &callbacks, &state) != 0) {
        return 2;
    }
    if (mqtt_start() != 0) {
        return 3;
    }

    for (i = 0; i < 80 && (state.open_count == 0 || state.video_value != 0); i++) {
        usleep(100000);
    }

    mqtt_publish_last_unlock();
    mqtt_stop();
    printf("RESULT open=%d video=%d\n", state.open_count, state.video_value);
    return (state.open_count == 1 && state.video_value == 0) ? 0 : 1;
}
