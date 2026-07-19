#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (0)

#define CHECK_INT(config, field, expected) \
    CHECK((config).field == (expected))
#define CHECK_STR(config, field, expected) \
    CHECK(strcmp((config).field, (expected)) == 0)

static int write_config(char *path, const char *contents)
{
    int fd = mkstemp(path);
    FILE *fp;

    if (fd < 0) {
        return -1;
    }
    fp = fdopen(fd, "w");
    if (!fp) {
        close(fd);
        return -1;
    }
    if (fputs(contents, fp) == EOF || fclose(fp) != 0) {
        return -1;
    }
    return 0;
}

static int test_defaults_and_missing_file(void)
{
    char missing[128];
    wibox_config_t config;

    config_init_defaults(NULL);
    config_print(NULL);
    CHECK(config_load(NULL, &config) == -1);
    CHECK(config_load("/tmp/unused", NULL) == -1);

    snprintf(missing, sizeof(missing), "/tmp/wibox-missing-config-%ld", (long)getpid());
    unlink(missing);
    CHECK(config_load(missing, &config) == 0);
    CHECK_INT(config, sip_outgoing_call_enabled, 1);
    CHECK_INT(config, hangup_on_door_unlock, 1);
    CHECK_INT(config, outgoing_call_timeout, 60);
    CHECK_INT(config, video_enabled, 0);
    CHECK_INT(config, video_bitrate_kbps, 4096);
    CHECK_INT(config, ring_snapshot_delay_ms, 2000);
    CHECK_INT(config, rtsp_enabled, 0);
    CHECK_INT(config, mqtt_enabled, 1);
    CHECK_INT(config, firmware_update_enabled, 1);
    CHECK_INT(config, prometheus_enabled, 1);
    CHECK_INT(config, hardware_watchdog_enabled, 1);
    CHECK_STR(config, hardware_watchdog_device, "/dev/watchdog");
    CHECK_INT(config, audio_input_gain_percent, 35);
    CHECK_INT(config, audio_output_volume_percent, 50);
    CHECK_INT(config, audio_line_mute_ms, 900);
    return 0;
}

static int test_all_supported_keys(void)
{
    char path[] = "/tmp/wibox-config-complete-XXXXXX";
    wibox_config_t config;
    const char *contents =
        "  # whitespace and comments are accepted\n"
        "\n"
        " sip_outgoing_call_enabled = 0 \n"
        "hangup_on_door_unlock=0\n"
        " outgoing_call_target = \"sip:2000@example.test:5070\" \n"
        "outgoing_call_timeout=45\n"
        "sip_port=5090\n"
        "rtp_port=9000\n"
        "video_enabled=1\n"
        "video_rtp_port=9002\n"
        "video_payload_type=97\n"
        "video_bitrate_kbps=3072\n"
        "video_gop_n=30\n"
        "video_idr_interval=2\n"
        "video_brc_mode=1\n"
        "video_rtsp_periodic_idr_ms=1000\n"
        "ring_snapshot_delay_ms=1250\n"
        "video_recording_enabled=1\n"
        "video_recording_path=/tmp/test-recording.h264\n"
        "video_recording_max_seconds=12\n"
        "rtsp_enabled=1\n"
        "rtsp_port=9554\n"
        "rtsp_auth_user='viewer'\n"
        "rtsp_auth_pass=secret\n"
        "video_bridge_path=/legacy/video\n"
        "audio_ai_pipe=/legacy/ai\n"
        "audio_ao_pipe=/legacy/ao\n"
        "sip_listen_pipe=/tmp/test-sip-pipe\n"
        "audio_bridge_pipe=/legacy/audio\n"
        "ding_message=TEST_DING\n"
        "serial_listener_enabled=0\n"
        "intercom_device=/tmp/fake-uart\n"
        "intercom_reopen_guard_ms=2500\n"
        "mqtt_enabled=0\n"
        "mqtt_host=broker.test:1884\n"
        "mqtt_user=test-user\n"
        "mqtt_pass=test-pass\n"
        "mqtt_homeassistant_prefix=ha-test\n"
        "mqtt_base_topic=wibox/test-device\n"
        "mqtt_device_id=test-device\n"
        "mqtt_device_name=Test Device\n"
        "firmware_update_enabled=0\n"
        "firmware_update_repo=aymerici/wibox-media\n"
        "prometheus_enabled=0\n"
        "prometheus_port=19617\n"
        "hardware_watchdog_enabled=0\n"
        "hardware_watchdog_device=/tmp/fake-watchdog\n"
        "hardware_watchdog_timeout_seconds=60\n"
        "hardware_watchdog_feed_interval_seconds=10\n"
        "mqtt_pub_path=/legacy/pub\n"
        "mqtt_sub_path=/legacy/sub\n"
        "audio_buffer_size=320\n"
        "audio_chip_gpio=23\n"
        "audio_input_gain_percent=40\n"
        "audio_output_volume_percent=55\n"
        "audio_line_mute_ms=1200\n"
        "pipe_retry_interval_ms=10\n"
        "pipe_retry_max_attempts=2\n"
        "unknown_future_key=ignored\n";

    CHECK(write_config(path, contents) == 0);
    CHECK(config_load(path, &config) == 0);
    unlink(path);

    CHECK_INT(config, sip_outgoing_call_enabled, 0);
    CHECK_INT(config, hangup_on_door_unlock, 0);
    CHECK_STR(config, outgoing_call_target, "sip:2000@example.test:5070");
    CHECK_INT(config, outgoing_call_timeout, 45);
    CHECK_INT(config, sip_port, 5090);
    CHECK_INT(config, rtp_port, 9000);
    CHECK_INT(config, video_enabled, 1);
    CHECK_INT(config, video_rtp_port, 9002);
    CHECK_INT(config, video_payload_type, 97);
    CHECK_INT(config, video_bitrate_kbps, 3072);
    CHECK_INT(config, video_gop_n, 30);
    CHECK_INT(config, video_idr_interval, 2);
    CHECK_INT(config, video_brc_mode, 1);
    CHECK_INT(config, video_rtsp_periodic_idr_ms, 1000);
    CHECK_INT(config, ring_snapshot_delay_ms, 1250);
    CHECK_INT(config, video_recording_enabled, 1);
    CHECK_STR(config, video_recording_path, "/tmp/test-recording.h264");
    CHECK_INT(config, video_recording_max_seconds, 12);
    CHECK_INT(config, rtsp_enabled, 1);
    CHECK_INT(config, rtsp_port, 9554);
    CHECK_STR(config, rtsp_auth_user, "viewer");
    CHECK_STR(config, rtsp_auth_pass, "secret");
    CHECK_STR(config, sip_listen_pipe, "/tmp/test-sip-pipe");
    CHECK_STR(config, ding_message, "TEST_DING");
    CHECK_INT(config, serial_listener_enabled, 0);
    CHECK_STR(config, intercom_device, "/tmp/fake-uart");
    CHECK_INT(config, intercom_reopen_guard_ms, 2500);
    CHECK_INT(config, mqtt_enabled, 0);
    CHECK_STR(config, mqtt_host, "broker.test:1884");
    CHECK_STR(config, mqtt_user, "test-user");
    CHECK_STR(config, mqtt_pass, "test-pass");
    CHECK_STR(config, mqtt_homeassistant_prefix, "ha-test");
    CHECK_STR(config, mqtt_base_topic, "wibox/test-device");
    CHECK_STR(config, mqtt_device_id, "test-device");
    CHECK_STR(config, mqtt_device_name, "Test Device");
    CHECK_INT(config, firmware_update_enabled, 0);
    CHECK_STR(config, firmware_update_repo, "segator/wibox-media");
    CHECK_INT(config, prometheus_enabled, 0);
    CHECK_INT(config, prometheus_port, 19617);
    CHECK_INT(config, hardware_watchdog_enabled, 0);
    CHECK_STR(config, hardware_watchdog_device, "/tmp/fake-watchdog");
    CHECK_INT(config, hardware_watchdog_timeout_seconds, 60);
    CHECK_INT(config, hardware_watchdog_feed_interval_seconds, 10);
    CHECK_INT(config, audio_buffer_size, 320);
    CHECK_INT(config, audio_chip_gpio, 23);
    CHECK_INT(config, audio_input_gain_percent, 40);
    CHECK_INT(config, audio_output_volume_percent, 55);
    CHECK_INT(config, audio_line_mute_ms, 1200);
    config_print(&config);
    return 0;
}

static int test_parse_errors_and_repo_passthrough(void)
{
    char invalid_path[] = "/tmp/wibox-config-invalid-XXXXXX";
    char repo_path[] = "/tmp/wibox-config-repo-XXXXXX";
    wibox_config_t config;

    CHECK(write_config(invalid_path, "sip_port=5070\ninvalid line\n") == 0);
    CHECK(config_load(invalid_path, &config) == -1);
    CHECK_INT(config, sip_port, 5070);
    unlink(invalid_path);

    CHECK(write_config(repo_path, "firmware_update_repo=example/custom\n") == 0);
    CHECK(config_load(repo_path, &config) == 0);
    CHECK_STR(config, firmware_update_repo, "example/custom");
    unlink(repo_path);
    return 0;
}

int main(void)
{
    if (test_defaults_and_missing_file() != 0 ||
        test_all_supported_keys() != 0 ||
        test_parse_errors_and_repo_passthrough() != 0) {
        return 1;
    }
    printf("RESULT config_coverage PASS\n");
    return 0;
}
