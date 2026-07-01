#!/usr/bin/env python3
import json
import os
import socket
import struct
import sys
import time


def env(name, default=""):
    return os.environ.get(name, default)


MQTT_HOST = env("MQTT_HOST", "127.0.0.1")
MQTT_PORT = int(env("MQTT_PORT", "1883"))
MQTT_USER = env("MQTT_USER")
MQTT_PASS = env("MQTT_PASS")
MQTT_HA_PREFIX = env("MQTT_HA_PREFIX", "homeassistant").strip("/")
MQTT_DEVICE = env("MQTT_DEVICE", "IDS7938jrvc")
MQTT_BASE_TOPIC = env("MQTT_BASE_TOPIC", f"wibox/{MQTT_DEVICE}").strip("/")
TIMEOUT = float(env("MQTT_VERIFY_TIMEOUT", "8"))

DEVICE_SLUG = MQTT_DEVICE.lower()
HA_ID = f"wibox_{DEVICE_SLUG}"


def enc_str(value):
    data = value.encode("utf-8")
    return struct.pack("!H", len(data)) + data


def enc_remaining(length):
    out = bytearray()
    while True:
        digit = length % 128
        length //= 128
        if length:
            digit |= 0x80
        out.append(digit)
        if not length:
            return bytes(out)


def packet(packet_type, body):
    return bytes([packet_type]) + enc_remaining(len(body)) + body


def read_exact(sock, count):
    data = b""
    while len(data) < count:
        chunk = sock.recv(count - len(data))
        if not chunk:
            raise EOFError("broker closed connection")
        data += chunk
    return data


def read_packet(sock):
    header = read_exact(sock, 1)[0]
    multiplier = 1
    remaining = 0
    while True:
        digit = read_exact(sock, 1)[0]
        remaining += (digit & 0x7F) * multiplier
        if not (digit & 0x80):
            break
        multiplier *= 128
        if multiplier > 128 * 128 * 128:
            raise ValueError("invalid MQTT remaining length")
    return header, read_exact(sock, remaining)


def connect():
    sock = socket.create_connection((MQTT_HOST, MQTT_PORT), timeout=TIMEOUT)
    sock.settimeout(TIMEOUT)

    flags = 0x02
    if MQTT_USER:
        flags |= 0x80
    if MQTT_PASS:
        flags |= 0x40

    client_id = f"wibox-verify-{int(time.time())}"
    body = enc_str("MQTT") + bytes([4, flags]) + struct.pack("!H", 30)
    body += enc_str(client_id)
    if MQTT_USER:
        body += enc_str(MQTT_USER)
    if MQTT_PASS:
        body += enc_str(MQTT_PASS)
    sock.sendall(packet(0x10, body))

    header, data = read_packet(sock)
    if header != 0x20 or len(data) < 2 or data[1] != 0:
        code = data[1] if len(data) > 1 else "?"
        raise RuntimeError(f"MQTT CONNACK failed, code={code}")
    return sock


def subscribe(sock, topics):
    body = struct.pack("!H", 1)
    for topic in topics:
        body += enc_str(topic) + b"\x00"
    sock.sendall(packet(0x82, body))

    header, data = read_packet(sock)
    if header != 0x90:
        raise RuntimeError(f"expected SUBACK, got packet type 0x{header:02x}")
    failures = [qos for qos in data[2:] if qos == 0x80]
    if failures:
        raise RuntimeError("broker rejected one or more subscriptions")


def collect(sock, topics):
    wanted = set(topics)
    seen = {}
    deadline = time.time() + TIMEOUT
    while time.time() < deadline and wanted - set(seen):
        try:
            header, data = read_packet(sock)
        except socket.timeout:
            break
        if header >> 4 != 3:
            continue

        qos = (header >> 1) & 0x03
        pos = 0
        topic_len = struct.unpack("!H", data[pos:pos + 2])[0]
        pos += 2
        topic = data[pos:pos + topic_len].decode("utf-8")
        pos += topic_len
        if qos:
            pos += 2
        payload = data[pos:].decode("utf-8", errors="replace")
        if topic in wanted:
            seen[topic] = payload
    return seen


def assert_present(seen, topic):
    payload = seen.get(topic)
    if payload is None or payload == "":
        raise AssertionError(f"missing retained payload for {topic}")
    return payload


def assert_config(seen, component, object_id, expected_state_topic,
                  expected_device_class=None, no_device_class=False,
                  expected_icon=None):
    topic = f"{MQTT_HA_PREFIX}/{component}/{HA_ID}_{object_id}/config"
    payload = assert_present(seen, topic)
    try:
        config = json.loads(payload)
    except json.JSONDecodeError as exc:
        raise AssertionError(f"invalid JSON for {topic}: {exc}") from exc

    state_topic = config.get("state_topic")
    if state_topic != expected_state_topic:
        raise AssertionError(f"{topic} state_topic={state_topic!r}, expected {expected_state_topic!r}")
    if no_device_class and "device_class" in config:
        raise AssertionError(f"{topic} should not set device_class")
    if expected_device_class is not None and config.get("device_class") != expected_device_class:
        raise AssertionError(
            f"{topic} device_class={config.get('device_class')!r}, expected {expected_device_class!r}"
        )
    if expected_icon is not None and config.get("icon") != expected_icon:
        raise AssertionError(f"{topic} icon={config.get('icon')!r}, expected {expected_icon!r}")
    return config


def assert_command_config(seen, component, object_id, expected_command_topic,
                          expected_icon=None, expected_availability_topic=None):
    topic = f"{MQTT_HA_PREFIX}/{component}/{HA_ID}_{object_id}/config"
    payload = assert_present(seen, topic)
    try:
        config = json.loads(payload)
    except json.JSONDecodeError as exc:
        raise AssertionError(f"invalid JSON for {topic}: {exc}") from exc

    command_topic = config.get("command_topic")
    if command_topic != expected_command_topic:
        raise AssertionError(f"{topic} command_topic={command_topic!r}, expected {expected_command_topic!r}")
    if expected_availability_topic is not None:
        availability_topic = config.get("availability_topic")
        if availability_topic != expected_availability_topic:
            raise AssertionError(
                f"{topic} availability_topic={availability_topic!r}, expected {expected_availability_topic!r}"
            )
    if expected_icon is not None and config.get("icon") != expected_icon:
        raise AssertionError(f"{topic} icon={config.get('icon')!r}, expected {expected_icon!r}")
    return config


def assert_absent(seen, topic):
    payload = seen.get(topic)
    if payload not in (None, ""):
        raise AssertionError(f"expected {topic} to be cleared, got {payload!r}")


def main():
    topics = [
        MQTT_BASE_TOPIC,
        f"{MQTT_BASE_TOPIC}/media/state",
        f"{MQTT_BASE_TOPIC}/firmware/version",
        f"{MQTT_BASE_TOPIC}/firmware/commit",
        f"{MQTT_BASE_TOPIC}/firmware/build_timestamp",
        f"{MQTT_BASE_TOPIC}/wifi/rssi",
        f"{MQTT_BASE_TOPIC}/video/enabled",
        f"{MQTT_BASE_TOPIC}/firmware/update/install/availability",
        f"{MQTT_BASE_TOPIC}/firmware/update/available",
        f"{MQTT_BASE_TOPIC}/firmware/update/version",
        f"{MQTT_HA_PREFIX}/button/{HA_ID}_open_door/config",
        f"{MQTT_HA_PREFIX}/sensor/{HA_ID}_media_state/config",
        f"{MQTT_HA_PREFIX}/sensor/{HA_ID}_firmware_version/config",
        f"{MQTT_HA_PREFIX}/sensor/{HA_ID}_firmware_commit/config",
        f"{MQTT_HA_PREFIX}/sensor/{HA_ID}_firmware_build_timestamp/config",
        f"{MQTT_HA_PREFIX}/button/{HA_ID}_firmware_update_install/config",
        f"{MQTT_HA_PREFIX}/button/{HA_ID}_firmware_update_refresh/config",
        f"{MQTT_HA_PREFIX}/binary_sensor/{HA_ID}_firmware_update_available/config",
        f"{MQTT_HA_PREFIX}/sensor/{HA_ID}_firmware_update_version/config",
        f"{MQTT_HA_PREFIX}/binary_sensor/{HA_ID}_door_unlocked/config",
        f"{MQTT_HA_PREFIX}/switch/{HA_ID}_video_enabled/config",
        f"{MQTT_HA_PREFIX}/sensor/{HA_ID}_wifi_rssi/config",
        f"{MQTT_HA_PREFIX}/binary_sensor/{HA_ID}_ringing/config",
        f"{MQTT_HA_PREFIX}/binary_sensor/{HA_ID}_call_active/config",
        f"{MQTT_HA_PREFIX}/binary_sensor/{HA_ID}_sip_call_active/config",
        f"{MQTT_HA_PREFIX}/binary_sensor/{HA_ID}_video_active/config",
        f"{MQTT_BASE_TOPIC}/call/active",
        f"{MQTT_BASE_TOPIC}/sip/active",
        f"{MQTT_BASE_TOPIC}/video/active",
    ]

    print(f"[*] Connecting to MQTT broker {MQTT_HOST}:{MQTT_PORT}")
    sock = connect()
    try:
        subscribe(sock, topics)
        seen = collect(sock, topics)
    finally:
        sock.close()

    assert_present(seen, MQTT_BASE_TOPIC)
    media_state = assert_present(seen, f"{MQTT_BASE_TOPIC}/media/state")
    firmware_version = assert_present(seen, f"{MQTT_BASE_TOPIC}/firmware/version")
    firmware_commit = assert_present(seen, f"{MQTT_BASE_TOPIC}/firmware/commit")
    firmware_build_timestamp = assert_present(seen, f"{MQTT_BASE_TOPIC}/firmware/build_timestamp")
    wifi_rssi = assert_present(seen, f"{MQTT_BASE_TOPIC}/wifi/rssi")
    video_enabled = assert_present(seen, f"{MQTT_BASE_TOPIC}/video/enabled")
    firmware_update_install_availability = assert_present(
        seen, f"{MQTT_BASE_TOPIC}/firmware/update/install/availability"
    )

    assert_command_config(seen, "button", "open_door", f"{MQTT_BASE_TOPIC}/door/open/set",
                          expected_icon="mdi:door-open")
    assert_config(seen, "sensor", "media_state", f"{MQTT_BASE_TOPIC}/media/state",
                  no_device_class=True, expected_icon="mdi:phone")
    assert_config(seen, "sensor", "firmware_version", f"{MQTT_BASE_TOPIC}/firmware/version",
                  no_device_class=True, expected_icon="mdi:tag")
    assert_config(seen, "sensor", "firmware_commit", f"{MQTT_BASE_TOPIC}/firmware/commit",
                  no_device_class=True, expected_icon="mdi:source-commit")
    assert_config(seen, "sensor", "firmware_build_timestamp", f"{MQTT_BASE_TOPIC}/firmware/build_timestamp",
                  expected_device_class="timestamp", expected_icon="mdi:clock-outline")
    assert_command_config(
        seen, "button", "firmware_update_install", f"{MQTT_BASE_TOPIC}/firmware/update/install/set",
        expected_icon="mdi:update",
        expected_availability_topic=f"{MQTT_BASE_TOPIC}/firmware/update/install/availability"
    )
    assert_command_config(
        seen, "button", "firmware_update_refresh", f"{MQTT_BASE_TOPIC}/firmware/update/check/set",
        expected_icon="mdi:refresh",
        expected_availability_topic=MQTT_BASE_TOPIC
    )
    assert_config(seen, "binary_sensor", "firmware_update_available", f"{MQTT_BASE_TOPIC}/firmware/update/available",
                  expected_icon="mdi:update")
    assert_config(seen, "sensor", "firmware_update_version", f"{MQTT_BASE_TOPIC}/firmware/update/version",
                  no_device_class=True, expected_icon="mdi:tag")
    assert_absent(seen, f"{MQTT_HA_PREFIX}/sensor/{HA_ID}_last_ring/config")
    assert_absent(seen, f"{MQTT_HA_PREFIX}/sensor/{HA_ID}_last_unlock/config")
    assert_absent(seen, f"{MQTT_BASE_TOPIC}/door/last_unlock")
    assert_config(seen, "binary_sensor", "door_unlocked", f"{MQTT_BASE_TOPIC}/door/unlocked",
                  expected_icon="mdi:lock-open")
    assert_config(seen, "switch", "video_enabled", f"{MQTT_BASE_TOPIC}/video/enabled",
                  expected_icon="mdi:video")
    assert_config(seen, "sensor", "wifi_rssi", f"{MQTT_BASE_TOPIC}/wifi/rssi",
                  expected_device_class="signal_strength", expected_icon="mdi:wifi")

    assert_absent(seen, f"{MQTT_HA_PREFIX}/binary_sensor/{HA_ID}_call_active/config")
    assert_absent(seen, f"{MQTT_HA_PREFIX}/binary_sensor/{HA_ID}_sip_call_active/config")
    assert_absent(seen, f"{MQTT_HA_PREFIX}/binary_sensor/{HA_ID}_video_active/config")
    assert_absent(seen, f"{MQTT_BASE_TOPIC}/call/active")
    assert_absent(seen, f"{MQTT_BASE_TOPIC}/sip/active")
    assert_absent(seen, f"{MQTT_BASE_TOPIC}/video/active")

    print(f"[*] MQTT discovery/state OK for {MQTT_BASE_TOPIC}")
    print(f"    media/state={media_state} firmware/version={firmware_version} "
          f"firmware/commit={firmware_commit} firmware/build_timestamp={firmware_build_timestamp} "
          f"wifi/rssi={wifi_rssi} video/enabled={video_enabled} "
          f"firmware/update/install/availability={firmware_update_install_availability}")


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print(f"[!] MQTT verification failed: {exc}", file=sys.stderr)
        sys.exit(1)
