#include "fakes/fake_pjsip.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/reboot.h>

static int test_system(const char *command);
static int test_reboot(int command);
static void test_sync(void);

#define WIFI_AP_REQUEST_PATH "/tmp/wibox-orchestration-ap-request"
#define system test_system
#define reboot test_reboot
#define sync test_sync
#define main sip_media_daemon_main
#include "../src/sip_media/sip_media.c"
#undef main
#undef sync
#undef reboot
#undef system

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #condition); \
        exit(1); \
    } \
} while (0)

static pj_pool_t media_pool;
static sip_call_state_t fake_sip_state;
static sip_call_session_t fake_sip_session;
static pj_status_t fake_make_call_status;
static pj_status_t fake_set_target_status;
static pj_status_t fake_invite_status;
static int fake_make_call_count;
static int fake_terminate_count;
static int fake_invite_count;
static int fake_ack_count;
static int fake_bye_count;
static int fake_cancel_count;
static int intercom_counts[INTERCOM_CMD_F1_OFF + 1];
static int intercom_fail_command;
static int mqtt_ring_state;
static int mqtt_call_state;
static int mqtt_sip_state;
static int mqtt_video_state;
static int mqtt_snapshot_available;
static int mqtt_unlock_count;
static int mqtt_uart_count;
static int mqtt_call_event_count;
static int mqtt_media_state_count;
static char mqtt_last_media_state[32];
static int prometheus_ring_count;
static int prometheus_unlock_count;
static int prometheus_call_count;
static int prometheus_uart_unknown_count;
static int prometheus_uart_frame_count;
static int audio_start_count;
static int audio_stop_count;
static int rtsp_audio_clients;
static int rtsp_video_clients;
static int rtsp_start_count;
static int rtsp_stop_count;
static int reboot_count;
static int sync_count;
static int system_count;

static void reset_records(void) {
    if (call_session_ready) {
        call_session_destroy(&current_call_session);
        call_session_ready = 0;
    }
    memset(&app_config, 0, sizeof(app_config));
    app_config.sip_outgoing_call_enabled = 0;
    snprintf(app_config.outgoing_call_target,
             sizeof(app_config.outgoing_call_target),
             "sip:portal@example.test");
    app_config.outgoing_call_timeout = 10;
    app_config.sip_port = 5060;
    app_config.rtp_port = 8000;
    app_config.video_enabled = 0;
    app_config.video_rtp_port = 8002;
    app_config.video_payload_type = 96;
    app_config.video_bitrate_kbps = 2048;
    app_config.video_gop_n = 30;
    app_config.video_idr_interval = 2;
    app_config.video_brc_mode = 1;
    app_config.video_rtsp_periodic_idr_ms = 1000;
    app_config.ring_snapshot_delay_ms = 0;
    app_config.rtsp_port = 8554;
    app_config.audio_buffer_size = 160;
    app_config.audio_chip_gpio = 18;
    app_config.audio_input_gain_percent = 35;
    app_config.audio_output_volume_percent = 50;
    app_config.audio_line_mute_ms = 0;
    snprintf(app_config.ding_message, sizeof(app_config.ding_message), "DING");
    snprintf(app_config.sip_listen_pipe, sizeof(app_config.sip_listen_pipe),
             "/tmp/wibox-orchestration-test.pipe");
    snprintf(local_ip_addr, sizeof(local_ip_addr), "192.0.2.10");

    if (!call_active_mutex) {
        CHECK(pj_mutex_create_simple(&media_pool, "test-call", &call_active_mutex) == PJ_SUCCESS);
    }
    pool = &media_pool;
    set_call_active_status(PJ_FALSE);
    set_audio_engine_running(0);
    clear_audio_sip_target();
    audio_input_thread = NULL;
    audio_output_thread = NULL;
    audio_input_mute_until_ms = 0;
    video_bridge_pid = -1;
    video_bridge_control_fd = -1;
    video_bridge_has_sip_rtp = 0;
    snapshot_active = 0;
    simulated_ding_panel_context_active = 0;
    intercom_last_close_ms = 0;
    snprintf(intercom_last_close_reason, sizeof(intercom_last_close_reason), "none");
    reboot_requested = 0;
    ringing_timeout_generation = 0;
    quit_flag = PJ_FALSE;
    last_dtmf_event = 255;
    last_dtmf_timestamp = 0;
    last_dtmf_time = 0;
    current_dtmf_payload_type = RTP_PAYLOAD_DTMF;
    CHECK(call_session_init(&current_call_session, 0x12345678U) == 0);
    call_session_ready = 1;

    memset(&fake_sip_session, 0, sizeof(fake_sip_session));
    fake_sip_session.state = SIP_CALL_STATE_IDLE;
    fake_sip_session.direction = SIP_CALL_DIRECTION_NONE;
    fake_sip_session.remote_dtmf_payload_type = RTP_PAYLOAD_DTMF;
    fake_sip_session.remote_video_payload_type = 96;
    fake_sip_state = SIP_CALL_STATE_IDLE;
    fake_make_call_status = PJ_SUCCESS;
    fake_set_target_status = PJ_SUCCESS;
    fake_invite_status = PJ_SUCCESS;
    fake_make_call_count = 0;
    fake_terminate_count = 0;
    fake_invite_count = 0;
    fake_ack_count = 0;
    fake_bye_count = 0;
    fake_cancel_count = 0;
    memset(intercom_counts, 0, sizeof(intercom_counts));
    intercom_fail_command = -1;
    mqtt_ring_state = -1;
    mqtt_call_state = -1;
    mqtt_sip_state = -1;
    mqtt_video_state = -1;
    mqtt_snapshot_available = -1;
    mqtt_unlock_count = 0;
    mqtt_uart_count = 0;
    mqtt_call_event_count = 0;
    mqtt_media_state_count = 0;
    mqtt_last_media_state[0] = '\0';
    prometheus_ring_count = 0;
    prometheus_unlock_count = 0;
    prometheus_call_count = 0;
    prometheus_uart_unknown_count = 0;
    prometheus_uart_frame_count = 0;
    audio_start_count = 0;
    audio_stop_count = 0;
    rtsp_audio_clients = 0;
    rtsp_video_clients = 0;
    rtsp_start_count = 0;
    rtsp_stop_count = 0;
    reboot_count = 0;
    sync_count = 0;
    system_count = 0;
    fake_pjsip_reset();
}

static void set_message_body(pjsip_msg *message, pjsip_msg_body *body,
                             const char *text) {
    memset(message, 0, sizeof(*message));
    memset(body, 0, sizeof(*body));
    if (text) {
        body->data = (void *)text;
        body->len = (unsigned int)strlen(text);
        message->body = body;
    }
}

static void test_dtmf_paths(void) {
    pjsip_msg message;
    pjsip_msg_body body;
    pjsip_rx_data request;
    unsigned char packet[32];
    int event;

    reset_records();
    app_config.hangup_on_door_unlock = 1;
    fake_sip_state = SIP_CALL_STATE_ESTABLISHED;
    for (event = DTMF_EVENT_0; event <= DTMF_EVENT_STAR; event++) {
        handle_dtmf_event((unsigned char)event, 3, 10);
    }
    handle_dtmf_event(99, 0, 0);
    CHECK(mqtt_unlock_count == 0);
    handle_dtmf_event(DTMF_EVENT_HASH, 2, 20);
    CHECK(intercom_counts[INTERCOM_CMD_UNLOCK_DOOR] == 1);
    CHECK(mqtt_unlock_count == 1 && prometheus_unlock_count == 1);
    CHECK(fake_terminate_count == 0);
    CHECK(fake_sip_state == SIP_CALL_STATE_ESTABLISHED);

    memset(&request, 0, sizeof(request));
    request.msg_info.msg = &message;
    set_message_body(&message, &body, NULL);
    CHECK(handle_sip_info_dtmf(&request));
    set_message_body(&message, &body, "Signal=5\r\nDuration=100");
    CHECK(handle_sip_info_dtmf(&request));
    set_message_body(&message, &body, "digit=#");
    CHECK(handle_sip_info_dtmf(&request));
    CHECK(mqtt_unlock_count == 2);
    set_message_body(&message, &body, "no tone here");
    CHECK(handle_sip_info_dtmf(&request));
    CHECK(fake_stateless_response_count == 4);

    memset(packet, 0, sizeof(packet));
    CHECK(!parse_rtp_dtmf_event(packet, 15));
    packet[0] = 0x80;
    packet[1] = 8;
    CHECK(!parse_rtp_dtmf_event(packet, 16));
    packet[1] = RTP_PAYLOAD_DTMF;
    packet[4] = 0x12;
    packet[7] = 0x34;
    packet[12] = DTMF_EVENT_HASH;
    packet[13] = 0x85;
    packet[14] = 0;
    packet[15] = 40;
    CHECK(parse_rtp_dtmf_event(packet, 16));
    CHECK(mqtt_unlock_count == 3);
    CHECK(parse_rtp_dtmf_event(packet, 16));
    CHECK(mqtt_unlock_count == 3);
    packet[0] = 0x90;
    CHECK(parse_rtp_dtmf_event(packet, 16));
    packet[0] = 0x8f;
    CHECK(parse_rtp_dtmf_event(packet, 16));
}

static void test_intercom_and_call_transitions(void) {
    reset_records();
    CHECK(ensure_intercom_call_open("test") == 1);
    CHECK(get_call_active_status());
    CHECK(ensure_intercom_call_open("duplicate") == 0);
    CHECK(intercom_counts[INTERCOM_CMD_START_CALL] == 1);
    CHECK(close_intercom_call("test-close") == 0);
    CHECK(!get_call_active_status());
    CHECK(intercom_counts[INTERCOM_CMD_STOP_CALL] == 1);

    intercom_fail_command = INTERCOM_CMD_START_CALL;
    CHECK(ensure_intercom_call_open("failure") == -1);
    CHECK(!get_call_active_status());
    intercom_fail_command = INTERCOM_CMD_STOP_CALL;
    set_call_active_status(PJ_TRUE);
    CHECK(close_intercom_call("stop-failure") == -1);
    CHECK(!get_call_active_status());

    intercom_fail_command = -1;
    on_call_state_change(SIP_CALL_STATE_RINGING, SIP_CALL_STATE_ESTABLISHED, NULL);
    CHECK(get_call_active_status());
    CHECK(intercom_counts[INTERCOM_CMD_START_CALL] == 3);
    CHECK(mqtt_sip_state == 1);
    CHECK(strcmp(mqtt_last_media_state, "established") == 0);
    CHECK(prometheus_call_count == 1);
    on_call_state_change(SIP_CALL_STATE_ESTABLISHED, SIP_CALL_STATE_IDLE, NULL);
    CHECK(!get_call_active_status());
    CHECK(mqtt_sip_state == 0);
    CHECK(strcmp(mqtt_last_media_state, "idle") == 0);

    simulated_ding_panel_context_active = 1;
    set_call_active_status(PJ_TRUE);
    on_call_state_change(SIP_CALL_STATE_CALLING, SIP_CALL_STATE_IDLE, NULL);
    CHECK(!simulated_ding_panel_context_active);
    CHECK(!get_call_active_status());
    on_call_state_change(SIP_CALL_STATE_RINGING, SIP_CALL_STATE_FAILED, NULL);
    CHECK(mqtt_sip_state == 0);
}

static void test_audio_ownership(void) {
    reset_records();
    fake_sip_session.state = SIP_CALL_STATE_ESTABLISHED;
    fake_sip_session.remote_dtmf_payload_type = 110;
    app_config.hangup_on_door_unlock = 1;
    fake_sip_state = SIP_CALL_STATE_ESTABLISHED;
    on_audio_ready("198.51.100.9", 9000, 0, NULL);
    CHECK(get_audio_sip_rtp_active());
    CHECK(get_audio_engine_running());
    CHECK(audio_start_count == 1);
    CHECK(current_dtmf_payload_type == 110);
    on_audio_ready("198.51.100.10", 9002, 0, NULL);
    CHECK(audio_start_count == 1);
    stop_audio_session();
    CHECK(!get_audio_sip_rtp_active());
    CHECK(!get_audio_engine_running());
    CHECK(audio_stop_count == 1);

    start_audio_session(NULL, 9000);
    start_audio_session("bad-ip", 9000);
    start_audio_session("198.51.100.9", 0);
    CHECK(audio_start_count == 1);
}

static void test_uart_and_control_paths(void) {
    static const unsigned char unknown[4] = {0xfb, 0xff, 0, 0};
    static const unsigned char start_call[4] = {0xfb, 0x14, 0x01, 0x20};
    static const unsigned char hangup0[4] = {0xfb, 0x13, 0x00, 0x1e};
    static const unsigned char handset[4] = {0xfb, 0x23, 0x00, 0x2e};
    static const unsigned char push0[4] = {0xfb, 0x19, 0x00, 0x24};
    static const unsigned char push1[4] = {0xfb, 0x19, 0x01, 0x25};
    static const unsigned char sta_to_ap[4] = {0xfb, 0x21, 0x00, 0x2c};
    static const unsigned char long_press_1[4] = {0xfb, 0x24, 0x01, 0x30};
    static const unsigned char long_press_2[4] = {0xfb, 0x24, 0x02, 0x31};
    FILE *ap_marker;

    reset_records();
    handle_uart_frame(unknown);
    CHECK(prometheus_uart_unknown_count == 1 && mqtt_uart_count == 1);
    handle_uart_frame(start_call);
    CHECK(get_call_active_status());
    handle_uart_frame(push0);
    handle_uart_frame(push1);
    fake_sip_state = SIP_CALL_STATE_RINGING;
    handle_uart_frame(hangup0);
    CHECK(!get_call_active_status());
    CHECK(fake_terminate_count == 1);
    fake_sip_state = SIP_CALL_STATE_RINGING;
    handle_uart_frame(handset);
    CHECK(fake_terminate_count == 2);
    CHECK(prometheus_uart_frame_count >= 5);

    unlink(WIFI_AP_REQUEST_PATH);
    handle_uart_frame(sta_to_ap);
    ap_marker = fopen(WIFI_AP_REQUEST_PATH, "r");
    CHECK(ap_marker != NULL);
    fclose(ap_marker);
    CHECK(reboot_count == 1 && sync_count == 1);
    unlink(WIFI_AP_REQUEST_PATH);

    /* The observed GK7102S button emits a two-frame long-press sequence. A
     * stage-2 frame alone must never force provisioning. */
    handle_uart_frame(long_press_2);
    CHECK(access(WIFI_AP_REQUEST_PATH, F_OK) != 0);
    CHECK(reboot_count == 1);
    handle_uart_frame(long_press_1);
    handle_uart_frame(long_press_2);
    ap_marker = fopen(WIFI_AP_REQUEST_PATH, "r");
    CHECK(ap_marker != NULL);
    fclose(ap_marker);
    CHECK(reboot_count == 2 && sync_count == 2);
    unlink(WIFI_AP_REQUEST_PATH);

    handle_control_message("UART FB 19 01 25");
    handle_control_message("not-a-command");

    app_config.sip_outgoing_call_enabled = 0;
    handle_control_message("DING");
    CHECK(simulated_ding_panel_context_active);
    CHECK(mqtt_ring_state == 1);
    mqtt_simulate_handset_answered_callback(NULL);
    CHECK(!simulated_ding_panel_context_active);
    CHECK(!get_call_active_status());
}

static void test_mqtt_callbacks_and_runtime_config(void) {
    video_encoder_tuning_t tuning;

    reset_records();
    mqtt_set_video_enabled_callback(1, NULL);
    CHECK(app_config.video_enabled == 1);
    mqtt_set_video_enabled_callback(0, NULL);
    CHECK(app_config.video_enabled == 0);
    mqtt_set_video_bitrate_callback(1, NULL);
    CHECK(app_config.video_bitrate_kbps >= 128);
    mqtt_set_video_bitrate_callback(20000, NULL);
    CHECK(app_config.video_bitrate_kbps <= 8192);
    mqtt_set_sip_outgoing_call_enabled_callback(1, NULL);
    CHECK(app_config.sip_outgoing_call_enabled == 1);
    mqtt_set_sip_outgoing_call_enabled_callback(0, NULL);
    CHECK(app_config.sip_outgoing_call_enabled == 0);

    mqtt_set_outgoing_call_target_callback("sip:portal@example.test", NULL);
    CHECK(strcmp(app_config.outgoing_call_target,
                 "sip:portal@example.test") == 0);
    fake_set_target_status = PJ_EBUSY;
    mqtt_set_outgoing_call_target_callback("sip:busy@example.test", NULL);
    CHECK(strstr(app_config.outgoing_call_target, "portal@example.test") != NULL);
    mqtt_set_outgoing_call_target_callback("invalid target", NULL);

    mqtt_set_outgoing_call_timeout_callback(1, NULL);
    CHECK(app_config.outgoing_call_timeout == 10);
    mqtt_set_ring_snapshot_delay_callback(-1, NULL);
    CHECK(app_config.ring_snapshot_delay_ms == 0);
    mqtt_set_call_forward_enabled_callback(1, NULL);
    mqtt_set_call_forward_enabled_callback(0, NULL);
    CHECK(intercom_counts[INTERCOM_CMD_ENABLE_PUSH_STATE] == 1);
    CHECK(intercom_counts[INTERCOM_CMD_DISABLE_PUSH_STATE] == 1);

    mqtt_set_rtsp_enabled_callback(1, NULL);
    mqtt_set_rtsp_enabled_callback(0, NULL);
    CHECK(rtsp_start_count == 1 && rtsp_stop_count == 1);
    populate_video_encoder_tuning(&tuning);
    CHECK(tuning.gop_n >= 1 && tuning.idr_interval >= 1);
    publish_snapshot_button_availability();
    CHECK(mqtt_snapshot_available == 0);

    app_config.hangup_on_door_unlock = 1;
    fake_sip_state = SIP_CALL_STATE_ESTABLISHED;
    mqtt_open_door_callback(NULL);
    CHECK(mqtt_unlock_count == 1);
    CHECK(fake_terminate_count == 1);
    CHECK(fake_sip_state == SIP_CALL_STATE_IDLE);
    app_config.hangup_on_door_unlock = 0;
    fake_sip_state = SIP_CALL_STATE_ESTABLISHED;
    mqtt_open_door_callback(NULL);
    CHECK(mqtt_unlock_count == 2);
    CHECK(fake_sip_state == SIP_CALL_STATE_ESTABLISHED);
    mqtt_reboot_device_callback(NULL);
    mqtt_reboot_device_callback(NULL);
    CHECK(reboot_count == 1 && sync_count == 1);
}

static void set_request_method(pjsip_rx_data *request, pjsip_msg *message,
                               int id, char *name) {
    memset(request, 0, sizeof(*request));
    memset(message, 0, sizeof(*message));
    request->msg_info.msg = message;
    message->line.req.method.id = id;
    message->line.req.method.name = pj_str(name);
}

static void test_sip_dispatch(void) {
    pjsip_rx_data request;
    pjsip_msg message;
    pjsip_msg_body body;

    reset_records();
    set_request_method(&request, &message, 0, "OPTIONS");
    CHECK(on_rx_request(&request));
    CHECK(fake_last_stateless_status == 200);
    set_request_method(&request, &message, FAKE_METHOD_INVITE, "INVITE");
    CHECK(on_rx_request(&request));
    CHECK(fake_invite_count == 1);
    fake_invite_status = -1;
    CHECK(on_rx_request(&request));
    CHECK(fake_last_stateless_status == 500);
    fake_invite_status = PJ_EBUSY;
    CHECK(on_rx_request(&request));
    set_request_method(&request, &message, FAKE_METHOD_BYE, "BYE");
    CHECK(on_rx_request(&request));
    set_request_method(&request, &message, FAKE_METHOD_ACK, "ACK");
    CHECK(on_rx_request(&request));
    set_request_method(&request, &message, FAKE_METHOD_CANCEL, "CANCEL");
    CHECK(on_rx_request(&request));
    set_request_method(&request, &message, 0, "INFO");
    set_message_body(&message, &body, "Signal=#");
    CHECK(on_rx_request(&request));
    set_request_method(&request, &message, 0, "MESSAGE");
    CHECK(on_rx_request(&request));
    CHECK(fake_last_stateless_status == 405);
    CHECK(on_rx_response(&request));
}

int main(void) {
    test_dtmf_paths();
    test_intercom_and_call_transitions();
    test_audio_ownership();
    test_uart_and_control_paths();
    test_mqtt_callbacks_and_runtime_config();
    test_sip_dispatch();
    if (call_session_ready) {
        call_session_destroy(&current_call_session);
        call_session_ready = 0;
    }
    puts("RESULT sip_media_orchestration PASS");
    return 0;
}

static int test_system(const char *command) {
    (void)command;
    system_count++;
    return 0;
}

static int test_reboot(int command) {
    (void)command;
    reboot_count++;
    return 0;
}

static void test_sync(void) { sync_count++; }

pj_status_t sip_calling_init(const sip_call_config_t *call_config,
                             pjsip_endpoint *endpoint_arg,
                             pj_pool_t *pool_arg) {
    (void)call_config; (void)endpoint_arg; (void)pool_arg; return PJ_SUCCESS;
}
void sip_calling_set_callbacks(sip_call_state_callback_t state_cb,
                               sip_call_audio_callback_t audio_cb,
                               void *user_data) {
    (void)state_cb; (void)audio_cb; (void)user_data;
}
void sip_calling_set_call_timeout(int timeout_seconds) { (void)timeout_seconds; }
pj_status_t sip_calling_set_target_uri(const char *target_uri) {
    (void)target_uri; return fake_set_target_status;
}
void sip_calling_set_video_config(int port, int payload) { (void)port; (void)payload; }
pj_status_t sip_calling_make_call(void) {
    fake_make_call_count++;
    if (fake_make_call_status == PJ_SUCCESS) fake_sip_state = SIP_CALL_STATE_CALLING;
    return fake_make_call_status;
}
pj_status_t sip_calling_terminate_call(void) {
    fake_terminate_count++; fake_sip_state = SIP_CALL_STATE_IDLE; return PJ_SUCCESS;
}
sip_call_state_t sip_calling_get_state(void) { return fake_sip_state; }
pj_bool_t sip_calling_is_call_active(void) { return fake_sip_state != SIP_CALL_STATE_IDLE; }
const sip_call_session_t *sip_calling_get_session(void) {
    return fake_sip_state == SIP_CALL_STATE_IDLE ? NULL : &fake_sip_session;
}
pj_bool_t sip_calling_check_timeout(void) { return PJ_FALSE; }
pj_bool_t sip_calling_handle_response(pjsip_rx_data *data) { (void)data; return PJ_TRUE; }
pj_status_t sip_calling_handle_incoming_invite(pjsip_rx_data *data) {
    (void)data; fake_invite_count++; return fake_invite_status;
}
pj_status_t sip_calling_handle_incoming_ack(pjsip_rx_data *data) {
    (void)data; fake_ack_count++; return PJ_SUCCESS;
}
pj_status_t sip_calling_handle_incoming_bye(pjsip_rx_data *data) {
    (void)data; fake_bye_count++; return PJ_SUCCESS;
}
pj_status_t sip_calling_handle_incoming_cancel(pjsip_rx_data *data) {
    (void)data; fake_cancel_count++; return PJ_SUCCESS;
}
pj_status_t sip_calling_create_sdp_offer(pj_pool_t *pool_arg, const char *ip,
                                         int audio_port, int video_port,
                                         int payload, pj_str_t *sdp) {
    (void)pool_arg; (void)ip; (void)audio_port; (void)video_port; (void)payload; (void)sdp;
    return PJ_SUCCESS;
}
pj_status_t sip_calling_parse_sdp_answer(const char *sdp, int *audio_port,
                                         int *dtmf, int *video_port,
                                         int *video_payload) {
    (void)sdp; (void)audio_port; (void)dtmf; (void)video_port; (void)video_payload;
    return PJ_SUCCESS;
}
void sip_calling_cleanup(void) {}

int intercom_init(void) { return 0; }
int intercom_send_command(intercom_cmd_t command) {
    intercom_counts[command]++;
    return (int)command == intercom_fail_command ? -1 : 0;
}
void intercom_cleanup(void) {}

int mqtt_init(const wibox_config_t *configuration, const char *ip,
              const mqtt_callbacks_t *callbacks, void *data) {
    (void)configuration; (void)ip; (void)callbacks; (void)data; return 0;
}
int mqtt_start(void) { return 0; }
void mqtt_stop(void) {}
int mqtt_is_connected(void) { return 1; }
void mqtt_publish_online(void) {}
void mqtt_publish_offline(void) {}
void mqtt_publish_discovery(void) {}
void mqtt_publish_ringing(int active) { mqtt_ring_state = active; }
void mqtt_publish_call_active(int active) { mqtt_call_state = active; }
void mqtt_publish_sip_call_active(int active) { mqtt_sip_state = active; }
void mqtt_publish_video_active(int active) { mqtt_video_state = active; }
void mqtt_publish_video_enabled(int enabled) { (void)enabled; }
void mqtt_publish_video_bitrate(int bitrate) { (void)bitrate; }
void mqtt_publish_sip_outgoing_call_enabled(int enabled) { (void)enabled; }
void mqtt_publish_hangup_on_door_unlock(int enabled) { (void)enabled; }
void mqtt_publish_outgoing_call_target(const char *target) { (void)target; }
void mqtt_publish_outgoing_call_timeout(int timeout) { (void)timeout; }
void mqtt_publish_ring_snapshot_delay(int delay) { (void)delay; }
void mqtt_publish_call_forward_enabled(int enabled) { (void)enabled; }
void mqtt_publish_rtsp_enabled(int enabled) { (void)enabled; }
void mqtt_publish_media_state(const char *state) {
    mqtt_media_state_count++;
    snprintf(mqtt_last_media_state, sizeof(mqtt_last_media_state), "%s", state ? state : "");
}
void mqtt_publish_call_id(const char *id) { (void)id; }
void mqtt_publish_call_event(const call_session_event_t *event) {
    if (event) mqtt_call_event_count++;
}
void mqtt_publish_firmware_version(void) {}
void mqtt_publish_door_unlocked_pulse(void) { mqtt_unlock_count++; }
void mqtt_publish_wifi_stats(void) {}
void mqtt_publish_snapshot_available(int available) { mqtt_snapshot_available = available; }
int mqtt_publish_snapshot_file(const char *path) { (void)path; return 0; }
int mqtt_publish_support_report(const char *body) { (void)body; return 0; }
void mqtt_publish_uart_event(const char *event_type, const char *alias,
                             const unsigned char *raw, size_t raw_len,
                             int param, int known) {
    (void)event_type; (void)alias; (void)raw; (void)raw_len; (void)param; (void)known;
    mqtt_uart_count++;
}
void mqtt_publish_uart_event_ex(const char *event_type, const char *alias,
                                const char *direction,
                                const unsigned char *raw, size_t raw_len,
                                int param, int known) {
    (void)event_type; (void)alias; (void)direction; (void)raw; (void)raw_len;
    (void)param; (void)known; mqtt_uart_count++;
}

int prometheus_start(int port) { (void)port; return 0; }
void prometheus_stop(void) {}
void prometheus_set_call_active(int active) { (void)active; }
void prometheus_set_sip_call_active(int active) { (void)active; }
void prometheus_set_video_active(int active) { (void)active; }
void prometheus_set_video_enabled(int enabled) { (void)enabled; }
void prometheus_set_ringing(int active) { (void)active; }
void prometheus_inc_ring(void) { prometheus_ring_count++; }
void prometheus_inc_door_unlock(void) { prometheus_unlock_count++; }
void prometheus_inc_call_started(void) { prometheus_call_count++; }
void prometheus_inc_video_started(void) {}
void prometheus_inc_uart_frame(void) { prometheus_uart_frame_count++; }
void prometheus_inc_uart_unknown_frame(void) { prometheus_uart_unknown_count++; }
void prometheus_inc_uart_alarm_report(void) {}
void prometheus_inc_uart_hangup(void) {}
void prometheus_inc_uart_stop_ring(void) {}
void prometheus_inc_uart_reset(void) {}
void prometheus_inc_uart_push_state(void) {}
void prometheus_inc_uart_f1(void) {}

int audio_hw_start(int gpio, int samples, int gain, int volume) {
    (void)gpio; (void)samples; (void)gain; (void)volume; audio_start_count++; return 0;
}
void audio_hw_stop(void) { audio_stop_count++; }
int audio_hw_get_frame(unsigned char *buffer, size_t size) {
    memset(buffer, 0xd5, size); return (int)size;
}
int audio_hw_send_frame(const unsigned char *buffer, size_t size) {
    (void)buffer; return (int)size;
}
int audio_hw_frame_size(void) { return 160; }

int video_worker_run(const char *ip, int remote_port, int local_port,
                     int payload, int bitrate,
                     const video_encoder_tuning_t *tuning,
                     const char *dump, long long limit,
                     int rtsp_fd, int control_fd) {
    (void)ip; (void)remote_port; (void)local_port; (void)payload; (void)bitrate;
    (void)tuning; (void)dump; (void)limit; (void)rtsp_fd; (void)control_fd; return 0;
}
int video_snapshot_capture(const char *path, int quality) {
    (void)path; (void)quality; return 0;
}

int rtsp_stream_start(int port, const char *ip, int video,
                      const char *user, const char *password) {
    (void)port; (void)ip; (void)video; (void)user; (void)password;
    rtsp_start_count++; return 0;
}
void rtsp_stream_stop(void) { rtsp_stop_count++; }
void rtsp_stream_set_video_enabled(int enabled) { (void)enabled; }
int rtsp_stream_get_video_pipe_fd(void) { return -1; }
int rtsp_stream_get_video_client_count(void) { return rtsp_video_clients; }
int rtsp_stream_get_audio_client_count(void) { return rtsp_audio_clients; }
void rtsp_stream_set_client_callback(rtsp_stream_client_callback_t callback,
                                     void *data) { (void)callback; (void)data; }
void rtsp_stream_send_audio_rtp(const unsigned char *packet, size_t size) {
    (void)packet; (void)size;
}

int hardware_watchdog_start(int enabled, const char *device,
                            int timeout, int interval) {
    (void)enabled; (void)device; (void)timeout; (void)interval; return 0;
}
void hardware_watchdog_heartbeat(void) {}
void hardware_watchdog_stop(int disarm) { (void)disarm; }
