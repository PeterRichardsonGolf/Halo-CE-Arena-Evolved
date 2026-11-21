#!/usr/bin/env python3
"""A small bot for the dedicated server's remote console (docs/dedicated_server.md, "The RCON"), standard library only.

It signs in with an API key, subscribes to the events and answers some of them with server commands:

    server_rcon_bot.py --port 8080 --key aek_xxxxxxxx_... [--seconds 600]

A player joins: a welcome notice. A game ends: a thank-you. The bot ignores every event and field it does not know, so
a newer server does not break it. Make the key with `sv_key_add bot moderator` (a moderator may say, kick and mute).
"""
import argparse
import base64
import json
import os
import socket
import struct
import time


def connect(host, port):
    """a WebSocket to /ws with the RCON's subprotocol; the bytes read past the handshake"""
    sock = socket.create_connection((host, port), timeout=15)
    key = base64.b64encode(os.urandom(16)).decode()
    sock.sendall((f"GET /ws HTTP/1.1\r\nHost: {host}:{port}\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                  f"Sec-WebSocket-Version: 13\r\nSec-WebSocket-Key: {key}\r\n"
                  f"Sec-WebSocket-Protocol: opence-rcon-1\r\n\r\n").encode())
    data = b""
    while b"\r\n\r\n" not in data:
        chunk = sock.recv(4096)
        if not chunk:
            raise ConnectionError("closed during the handshake")
        data += chunk
    head, rest = data.split(b"\r\n\r\n", 1)
    if not head.startswith(b"HTTP/1.1 101"):
        raise ConnectionError(head.split(b"\r\n")[0].decode())
    return sock, rest


def send(sock, message):
    """a text frame, masked as a client must"""
    payload = json.dumps(message).encode()
    mask = os.urandom(4)
    size = len(payload)
    head = bytes([0x81]) + (bytes([0x80 | size]) if size < 126 else bytes([0x80 | 126]) + struct.pack(">H", size))
    sock.sendall(head + mask + bytes(byte ^ mask[index % 4] for index, byte in enumerate(payload)))


def receive(sock, buffer):
    """the next JSON message ({"closed": code} when the server closes), and the rest of the buffer"""
    def need(count):
        nonlocal buffer
        while len(buffer) < count:
            chunk = sock.recv(65536)
            if not chunk:
                raise ConnectionError("closed")
            buffer += chunk
    while True:
        need(2)
        opcode, size, offset = buffer[0] & 15, buffer[1] & 127, 2
        if size == 126:
            need(4)
            size, offset = struct.unpack(">H", buffer[2:4])[0], 4
        elif size == 127:
            need(10)
            size, offset = struct.unpack(">Q", buffer[2:10])[0], 10
        need(offset + size)
        payload, buffer = buffer[offset:offset + size], buffer[offset + size:]
        if opcode == 8:
            return {"closed": struct.unpack(">H", payload[:2])[0] if len(payload) >= 2 else 0}, buffer
        if opcode == 1:
            return json.loads(payload), buffer
        # (pings and anything else: ignored; the server's keep-alive is the JSON ping below)


def clean(text):
    """a player's name made safe to put in a command: printable ASCII, no quotes or semicolons"""
    return "".join(c if 32 <= ord(c) < 127 and c not in '";\\' else "_" for c in str(text))[:15]


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8080)
    parser.add_argument("--key", required=True, help="an API key (sv_key_add)")
    parser.add_argument("--seconds", type=int, default=600, help="how long to run (0: for ever)")
    args = parser.parse_args()

    sock, buffer = connect(args.host, args.port)
    message, buffer = receive(sock, buffer)
    assert message.get("type") == "hello", message
    send(sock, {"type": "auth", "key": args.key})
    message, buffer = receive(sock, buffer)
    if not message.get("ok"):
        raise SystemExit(f"signed out: {message}")
    print(f"signed in as {message['role']}", flush=True)
    send(sock, {"type": "subscribe", "events": ["player", "game", "moderation"]})
    command_id = 0
    deadline = time.time() + args.seconds if args.seconds else None
    sock.settimeout(5)
    while deadline is None or time.time() < deadline:
        try:
            message, buffer = receive(sock, buffer)
        except socket.timeout:
            send(sock, {"type": "ping"})
            continue
        if "closed" in message:
            raise SystemExit(f"the server closed the console ({message['closed']})")
        if message.get("type") != "event" or message.get("replay"):
            continue
        event = message.get("event")
        line = None
        if event == "player_joined":
            line = f'say Welcome, {clean(message.get("name", "player"))}!'
        elif event == "game_ended":
            line = "say Thanks for playing."
        print(f"event {event}", flush=True)
        if line:
            command_id += 1
            send(sock, {"type": "command", "id": command_id, "line": line})
            print(f"sent: {line}", flush=True)
    sock.close()


if __name__ == "__main__":
    main()
