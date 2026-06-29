#!/usr/bin/env python3
import socket
import struct
import subprocess
import sys
import threading
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HARNESS = Path("/tmp/wibox_mqtt_native_harness")
PORT = 18883


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


def publish(topic, payload):
    topic_b = topic.encode()
    payload_b = payload.encode()
    body = struct.pack("!H", len(topic_b)) + topic_b + payload_b
    return packet(0x30, body)


def broker(published):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server:
        server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server.bind(("127.0.0.1", PORT))
        server.listen(1)
        conn, _ = server.accept()
        with conn:
            conn.settimeout(10)
            typ, _ = read_packet(conn)
            if typ != 0x10:
                return
            conn.sendall(packet(0x20, b"\x00\x00"))

            typ, payload = read_packet(conn)
            if typ != 0x82:
                return
            packet_id = payload[:2]
            conn.sendall(packet(0x90, packet_id + b"\x00\x00"))

            deadline = time.time() + 5
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
                    if not sent_commands and topic == "wibox/test":
                        sent_commands = True
                        conn.sendall(publish("wibox/test/video/enabled/set", "OFF"))
                        conn.sendall(publish("wibox/test/door/open/set", "PRESS"))
                elif typ == 0xC0:
                    conn.sendall(packet(0xD0))


def main():
    build = [
        "gcc",
        "-Wall",
        "-Wextra",
        "-std=gnu99",
        "-pthread",
        "-Isrc/sip_media",
        "tests/mqtt_native_harness.c",
        "src/sip_media/mqtt.c",
        "src/sip_media/config.c",
        "-o",
        str(HARNESS),
    ]
    subprocess.run(build, cwd=ROOT, check=True)

    published = []
    thread = threading.Thread(target=broker, args=(published,), daemon=True)
    thread.start()
    time.sleep(0.1)

    proc = subprocess.run([str(HARNESS)], cwd=ROOT, text=True, capture_output=True)
    thread.join(timeout=2)
    sys.stdout.write(proc.stdout)
    sys.stderr.write(proc.stderr)

    if proc.returncode != 0:
        return proc.returncode
    if not any(topic.startswith("homeassistant/") for topic, _ in published):
        print("missing Home Assistant discovery publish", file=sys.stderr)
        return 1
    if ("wibox/test", "online") not in published:
        print("missing retained online publish", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
