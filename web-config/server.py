#!/usr/bin/env python3
"""OpenAuto Prodigy — Web Configuration Panel.

Lightweight Flask server accessible at http://10.0.0.1:8080 (or localhost for dev).
Communicates with the Qt main app via Unix domain socket IPC.
Single-writer rule: only the Qt app writes config files.
"""

import json
import os
import socket
import sys
from pathlib import Path

from flask import Flask, render_template, request, jsonify

app = Flask(__name__)

# Unix socket path for IPC with the Qt app
IPC_SOCKET = os.environ.get("OAP_IPC_SOCKET", "/tmp/openauto-prodigy.sock")


def ipc_request(command: str, data: dict | None = None) -> dict:
    """Send a JSON request to the Qt app via Unix socket and return the response."""
    msg = {"command": command}
    if data:
        msg["data"] = data

    try:
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        sock.settimeout(5.0)
        sock.connect(IPC_SOCKET)
        sock.sendall(json.dumps(msg).encode() + b"\n")

        # Read response (newline-delimited JSON)
        buf = b""
        while b"\n" not in buf:
            chunk = sock.recv(4096)
            if not chunk:
                break
            buf += chunk
        sock.close()

        if buf:
            return json.loads(buf.decode().strip())
        return {"error": "Empty response from app"}
    except FileNotFoundError:
        return {"error": "Qt app not running (IPC socket not found)"}
    except ConnectionRefusedError:
        return {"error": "Qt app not accepting connections"}
    except socket.timeout:
        return {"error": "IPC request timed out"}
    except Exception as e:
        return {"error": str(e)}


# --- Routes ---

@app.route("/")
def dashboard():
    """Dashboard — system status overview."""
    status = ipc_request("status")
    return render_template("index.html", page="dashboard", status=status)


@app.route("/settings")
def settings():
    """Settings page — config.yaml editor."""
    config = ipc_request("get_config")
    return render_template("settings.html", page="settings", config=config)


@app.route("/themes")
def themes():
    """Theme editor — color pickers for day/night colors."""
    theme = ipc_request("get_theme")
    return render_template("themes.html", page="themes", theme=theme)


@app.route("/audio")
def audio():
    """Audio settings — device selection, volume control."""
    devices = ipc_request("get_audio_devices")
    config = ipc_request("get_audio_config")
    return render_template("audio.html", page="audio", devices=devices, audio_config=config)


@app.route("/plugins")
def plugins():
    """Plugin management — enable/disable, view info."""
    plugin_list = ipc_request("list_plugins")
    return render_template("plugins.html", page="plugins", plugins=plugin_list)


# --- API endpoints ---

@app.route("/api/config", methods=["GET"])
def api_get_config():
    return jsonify(ipc_request("get_config"))


@app.route("/api/config", methods=["POST"])
def api_set_config():
    data = request.get_json()
    if not data:
        return jsonify({"error": "No JSON data"}), 400
    return jsonify(ipc_request("set_config", data))


@app.route("/api/theme", methods=["GET"])
def api_get_theme():
    return jsonify(ipc_request("get_theme"))


@app.route("/api/theme", methods=["POST"])
def api_set_theme():
    data = request.get_json()
    if not data:
        return jsonify({"error": "No JSON data"}), 400
    return jsonify(ipc_request("set_theme", data))


@app.route("/api/audio/devices", methods=["GET"])
def api_get_audio_devices():
    return jsonify(ipc_request("get_audio_devices"))


@app.route("/api/audio/config", methods=["GET"])
def api_get_audio_config():
    return jsonify(ipc_request("get_audio_config"))


@app.route("/api/audio/config", methods=["POST"])
def api_set_audio_config():
    data = request.get_json()
    if not data:
        return jsonify({"error": "No JSON data"}), 400
    return jsonify(ipc_request("set_audio_config", data))


@app.route("/api/plugins", methods=["GET"])
def api_list_plugins():
    return jsonify(ipc_request("list_plugins"))


@app.route("/api/status", methods=["GET"])
def api_status():
    return jsonify(ipc_request("status"))


if __name__ == "__main__":
    host = os.environ.get("OAP_WEB_HOST", "0.0.0.0")
    port = int(os.environ.get("OAP_WEB_PORT", "8080"))
    debug = os.environ.get("OAP_WEB_DEBUG", "0") == "1"

    print(f"OpenAuto Prodigy Web Config — http://{host}:{port}")
    print(f"IPC socket: {IPC_SOCKET}")
    app.run(host=host, port=port, debug=debug)
