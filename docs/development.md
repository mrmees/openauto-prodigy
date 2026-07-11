# Development Guide

## Build Environments

| Environment | Qt Version | Purpose |
|-------------|-----------|---------|
| **Raspberry Pi OS Trixie** (Pi 4) | Qt 6.8 | Target platform — run and verify here |
| **WSL2 / Debian Trixie** | Qt 6.8 | Dev builds, unit tests, cross-compiling |

Both environments run the same OS family and the same **system Qt** — no
`CMAKE_PREFIX_PATH`, no vendored Qt. Any Debian-Trixie-equivalent Linux works
for development. (An Ubuntu 24.04 / Qt 6.4 VM was used early in the project;
it is retired and dual-Qt-version compatibility is no longer a constraint.)

## Dependencies

### Raspberry Pi OS Trixie (Debian 13)

```bash
sudo apt install cmake g++ git pkg-config \
  qt6-base-dev qt6-declarative-dev qt6-wayland \
  qt6-connectivity-dev qt6-multimedia-dev qt6-websockets-dev \
  qml6-module-qtquick-controls qml6-module-qtquick-layouts \
  qml6-module-qtquick-window qml6-module-qtqml-workerscript \
  libboost-system-dev libboost-log-dev \
  libprotobuf-dev protobuf-compiler libssl-dev \
  libavformat-dev libavcodec-dev libavutil-dev \
  libpipewire-0.3-dev libspa-0.2-dev \
  libyaml-cpp-dev libbluetooth-dev \
  hostapd dnsmasq bluez \
  python3-flask \
  udisks2 \
  qt6-webengine-dev qml6-module-qtwebengine  # optional — web widget runtime; builds fine without
```

Note: `libbluetooth-dev` (raw BlueZ headers, `bluetooth/bluetooth.h`) is required in addition to `qt6-connectivity-dev` — `BluetoothDiscoveryService` uses BlueZ sockets/SDP directly. `install.sh` already includes it.

Note: `udisks2` is a Pi **runtime** dependency, not a build dep — it ships no
headers/libraries to link against. `UsbMediaWatcher` talks to its D-Bus
service to detect and mount USB media; the polkit grant that lets the
service user drive it without a password lives in
`config/udisks-polkit.rules` (installed by `install.sh` alongside the BlueZ
agent rule).

### WSL2 Debian Trixie (dev box)

Same package list minus `hostapd dnsmasq` (no AP on a dev box). Docker
(`docker.io`) enables `cross-build.sh` for Pi binaries.

If the repo checkout lives on a Windows drive (`/mnt/...`), keep the CMake
build directory on the Linux filesystem — object churn through the 9p mount
makes in-repo builds painfully slow. See `AGENTS.md` → Commands.

## Building

```bash
git clone --recurse-submodules https://github.com/mrmees/openauto-prodigy.git
cd openauto-prodigy
mkdir build && cd build
cmake ..
make -j$(nproc)
```

**Important:** Don't forget `--recurse-submodules` — the [open-android-auto](https://github.com/mrmees/open-android-auto) protobuf definitions are a git submodule under `libs/prodigy-oaa-protocol/proto/`.

### Interactive Install (RPi OS Trixie)

For a fresh Raspberry Pi, the install script handles everything:

```bash
bash install.sh
```

`install.sh` provides an install mode picker:
- Build locally from source.
- Download a precompiled release from GitHub.

Both install modes run platform validation first (Debian/RPi OS family, ARM architecture, and Pi 4 hardware model, with explicit continue prompts if mismatched).

To list available precompiled release assets without installing:

```bash
bash install.sh --list-prebuilt
```

The source mode installs dependencies, builds from source, generates config, and creates systemd services.

### Prebuilt Install (RPi OS Trixie)

If you have a packaged prebuilt release tarball, install without building:

```bash
tar -xzf openauto-prodigy-prebuilt-<tag>-pi4-aarch64.tar.gz
cd openauto-prodigy-prebuilt-<tag>-pi4-aarch64
bash install-prebuilt.sh
```

To create a prebuilt package from this repo:

```bash
./cross-build.sh -DCMAKE_BUILD_TYPE=Release
./tools/package-prebuilt-release.sh --build-dir build-pi --version-tag <tag>
```

`cross-build.sh` defaults to building only the app target (`openauto-prodigy`), which is all a Pi deploy or package needs (~4-6 min); pass `--full` to also build the ARM test binaries (~20 min).

Prebuilt release convention:
- Asset: `openauto-prodigy-prebuilt-<tag>-pi4-aarch64.tar.gz`
- Root dir: `openauto-prodigy-prebuilt-<tag>-pi4-aarch64/`
- Metadata: `RELEASE.json` at archive root

## Running

### On Raspberry Pi (Wayland/labwc)

```bash
export XDG_RUNTIME_DIR=/run/user/1000
export WAYLAND_DISPLAY=wayland-0
export QT_QPA_PLATFORM=wayland
./src/openauto-prodigy
```

The Pi runs labwc as its Wayland compositor. Do **not** use `eglfs` — it won't work with this display server setup.

### Via SSH

```bash
ssh matt@192.168.1.149 '~/openauto-prodigy/restart.sh'
# Validate without restarting:
ssh matt@192.168.1.149 '~/openauto-prodigy/restart.sh --check'
# Force-kill stuck process:
ssh matt@192.168.1.149 '~/openauto-prodigy/restart.sh --force-kill'
```

See `docs/pi-config/restart.sh` for the script source of truth.

### Logs

```bash
# systemd service (normal operation)
journalctl -u openauto-prodigy.service -n 200
# restart.sh runs log here instead
tail -f /tmp/oap.log
```

### Web Config Panel

```bash
cd web-config && python3 server.py
# Access at http://<pi-ip>:8080  (port override: OAP_WEB_PORT)
```

Or via the systemd service created by `install.sh`:
```bash
sudo systemctl start openauto-prodigy-web
```

## Running Tests

```bash
cd build && ctest --output-on-failure
```

**ctest does NOT compile the app target** — always build `openauto-prodigy`
explicitly (`cmake --build . --target openauto-prodigy`) before claiming
green; a broken `main.cpp` is invisible to the test suite.

See `tests/README.md` for suite layout and single-test invocation.

## Qt / QML Gotchas

Code-level traps are documented next to the code — read the nearest agent
file before touching a subsystem: `src/AGENTS.md` (Qt, D-Bus, PipeWire),
`src/core/aa/AGENTS.md` (AA protocol), `qml/AGENTS.md` (UI). Build-system
gotchas not covered there:

### QML Resource Paths

QML loads via `engine.load()` with `qrc:/OpenAutoProdigy/...` URLs — the
project pins this resource prefix in `qt_add_qml_module` (not the Qt 6.5+
default `qrc:/qt/qml/OpenAutoProdigy/`). Use the existing prefix for any new
QML component references.

### QML Subdirectory Types

Files in QML subdirectories (e.g., `qml/applications/home/HomeMenu.qml`) need the `QT_QML_SOURCE_TYPENAME` property set in CMake to flatten the type name:

```cmake
set_source_files_properties(qml/applications/home/HomeMenu.qml
    PROPERTIES QT_QML_SOURCE_TYPENAME HomeMenu)
```

### QSettings Forward Declaration

Don't forward-declare `class QSettings` inside a namespace — it creates `yournamespace::QSettings` instead of `::QSettings`. Always `#include <QSettings>`.

### QtQuick.Controls Runtime

The `qml6-module-qtqml-workerscript` package is required at runtime for QtQuick.Controls to load. Without it you'll get mysterious QML import failures.

## Project Structure

Top-level map only — the component-level view (plugins, services, threading,
data flow) is maintained in [architecture.md](architecture.md):

```
openauto-prodigy/
├── src/                        # C++ app — core services, AA protocol, plugins, UI controllers
├── qml/                        # QML UI (ships inside the binary — see qml/AGENTS.md)
├── libs/prodigy-oaa-protocol/  # AA protocol library (proto defs via git submodule)
├── proto/api/                  # External API v1 contract (frozen, additive-only)
├── web-config/                 # Flask web configuration panel
├── tests/                      # CTest + Qt Test suite (tests/README.md)
├── tools/                      # Dev and release tooling (tools/README.md)
├── scripts/                    # Repo scripts — review gate, link checker (scripts/README.md)
└── docs/                       # Documentation (docs/INDEX.md is the map)
```

### External API v1

TCP (`9810`) + WebSocket (`9811`) protobuf server for companion app, in-process web widgets, and third-party clients — implemented in `src/core/api/`. Design doc: `docs/archive/plans/2026-07-06-external-api-v1-design.md`. Config keys live under `api.*` in `config.yaml` (enable, ports, LAN exposure, pairing/queue timeouts).

## Debugging Tips

### BlueZ D-Bus Debugging

```bash
# List all BlueZ managed objects
busctl call org.bluez / org.freedesktop.DBus.ObjectManager GetManagedObjects

# Monitor D-Bus signals
dbus-monitor --system "sender='org.bluez'"

# Check paired devices
bluetoothctl devices
```

### Phone Behavior

Phone state can degrade after rapid connect/disconnect cycles during development. If the phone stops responding normally:
1. Clear Android Auto app cache/data on phone
2. Reboot the phone
3. Re-pair Bluetooth

## Contributing

See [CONTRIBUTING.md](../CONTRIBUTING.md) for the branch flow and PR checklist, and [debugging-notes.md](how-to/debugging-notes.md) for known issues and discoveries. The most helpful thing right now is testing with different phones and reporting connection behavior.
