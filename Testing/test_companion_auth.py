#!/usr/bin/env python3
"""Test the companion protocol HMAC authentication."""

import socket
import json
import hmac
import hashlib
import sys

PI_HOST = "192.168.1.149"
PI_PORT = 9876
SECRET = "test-secret-for-development"

def test_auth():
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(5)
    print(f"Connecting to {PI_HOST}:{PI_PORT}...")
    sock.connect((PI_HOST, PI_PORT))

    # Read challenge
    data = sock.recv(4096).decode('utf-8').strip()
    print(f"Received: {data}")
    challenge = json.loads(data)

    nonce = challenge["nonce"]
    print(f"Nonce: {nonce[:16]}...")
    print(f"Secret: {SECRET}")

    # Compute HMAC-SHA256(key=secret, message=nonce)
    # Qt computeHmac(key, data) calls QMessageAuthenticationCode::hash(data, key, ...)
    # Qt hash(message, key, method) — so message=nonce_bytes, key=secret_bytes
    token = hmac.new(
        SECRET.encode('utf-8'),      # key
        nonce.encode('utf-8'),        # message (the hex string, not decoded bytes)
        hashlib.sha256
    ).hexdigest()

    print(f"Computed token: {token[:16]}...")

    # Send hello
    hello = json.dumps({"type": "hello", "token": token, "device_name": "test-client"})
    print(f"Sending: {hello[:80]}...")
    sock.send((hello + "\n").encode('utf-8'))

    # Read response
    resp_data = sock.recv(4096).decode('utf-8').strip()
    print(f"Response: {resp_data}")
    resp = json.loads(resp_data)

    if resp.get("accepted"):
        print("AUTH SUCCESS!")
    else:
        print("AUTH FAILED!")

        # Let's also try with nonce as raw bytes (decoded from hex)
        print("\n--- Trying alternate: HMAC with nonce decoded from hex ---")
        sock.close()
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(5)
        sock.connect((PI_HOST, PI_PORT))

        data2 = sock.recv(4096).decode('utf-8').strip()
        challenge2 = json.loads(data2)
        nonce2 = challenge2["nonce"]

        token2 = hmac.new(
            SECRET.encode('utf-8'),
            bytes.fromhex(nonce2),  # decoded raw bytes
            hashlib.sha256
        ).hexdigest()

        hello2 = json.dumps({"type": "hello", "token": token2, "device_name": "test-client"})
        sock.send((hello2 + "\n").encode('utf-8'))

        resp_data2 = sock.recv(4096).decode('utf-8').strip()
        print(f"Response: {resp_data2}")
        resp2 = json.loads(resp_data2)
        if resp2.get("accepted"):
            print("AUTH SUCCESS with decoded nonce!")
        else:
            print("Still failed with decoded nonce.")

    sock.close()

if __name__ == "__main__":
    test_auth()
