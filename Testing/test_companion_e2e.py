#!/usr/bin/env python3
"""End-to-end test: auth + status messages with GPS, battery, time."""

import socket
import json
import hmac
import hashlib
import time
import sys

PI_HOST = "192.168.1.149"
PI_PORT = 9876
SECRET = sys.argv[1] if len(sys.argv) > 1 else "test-secret-for-development"

def compute_hmac(key: bytes, data: bytes) -> str:
    return hmac.new(key, data, hashlib.sha256).hexdigest()

def send_json(sock, obj):
    msg = json.dumps(obj, separators=(',', ':'), sort_keys=True)
    sock.send((msg + "\n").encode('utf-8'))

def recv_json(sock):
    data = sock.recv(4096).decode('utf-8').strip()
    return json.loads(data)

def main():
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(5)
    print(f"Connecting to {PI_HOST}:{PI_PORT}...")
    sock.connect((PI_HOST, PI_PORT))

    # 1. Receive challenge
    challenge = recv_json(sock)
    print(f"Received: {challenge}")
    if challenge.get("type") == "error":
        print(f"Server error: {challenge.get('msg')}")
        sock.close()
        return
    print(f"Challenge received, nonce={challenge['nonce'][:16]}...")

    # 2. Auth
    nonce = challenge["nonce"]
    token = compute_hmac(SECRET.encode('utf-8'), nonce.encode('utf-8'))
    send_json(sock, {"type": "hello", "token": token, "device_name": "test-e2e"})

    ack = recv_json(sock)
    if not ack.get("accepted"):
        print("Auth failed!")
        sock.close()
        return

    session_key = bytes.fromhex(ack["session_key"])
    print(f"Authenticated! Session key received.")

    # 3. Send status messages
    for seq in range(1, 4):
        status = {
            "type": "status",
            "seq": seq,
            "time_ms": int(time.time() * 1000),
            "gps": {
                "lat": round(41.8781 + seq * 0.001, 6),
                "lon": round(-87.6298 + seq * 0.001, 6),
                "speed": 25.5,
                "accuracy": 3,
                "bearing": 180,
                "age_ms": 500
            },
            "battery": {
                "level": 75 - seq,
                "charging": True
            },
            "socks5": {
                "active": True,
                "port": 1080
            }
        }

        # Compute MAC over the payload (without mac field)
        # IMPORTANT: Qt's QJsonDocument serializes keys alphabetically,
        # so we must sort_keys to match
        payload_bytes = json.dumps(status, separators=(',', ':'), sort_keys=True).encode('utf-8')
        mac = compute_hmac(session_key, payload_bytes)
        status["mac"] = mac

        send_json(sock, status)
        print(f"Sent status seq={seq}: GPS=({status['gps']['lat']:.4f}, {status['gps']['lon']:.4f}), battery={status['battery']['level']}%")
        time.sleep(1)

    print("\nDone! Check Pi logs and settings page for received data.")
    time.sleep(1)
    sock.close()

if __name__ == "__main__":
    main()
