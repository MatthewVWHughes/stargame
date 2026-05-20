#!/usr/bin/env python3
import base64
import hashlib
import json
import os
import socket
import struct
import sys
import time


GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"


def _recv_until(sock, marker):
    data = b""
    while marker not in data:
        chunk = sock.recv(4096)
        if not chunk:
            break
        data += chunk
    return data


def _send_frame(sock, payload):
    raw = payload.encode("utf-8")
    header = bytearray([0x81])
    length = len(raw)
    if length < 126:
        header.append(0x80 | length)
    elif length < 65536:
        header.append(0x80 | 126)
        header.extend(struct.pack("!H", length))
    else:
        header.append(0x80 | 127)
        header.extend(struct.pack("!Q", length))
    mask = os.urandom(4)
    header.extend(mask)
    masked = bytes(raw[i] ^ mask[i % 4] for i in range(length))
    sock.sendall(bytes(header) + masked)


def _recv_exact(sock, n):
    data = b""
    while len(data) < n:
        chunk = sock.recv(n - len(data))
        if not chunk:
            raise RuntimeError("socket closed")
        data += chunk
    return data


def _recv_frame(sock, timeout=30.0):
    sock.settimeout(timeout)
    first = _recv_exact(sock, 2)
    b1, b2 = first[0], first[1]
    opcode = b1 & 0x0F
    masked = bool(b2 & 0x80)
    length = b2 & 0x7F
    if length == 126:
        length = struct.unpack("!H", _recv_exact(sock, 2))[0]
    elif length == 127:
        length = struct.unpack("!Q", _recv_exact(sock, 8))[0]
    mask = _recv_exact(sock, 4) if masked else b""
    payload = _recv_exact(sock, length) if length else b""
    if masked:
        payload = bytes(payload[i] ^ mask[i % 4] for i in range(length))
    if opcode == 8:
        raise RuntimeError("websocket closed by peer")
    if opcode == 9:
        # ping: send pong
        return _recv_frame(sock, timeout)
    return payload.decode("utf-8", errors="replace")


def connect(host="127.0.0.1", ports=(8091, 8090)):
    last_error = None
    for port in ports:
        try:
            sock = socket.create_connection((host, port), timeout=5)
            key = base64.b64encode(os.urandom(16)).decode("ascii")
            request = (
                f"GET / HTTP/1.1\r\n"
                f"Host: {host}:{port}\r\n"
                "Upgrade: websocket\r\n"
                "Connection: Upgrade\r\n"
                f"Sec-WebSocket-Key: {key}\r\n"
                "Sec-WebSocket-Version: 13\r\n"
                "Sec-WebSocket-Protocol: mcp-automation\r\n"
                "\r\n"
            )
            sock.sendall(request.encode("ascii"))
            response = _recv_until(sock, b"\r\n\r\n")
            if b" 101 " not in response and b" 101\r\n" not in response:
                raise RuntimeError(response.decode("utf-8", errors="replace"))
            expected = base64.b64encode(hashlib.sha1((key + GUID).encode("ascii")).digest()).decode("ascii")
            if expected not in response.decode("utf-8", errors="replace"):
                raise RuntimeError("bad websocket accept")
            return sock, port
        except Exception as exc:
            last_error = exc
    raise RuntimeError(f"Could not connect to WebSocket bridge: {last_error}")


def call(action, payload, timeout=30.0):
    sock, port = connect()
    try:
        _send_frame(sock, json.dumps({"type": "bridge_hello"}))
        hello = json.loads(_recv_frame(sock, timeout))
        if hello.get("type") != "bridge_ack":
            raise RuntimeError(f"handshake failed: {hello}")
        request_id = f"codex_{int(time.time() * 1000)}"
        _send_frame(sock, json.dumps({
            "type": "automation_request",
            "requestId": request_id,
            "action": action,
            "payload": payload,
        }))
        deadline = time.time() + timeout
        messages = []
        while time.time() < deadline:
            msg = json.loads(_recv_frame(sock, max(0.5, deadline - time.time())))
            messages.append(msg)
            if msg.get("requestId") == request_id and msg.get("type") in {"automation_response", "automation_error"}:
                return {"port": port, "response": msg, "messages": messages}
        raise TimeoutError(f"timed out waiting for {request_id}; messages={messages}")
    finally:
        try:
            sock.close()
        except Exception:
            pass


def main():
    if len(sys.argv) < 3:
        print("usage: ws_bridge_call.py ACTION JSON_PAYLOAD", file=sys.stderr)
        return 2
    action = sys.argv[1]
    payload = json.loads(sys.argv[2])
    result = call(action, payload)
    print(json.dumps(result, indent=2))
    return 0 if result["response"].get("success", False) else 1


if __name__ == "__main__":
    raise SystemExit(main())
