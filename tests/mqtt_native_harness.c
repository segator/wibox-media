#include "config.h"
#include "mqtt.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    int open_count;
    int f1_count;
    int snapshot_count;
    int video_value;
    int rtsp_value;
    int video_bitrate;
    int call_timeout;
    int ring_snapshot_delay;
    int call_forward_value;
    int reboot_count;
    int simulate_ding_count;
    int simulate_handset_count;
    int support_report_count;
    int sip_outgoing_enabled;
    char outgoing_target[256];
} harness_state_t;

static void on_open_door(void* user_data) {
    harness_state_t* state = (harness_state_t*)user_data;
    state->open_count++;
    printf("CALLBACK open_door=%d\n", state->open_count);
}

static void on_trigger_f1(void* user_data) {
    harness_state_t* state = (harness_state_t*)user_data;
    state->f1_count++;
    printf("CALLBACK trigger_f1=%d\n", state->f1_count);
}

static void on_take_snapshot(void* user_data) {
    harness_state_t* state = (harness_state_t*)user_data;
    state->snapshot_count++;
    printf("CALLBACK take_snapshot=%d\n", state->snapshot_count);
}

static void on_video_enabled(int enabled, void* user_data) {
    harness_state_t* state = (harness_state_t*)user_data;
    state->video_value = enabled;
    printf("CALLBACK video_enabled=%d\n", enabled);
}

static void on_rtsp_enabled(int enabled, void* user_data) {
    harness_state_t* state = (harness_state_t*)user_data;
    state->rtsp_value = enabled;
    mqtt_publish_rtsp_enabled(enabled);
    printf("CALLBACK rtsp_enabled=%d\n", enabled);
}

static void on_video_bitrate(int bitrate_kbps, void* user_data) {
    harness_state_t* state = (harness_state_t*)user_data;
    state->video_bitrate = bitrate_kbps;
    printf("CALLBACK video_bitrate=%d\n", bitrate_kbps);
}

static void on_call_timeout(int timeout_seconds, void* user_data) {
    harness_state_t* state = (harness_state_t*)user_data;
    state->call_timeout = timeout_seconds;
    printf("CALLBACK call_timeout=%d\n", timeout_seconds);
}

static void on_ring_snapshot_delay(int delay_ms, void* user_data) {
    harness_state_t* state = (harness_state_t*)user_data;
    state->ring_snapshot_delay = delay_ms;
    printf("CALLBACK ring_snapshot_delay=%d\n", delay_ms);
}

static void on_call_forward_enabled(int enabled, void* user_data) {
    harness_state_t* state = (harness_state_t*)user_data;
    state->call_forward_value = enabled;
    printf("CALLBACK call_forward_enabled=%d\n", enabled);
}

static void on_reboot(void* user_data) {
    harness_state_t* state = (harness_state_t*)user_data;
    state->reboot_count++;
    printf("CALLBACK reboot=%d\n", state->reboot_count);
}

static void on_simulate_ding(void* user_data) {
    harness_state_t* state = (harness_state_t*)user_data;
    state->simulate_ding_count++;
    printf("CALLBACK simulate_ding=%d\n", state->simulate_ding_count);
}

static void on_simulate_handset(void* user_data) {
    harness_state_t* state = (harness_state_t*)user_data;
    state->simulate_handset_count++;
    printf("CALLBACK simulate_handset=%d\n", state->simulate_handset_count);
}

static void on_support_report(void* user_data) {
    harness_state_t* state = (harness_state_t*)user_data;
    state->support_report_count++;
    mqtt_publish_support_report("WiBox test support report\nsecret token must not be included");
    printf("CALLBACK support_report=%d\n", state->support_report_count);
}

static void on_sip_outgoing_enabled(int enabled, void* user_data) {
    harness_state_t* state = (harness_state_t*)user_data;
    state->sip_outgoing_enabled = enabled;
    mqtt_publish_sip_outgoing_call_enabled(enabled);
    printf("CALLBACK sip_outgoing_enabled=%d\n", enabled);
}

static void on_outgoing_target(const char* target, void* user_data) {
    harness_state_t* state = (harness_state_t*)user_data;
    snprintf(state->outgoing_target, sizeof(state->outgoing_target), "%s",
             target ? target : "");
    mqtt_publish_outgoing_call_target(state->outgoing_target);
    printf("CALLBACK outgoing_target=%s\n", state->outgoing_target);
}

int main(void) {
    wibox_config_t config;
    mqtt_callbacks_t callbacks;
    harness_state_t state;
    call_session_event_t call_event;
    int i;

    memset(&callbacks, 0, sizeof(callbacks));
    memset(&state, 0, sizeof(state));
    state.video_value = -1;
    state.rtsp_value = -1;
    state.video_bitrate = -1;
    state.call_timeout = -1;
    state.ring_snapshot_delay = -1;
    state.call_forward_value = -1;
    state.sip_outgoing_enabled = -1;

    config_init_defaults(&config);
    if (config.audio_input_gain_percent != 35 ||
        config.audio_output_volume_percent != 50 ||
        config.audio_line_mute_ms != 900) {
        fprintf(stderr, "unexpected audio defaults: input=%d output=%d mute=%d\n",
                config.audio_input_gain_percent,
                config.audio_output_volume_percent,
                config.audio_line_mute_ms);
        return 4;
    }
    config.mqtt_enabled = 1;
    strcpy(config.mqtt_host, "127.0.0.1:18883");
    strcpy(config.mqtt_user, "wibox");
    strcpy(config.mqtt_pass, "test");
    strcpy(config.mqtt_base_topic, "wibox/test");
    strcpy(config.mqtt_device_id, "test");
    strcpy(config.mqtt_device_name, "WiBox Test");
    strcpy(config.mqtt_homeassistant_prefix, "homeassistant");
    config.firmware_update_enabled = 0;

    callbacks.open_door = on_open_door;
    callbacks.trigger_f1 = on_trigger_f1;
    callbacks.take_snapshot = on_take_snapshot;
    callbacks.reboot_device = on_reboot;
    callbacks.simulate_ding = on_simulate_ding;
    callbacks.simulate_handset_answered = on_simulate_handset;
    callbacks.create_support_report = on_support_report;
    callbacks.set_video_enabled = on_video_enabled;
    callbacks.set_rtsp_enabled = on_rtsp_enabled;
    callbacks.set_video_bitrate = on_video_bitrate;
    callbacks.set_sip_outgoing_call_enabled = on_sip_outgoing_enabled;
    callbacks.set_outgoing_call_target = on_outgoing_target;
    callbacks.set_outgoing_call_timeout = on_call_timeout;
    callbacks.set_ring_snapshot_delay = on_ring_snapshot_delay;
    callbacks.set_call_forward_enabled = on_call_forward_enabled;

    if (mqtt_init(&config, "127.0.0.1", &callbacks, &state) != 0) {
        return 2;
    }
    if (mqtt_start() != 0) {
        return 3;
    }

    for (i = 0; i < 80 && (state.open_count == 0 || state.f1_count == 0 ||
                           state.snapshot_count == 0 ||
                           state.video_value != 0 ||
                           state.rtsp_value != 1 ||
                           state.video_bitrate != 2048 ||
                           state.call_timeout != 45 ||
                           state.ring_snapshot_delay != 1500 ||
                           state.call_forward_value != 0 ||
                           state.reboot_count != 1 ||
                           state.simulate_ding_count != 1 ||
                           state.simulate_handset_count != 1 ||
                           state.support_report_count != 1 ||
                           state.sip_outgoing_enabled != 0 ||
                           strcmp(state.outgoing_target, "sip:3000@example.test") != 0); i++) {
        usleep(100000);
    }

    mqtt_publish_ringing(1);
    mqtt_publish_call_active(1);
    mqtt_publish_sip_call_active(1);
    mqtt_publish_video_active(1);
    mqtt_publish_snapshot_available(1);
    mqtt_publish_media_state("ringing");
    mqtt_publish_door_unlocked_pulse();
    mqtt_publish_call_id("1a2b3c4d-00000001");

    /* The broker deliberately drops the first connection after seeing the
     * active call ID. A command delivered on the second connection proves
     * that reconnect completed before the active state is cleared. */
    for (i = 0; i < 100 && state.video_bitrate != 3072; i++) {
        usleep(100000);
    }

    memset(&call_event, 0, sizeof(call_event));
    strcpy(call_event.call_id, "1a2b3c4d-00000001");
    strcpy(call_event.event_type, "established");
    strcpy(call_event.source, "physical_panel");
    strcpy(call_event.route, "sip");
    strcpy(call_event.media_state, "established");
    strcpy(call_event.reason, "mqtt-e2e");
    call_event.sequence = 3;
    call_event.started_at = 1000;
    call_event.timestamp = 1002;
    mqtt_publish_call_event(&call_event);
    mqtt_publish_call_event(NULL);
    {
        static const unsigned char uart_frame[] = {0xfb, 0x10, 0x01, 0x1c};
        static const unsigned char jpeg[] = {0xff, 0xd8, 1, 2, 3, 4, 0xff, 0xd9};
        FILE* snapshot = fopen("/tmp/wibox-mqtt-test.jpg", "wb");
        if (!snapshot || fwrite(jpeg, 1, sizeof(jpeg), snapshot) != sizeof(jpeg)) {
            if (snapshot) fclose(snapshot);
            return 5;
        }
        fclose(snapshot);
        mqtt_publish_uart_event("alarm_report", "ALARM_REPORT", uart_frame,
                                sizeof(uart_frame), 1, 1);
        mqtt_publish_uart_event_ex("start_call", "START_CALL", "tx", uart_frame,
                                   sizeof(uart_frame), 1, 1);
        mqtt_publish_uart_event(NULL, NULL, NULL, 0, -1, 0);
        if (mqtt_publish_snapshot_file("/tmp/wibox-mqtt-test.jpg") != 0 ||
            mqtt_publish_snapshot_file("/tmp/does-not-exist.jpg") == 0) {
            unlink("/tmp/wibox-mqtt-test.jpg");
            return 6;
        }
        unlink("/tmp/wibox-mqtt-test.jpg");
    }
    mqtt_publish_call_id(NULL);
    mqtt_publish_ringing(0);
    mqtt_publish_call_active(0);
    mqtt_publish_sip_call_active(0);
    mqtt_publish_video_active(0);
    mqtt_publish_media_state("idle");
    mqtt_stop();
    printf("RESULT open=%d f1=%d snapshot=%d video=%d rtsp=%d bitrate=%d timeout=%d ring_snapshot_delay=%d call_forward=%d reboot=%d ding=%d handset=%d sip_outgoing=%d target=%s\n",
           state.open_count, state.f1_count, state.snapshot_count,
           state.video_value, state.rtsp_value, state.video_bitrate, state.call_timeout,
           state.ring_snapshot_delay, state.call_forward_value, state.reboot_count,
           state.simulate_ding_count, state.simulate_handset_count,
           state.sip_outgoing_enabled, state.outgoing_target);
    return (state.open_count == 1 && state.f1_count == 1 && state.snapshot_count == 1 &&
            state.video_value == 0 && state.rtsp_value == 1 && state.video_bitrate == 3072 &&
            state.call_timeout == 45 && state.ring_snapshot_delay == 1500 &&
            state.call_forward_value == 0 && state.reboot_count == 1 &&
            state.simulate_ding_count == 1 && state.simulate_handset_count == 1 &&
            state.sip_outgoing_enabled == 0 &&
            strcmp(state.outgoing_target, "sip:3000@example.test") == 0) ? 0 : 1;
}
