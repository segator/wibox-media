#!/usr/bin/env python3
import json
import os
import socket
import struct
import subprocess
import sys
import threading
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HARNESS = Path(os.environ.get("WIBOX_MQTT_HARNESS", "/tmp/wibox_mqtt_native_harness"))
PORT = 18883
ACTIVE_CALL_ID = "1a2b3c4d-00000001"


def enc_remaining(length):
    out = bytearray()
    while True:
        byte = length % 128
        length //= 128
        if length:
            byte |= 0x80
        out.append(byte)
        if not length:
            return bytes(out)


def read_packet(conn):
    first = conn.recv(1)
    if not first:
        return None, b""
    multiplier = 1
    length = 0
    while True:
        byte = conn.recv(1)[0]
        length += (byte & 127) * multiplier
        if not byte & 128:
            break
        multiplier *= 128
    payload = b""
    while len(payload) < length:
        chunk = conn.recv(length - len(payload))
        if not chunk:
            break
        payload += chunk
    return first[0], payload


def packet(packet_type, payload=b""):
    return bytes([packet_type]) + enc_remaining(len(payload)) + payload


def publish(topic, payload, retain=False):
    topic_b = topic.encode()
    payload_b = payload.encode()
    body = struct.pack("!H", len(topic_b)) + topic_b + payload_b
    return packet(0x31 if retain else 0x30, body)


def send_initial_commands(conn):
    conn.sendall(publish("wibox/test/f1/trigger/set", "PRESS", retain=True))
    conn.sendall(publish("wibox/test/snapshot/take/set", "PRESS", retain=True))
    conn.sendall(publish("wibox/test/door/open/set", "PRESS", retain=True))
    conn.sendall(publish("wibox/test/firmware/update/check/set", "PRESS", retain=True))
    conn.sendall(publish("wibox/test/firmware/update/install/set", "PRESS", retain=True))
    conn.sendall(publish("wibox/test/system/reboot/set", "PRESS", retain=True))
    conn.sendall(publish("wibox/test/developer/simulate_ding/set", "PRESS", retain=True))
    conn.sendall(publish("wibox/test/developer/simulate_handset_answered/set", "PRESS", retain=True))
    conn.sendall(publish("wibox/test/support/report/set", "PRESS", retain=True))
    conn.sendall(publish("wibox/test/video/enabled/set", "OFF", retain=True))
    conn.sendall(publish("wibox/test/rtsp/enabled/set", "ON", retain=True))
    conn.sendall(publish("wibox/test/video/bitrate_kbps/set", "2048", retain=True))
    conn.sendall(publish("wibox/test/call/timeout_seconds/set", "45", retain=True))
    conn.sendall(publish("wibox/test/snapshot/ring_delay_ms/set", "1500", retain=True))
    conn.sendall(publish("wibox/test/call_forward/enabled/set", "OFF", retain=True))
    conn.sendall(publish("wibox/test/f1/trigger/set", "PRESS"))
    conn.sendall(publish("wibox/test/snapshot/take/set", "PRESS"))
    conn.sendall(publish("wibox/test/door/open/set", "PRESS"))
    conn.sendall(publish("wibox/test/developer/simulate_ding/set", "PRESS"))
    conn.sendall(publish("wibox/test/developer/mode/set", "ON"))
    conn.sendall(publish("wibox/test/developer/simulate_ding/set", "PRESS"))
    conn.sendall(publish("wibox/test/developer/simulate_handset_answered/set", "PRESS"))
    conn.sendall(publish("wibox/test/support/report/set", "PRESS"))
    conn.sendall(publish("wibox/test/developer/mode/set", "OFF"))
    conn.sendall(publish("wibox/test/developer/simulate_ding/set", "PRESS"))
    conn.sendall(publish("wibox/test/system/reboot/set", "PRESS"))
    conn.sendall(publish("wibox/test/system/reboot/set", "PRESS"))
    conn.sendall(publish("wibox/test/call/outgoing_enabled/set", "OFF"))
    conn.sendall(publish("wibox/test/call/target_uri/set", "sip:3000@example.test"))
    conn.sendall(publish("wibox/test/video/bitrate_kbps/set", "not-a-number"))
    conn.sendall(publish("wibox/test", "CONFIG"))


def handle_connection(conn, session_index, published, retained_topics,
                      session_publishes, broker_errors):
    conn.settimeout(10)
    typ, _ = read_packet(conn)
    if typ != 0x10:
        broker_errors.append(f"session {session_index}: missing CONNECT")
        return
    conn.sendall(packet(0x20, b"\x00\x00"))

    typ, payload = read_packet(conn)
    if typ != 0x82:
        broker_errors.append(f"session {session_index}: missing SUBSCRIBE")
        return
    packet_id = payload[:2]
    conn.sendall(packet(0x90, packet_id + b"\x00\x00"))

    deadline = time.time() + 10
    sent_commands = False
    while time.time() < deadline:
        try:
            typ, payload = read_packet(conn)
        except socket.timeout:
            continue
        if typ is None:
            return
        if (typ & 0xF0) == 0x30 and len(payload) >= 2:
            topic_len = struct.unpack("!H", payload[:2])[0]
            topic = payload[2 : 2 + topic_len].decode(errors="replace")
            body = payload[2 + topic_len :].decode(errors="replace")
            published.append((topic, body))
            session_publishes.append((session_index, topic, body))
            if typ & 0x01:
                retained_topics.append(topic)
            if not sent_commands and topic == "wibox/test":
                sent_commands = True
                if session_index == 0:
                    send_initial_commands(conn)
                else:
                    conn.sendall(publish("wibox/test/video/bitrate_kbps/set", "3072"))
            if session_index == 0 and topic == "wibox/test/call/id" and body == ACTIVE_CALL_ID:
                return
        elif typ == 0xC0:
            conn.sendall(packet(0xD0))


def broker(published, retained_topics, session_publishes, broker_errors):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server:
        server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server.bind(("127.0.0.1", PORT))
        server.listen(2)
        server.settimeout(15)
        for session_index in range(2):
            try:
                conn, _ = server.accept()
            except (OSError, socket.timeout) as exc:
                broker_errors.append(f"session {session_index}: accept failed: {exc}")
                return
            with conn:
                handle_connection(conn, session_index, published, retained_topics,
                                  session_publishes, broker_errors)


def main():
    build = [
        "gcc",
        "-Wall",
        "-Wextra",
        "-std=gnu99",
        "-pthread",
        '-DWIBOX_VERSION="test-version"',
        '-DWIBOX_COMMIT="test-commit"',
        '-DWIBOX_BUILD_TIMESTAMP="2026-06-30T09:30:00Z"',
        "-Isrc/sip_media",
        "tests/mqtt_native_harness.c",
        "src/sip_media/mqtt.c",
        "src/sip_media/config.c",
        "-o",
        str(HARNESS),
    ]
    if os.environ.get("WIBOX_COVERAGE") == "1":
        build[1:1] = ["-O0", "--coverage"]
    subprocess.run(build, cwd=ROOT, check=True)

    published = []
    retained_topics = []
    session_publishes = []
    broker_errors = []
    thread = threading.Thread(target=broker,
                              args=(published, retained_topics, session_publishes,
                                    broker_errors), daemon=True)
    thread.start()
    time.sleep(0.1)

    proc = subprocess.run([str(HARNESS)], cwd=ROOT, text=True, capture_output=True)
    thread.join(timeout=5)
    sys.stdout.write(proc.stdout)
    sys.stderr.write(proc.stderr)

    if proc.returncode != 0:
        return proc.returncode
    if thread.is_alive():
        print("mock broker did not finish", file=sys.stderr)
        return 1
    if broker_errors:
        print("; ".join(broker_errors), file=sys.stderr)
        return 1
    if (1, "wibox/test/call/id", ACTIVE_CALL_ID) not in session_publishes:
        print("active call ID was not restored after MQTT reconnect", file=sys.stderr)
        return 1
    if (1, "wibox/test/media/state", "ringing") not in session_publishes:
        print("active media state was not restored after MQTT reconnect", file=sys.stderr)
        return 1
    if not any(topic.startswith("homeassistant/") for topic, _ in published):
        print("missing Home Assistant discovery publish", file=sys.stderr)
        return 1
    if ("wibox/test", "online") not in published:
        print("missing retained online publish", file=sys.stderr)
        return 1
    if ("wibox/test/firmware/version", "test-version") not in published:
        print("missing retained firmware version publish", file=sys.stderr)
        return 1
    if ("wibox/test/firmware/commit", "test-commit") not in published:
        print("missing retained firmware commit publish", file=sys.stderr)
        return 1
    if ("wibox/test/firmware/build_timestamp", "2026-06-30T09:30:00Z") not in published:
        print("missing retained firmware build timestamp publish", file=sys.stderr)
        return 1
    if not any(topic.endswith("_call_id/config") and "call/id" in payload
               for topic, payload in published):
        print("missing call ID Home Assistant discovery publish", file=sys.stderr)
        return 1
    if not any(topic.endswith("_call_event/config") and "call/event" in payload and
               "physical_handset_answered" in payload
               for topic, payload in published):
        print("missing call event Home Assistant discovery publish", file=sys.stderr)
        return 1
    if ("wibox/test/call/id", "none") not in published or \
       ("wibox/test/call/id", "1a2b3c4d-00000001") not in published:
        print("missing call ID lifecycle publishes", file=sys.stderr)
        return 1
    if ("wibox/test/system/reboot/availability", "offline") not in published:
        print("missing one-shot reboot availability", file=sys.stderr)
        return 1
    if ("wibox/test/developer/simulate_ding/availability", "online") not in published or \
       ("wibox/test/developer/simulate_ding/availability", "offline") not in published:
        print("missing developer mode availability lifecycle", file=sys.stderr)
        return 1
    if ("wibox/test/call/outgoing_enabled", "OFF") not in published or \
       ("wibox/test/call/target_uri", "sip:3000@example.test") not in published:
        print("missing outgoing SIP runtime state", file=sys.stderr)
        return 1
    if "wibox/test/call/id" not in retained_topics:
        print("call ID state must be retained", file=sys.stderr)
        return 1
    if "wibox/test/call/event" in retained_topics:
        print("call events must not be retained", file=sys.stderr)
        return 1
    call_events = [json.loads(payload) for topic, payload in published
                   if topic == "wibox/test/call/event"]
    if not any(event.get("event_type") == "established" and
               event.get("call_id") == "1a2b3c4d-00000001" and
               event.get("sequence") == 3 and
               event.get("source") == "physical_panel" and
               event.get("route") == "sip" and
               event.get("media_state") == "established" and
               event.get("reason") == "mqtt-e2e" and
               event.get("terminal") is False and
               event.get("started_at") == 1000 and event.get("ts") == 1002
               for event in call_events):
        print("missing or invalid structured call event", file=sys.stderr)
        return 1
    if not any(topic == "wibox/test/door/unlocked" and body == "ON" for topic, body in published):
        print("missing unlock ON publish", file=sys.stderr)
        return 1
    if not any(topic == "wibox/test/door/unlocked" and body == "OFF" for topic, body in published):
        print("missing unlock OFF publish", file=sys.stderr)
        return 1
    if not any(topic.endswith("_firmware_version/config") for topic, _ in published):
        print("missing firmware version Home Assistant discovery publish", file=sys.stderr)
        return 1
    if not any(topic.endswith("_firmware_commit/config") for topic, _ in published):
        print("missing firmware commit Home Assistant discovery publish", file=sys.stderr)
        return 1
    if not any(topic.endswith("_firmware_build_timestamp/config") for topic, _ in published):
        print("missing firmware build timestamp Home Assistant discovery publish", file=sys.stderr)
        return 1
    if not any(topic.endswith("_call_forward_enabled/config") for topic, _ in published):
        print("missing call forward Home Assistant discovery publish", file=sys.stderr)
        return 1
    if not any(topic.endswith("_f1_function/config") for topic, _ in published):
        print("missing F1 function Home Assistant discovery publish", file=sys.stderr)
        return 1
    if not any(topic.endswith("_take_snapshot/config") for topic, _ in published):
        print("missing snapshot button Home Assistant discovery publish", file=sys.stderr)
        return 1
    if not any(topic.endswith("_support_report/config") and
               'support/report/set' in payload for topic, payload in published):
        print("missing support report Home Assistant discovery publish", file=sys.stderr)
        return 1
    support_reports = [json.loads(payload) for topic, payload in published
                       if topic == "wibox/test/support/report"]
    if len(support_reports) != 1 or support_reports[0].get("event_type") != "support_report":
        print("missing support report publish or retained command was not ignored", file=sys.stderr)
        return 1
    if not any(topic.endswith("_take_snapshot/config") and
               'snapshot/take/availability' in payload
               for topic, payload in published):
        print("missing snapshot button availability topic", file=sys.stderr)
        return 1
    if not any(topic.endswith("_developer_simulate_handset_answered/config")
               for topic, _ in published):
        print("missing developer handset button discovery publish", file=sys.stderr)
        return 1
    if not any(topic.endswith("_developer_simulate_handset_answered/config") and
               'developer/simulate_handset_answered/availability' in payload
               for topic, payload in published):
        print("missing developer handset button availability topic", file=sys.stderr)
        return 1
    if ("wibox/test/developer/simulate_handset_answered/availability", "offline") \
            not in published:
        print("missing developer handset disabled availability publish", file=sys.stderr)
        return 1
    if not any(topic.endswith("_snapshot/config") and '"image_encoding":"b64"' in payload
               for topic, payload in published):
        print("missing snapshot image Home Assistant discovery publish", file=sys.stderr)
        return 1
    if not any(topic == "wibox/test/snapshot/image" and body.startswith("/9gB")
               for topic, body in published):
        print("missing base64 snapshot payload", file=sys.stderr)
        return 1
    uart_events = [json.loads(payload) for topic, payload in published
                   if topic == "wibox/test/uart/event"]
    if not any(event.get("event_type") == "alarm_report" and
               event.get("alias") == "ALARM_REPORT" and
               event.get("direction") == "in" and event.get("known") is True
               for event in uart_events):
        print("missing decoded RX UART event", file=sys.stderr)
        return 1
    if not any(event.get("event_type") == "start_call" and
               event.get("direction") == "tx" for event in uart_events):
        print("missing TX UART event", file=sys.stderr)
        return 1
    if ("wibox/test/snapshot/take/availability", "offline") not in published:
        print("missing snapshot disabled availability publish", file=sys.stderr)
        return 1
    if ("wibox/test/call_forward/enabled", "ON") not in published:
        print("missing retained call forward initial state", file=sys.stderr)
        return 1
    if not any(topic.endswith("_video_bitrate/config") and '"mode":"slider"' in payload
               for topic, payload in published):
        print("missing video bitrate Home Assistant discovery publish", file=sys.stderr)
        return 1
    if not any(topic.endswith("_outgoing_call_timeout/config") and '"mode":"slider"' in payload
               for topic, payload in published):
        print("missing outgoing call timeout Home Assistant discovery publish", file=sys.stderr)
        return 1
    if not any(topic.endswith("_ring_snapshot_delay/config") and '"mode":"slider"' in payload
               for topic, payload in published):
        print("missing ring snapshot delay Home Assistant discovery publish", file=sys.stderr)
        return 1
    if not any(topic.endswith("_rtsp_enabled/config") for topic, _ in published):
        print("missing RTSP enabled Home Assistant discovery publish", file=sys.stderr)
        return 1
    if ("wibox/test/video/enabled", "OFF") not in published:
        print("missing default video disabled state publish", file=sys.stderr)
        return 1
    if ("wibox/test/rtsp/enabled", "OFF") not in published:
        print("missing default RTSP disabled state publish", file=sys.stderr)
        return 1
    if ("wibox/test/rtsp/enabled", "ON") not in published:
        print("missing retained RTSP enabled state publish", file=sys.stderr)
        return 1
    if ("wibox/test/video/bitrate_kbps", "4096") not in published:
        print("missing default video bitrate state publish", file=sys.stderr)
        return 1
    if ("wibox/test/call/timeout_seconds", "60") not in published:
        print("missing default outgoing call timeout state publish", file=sys.stderr)
        return 1
    if ("wibox/test/snapshot/ring_delay_ms", "2000") not in published:
        print("missing default ring snapshot delay state publish", file=sys.stderr)
        return 1
    if ("homeassistant/sensor/wibox_test_last_ring/config", "") not in published:
        print("missing retained last ring discovery cleanup", file=sys.stderr)
        return 1
    if ("wibox/test/ringing/last", "") not in published:
        print("missing retained last ring state cleanup", file=sys.stderr)
        return 1
    for topic, payload in published:
        if topic.endswith("_firmware_update_available/config") or \
           topic.endswith("_firmware_update_version/config") or \
           topic.endswith("_firmware_update_install/config") or \
           topic.endswith("_firmware_update_refresh/config"):
            if payload not in ("", None):
                print("unexpected firmware update discovery publish in disabled test", file=sys.stderr)
                return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
